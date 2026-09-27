#include "vfs.h"
#include "../kernel/sync.h"
#include "../kernel/scheduler.h"
#include "../mm/heap.h"
#include "../mm/page_alloc.h"
#include "../mm/layout.h"
#include "../platform/platform.h"
#include "../runtime.h"

static LIST_HEAD(nodes);
static struct mount mounts[3];
static uint64_t next_inode;
static size_t live_files;

static bool equal(const char *a, const char *b)
{
    size_t n = strlen(a);
    return n == strlen(b) && memcmp(a, b, n) == 0;
}

struct vnode *vfs_root(void) { return mounts[0].root; }
size_t file_live_count(void) { return live_files; }

static struct vnode *child(struct vnode *parent, const char *name)
{
    for (struct list_node *e = parent->children.next;
         e != &parent->children; e = e->next) {
        struct vnode *n = container_of(e, struct vnode, sibling);
        if (equal(n->name, name))
            return n;
    }
    return 0;
}

static struct vnode *new_node(struct vnode *parent, const char *name,
                               uint32_t type, struct mount *mount)
{
    struct vnode *n = kmalloc(sizeof(*n));
    if (!n)
        return 0;
    memcpy(n->name, name, strlen(name) + 1);
    n->parent = parent ? parent : n;
    n->mount = mount;
    n->type = type;
    n->inode = next_inode++;
    list_init(&n->children);
    list_init(&n->sibling);
    if (parent)
        list_push_back(&parent->children, &n->sibling);
    list_push_back(&nodes, &n->all_link);
    return n;
}

/* Walk components, crossing mounts as we descend. At a mounted root, '..'
 * crosses the covered directory, but can never escape process->root.
 * Symlinks are expanded iteratively into bounded scratch buffers; this avoids
 * recursive kernel-stack growth and enforces an eight-link traversal limit. */
int vfs_resolve(struct vnode *root, struct vnode *base, const char *path,
                bool nofollow, struct vnode **result)
{
    char work[PATH_LIMIT], expanded[PATH_LIMIT], name[NAME_LIMIT];
    struct vnode *node;
    size_t cursor = 0;
    unsigned links = 0;
    if (!path || !*path)
        return -E_NOENT;
    if (strlen(path) >= sizeof(work))
        return -E_NAMETOOLONG;
    memcpy(work, path, strlen(path) + 1);
    node = *path == '/' ? root : base;
    if (!node)
        return -E_NOENT;
    for (;;) {
        bool slash = work[cursor] == '/';
        while (work[cursor] == '/')
            ++cursor;
        if (!work[cursor]) {
            if (slash && node->type != DT_DIR)
                return -E_NOTDIR;
            *result = node;
            return 0;
        }
        if (node->type != DT_DIR)
            return -E_NOTDIR;
        size_t start = cursor;
        while (work[cursor] && work[cursor] != '/')
            ++cursor;
        size_t length = cursor - start;
        if (length >= sizeof(name))
            return -E_NAMETOOLONG;
        memcpy(name, work + start, length);
        name[length] = 0;
        if (equal(name, "."))
            continue;
        if (equal(name, "..")) {
            if (node != root)
                node = node->mount->root == node && node->mount->covered
                           ? node->mount->covered->parent : node->parent;
            continue;
        }
        struct vnode *next = child(node, name);
        if (!next)
            return -E_NOENT;
        if (next->mounted)
            next = next->mounted->root;
        if (next->type == DT_LNK) {
            if (nofollow && !work[cursor])
                return -E_LOOP;
            if (++links > 8)
                return -E_LOOP;
            size_t rest = strlen(work + cursor);
            if (!next->size || next->size + rest >= sizeof(expanded))
                return -E_NAMETOOLONG;
            memcpy(expanded, next->data, next->size);
            memcpy(expanded + next->size, work + cursor, rest + 1);
            memcpy(work, expanded, next->size + rest + 1);
            if (work[0] == '/')
                node = root;
            cursor = 0;
        } else {
            node = next;
        }
    }
}

int vfs_getcwd(struct vnode *root, struct vnode *cwd, char *buffer, size_t size)
{
    char reverse[PATH_LIMIT];
    size_t at = sizeof(reverse) - 1;
    reverse[at] = 0;
    while (cwd != root) {
        if (cwd->mount->root == cwd && cwd->mount->covered)
            cwd = cwd->mount->covered;
        size_t n = strlen(cwd->name);
        if (n + 1 > at)
            return -E_NAMETOOLONG;
        at -= n;
        memcpy(reverse + at, cwd->name, n);
        reverse[--at] = '/';
        cwd = cwd->parent;
    }
    if (at == sizeof(reverse) - 1)
        reverse[--at] = '/';
    size_t length = sizeof(reverse) - at;
    if (length > size)
        return -E_INVAL;
    memcpy(buffer, reverse + at, length);
    return (int)length;
}

static bool hex8(const unsigned char *p, uint32_t *value)
{
    *value = 0;
    for (unsigned i = 0; i < 8; ++i) {
        unsigned c = p[i], v;
        if (c >= '0' && c <= '9') v = c - '0';
        else if (c >= 'a' && c <= 'f') v = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') v = c - 'A' + 10;
        else return false;
        *value = (*value << 4) | v;
    }
    return true;
}

/* Archive entries must be canonical relative names, parents before children.
 * Payloads remain borrowed from .rodata; only namespace objects are allocated.
 * A failed parse tears down every created node before reporting failure. */
static bool archive_node(const char *path, const unsigned char *data,
                          size_t size, uint32_t mode)
{
    if (equal(path, "."))
        return (mode & 0170000) == 0040000;
    char name[NAME_LIMIT];
    struct vnode *parent = vfs_root();
    if (!*path || *path == '/') return false;
    for (;;) {
        size_t n = 0;
        while (path[n] && path[n] != '/') ++n;
        if (!n || n >= sizeof(name)) return false;
        memcpy(name, path, n); name[n] = 0;
        if (equal(name, ".") || equal(name, "..")) return false;
        if (path[n] == '/') {
            parent = child(parent, name);
            if (!parent || parent->type != DT_DIR) return false;
            path += n + 1;
            continue;
        }
        if (child(parent, name)) return false;
        uint32_t type = (mode & 0170000) == 0040000 ? DT_DIR :
                        (mode & 0170000) == 0100000 ? DT_REG :
                        (mode & 0170000) == 0120000 ? DT_LNK : 0;
        if (!type || (type == DT_DIR && size)) return false;
        if (type == DT_LNK) {
            if (!size || size >= PATH_LIMIT) return false;
            for (size_t i = 0; i < size; ++i) if (!data[i]) return false;
        }
        struct vnode *node = new_node(parent, name, type, &mounts[0]);
        if (!node) return false;
        node->data = data; node->size = size;
        return true;
    }
}

void vfs_shutdown(void)
{
    /* Namespace lifetime ends only when no fd/cwd can reference it. */
    while (!list_empty(&nodes)) {
        struct vnode *node = container_of(nodes.next, struct vnode, all_link);
        list_remove(&node->all_link);
        for (unsigned i = 0; i < TMP_FILE_PAGES; ++i)
            if (node->blocks[i]) page_free(node->blocks[i]);
        kfree(node);
    }
    memset(mounts, 0, sizeof(mounts));
}

bool vfs_init(const void *archive, size_t size)
{
    const unsigned char *bytes = archive;
    size_t offset = 0;
    bool trailer = false;
    if (!list_empty(&nodes) || !archive) return false;
    next_inode = 1;
    mounts[0].readonly = true;
    mounts[0].root = new_node(0, "", DT_DIR, &mounts[0]);
    if (!mounts[0].root) return false;
    while (offset <= size && size - offset >= 110) {
        const unsigned char *header = bytes + offset;
        uint32_t mode, payload, namesize;
        if (memcmp(header, "070701", 6) || !hex8(header + 14, &mode) ||
            !hex8(header + 54, &payload) || !hex8(header + 94, &namesize) ||
            !namesize || namesize > PATH_LIMIT) goto fail;
        size_t nameoff = offset + 110;
        if (namesize > size - nameoff || bytes[nameoff + namesize - 1]) goto fail;
        const char *name = (const char *)bytes + nameoff;
        if (strlen(name) != namesize - 1) goto fail;
        size_t dataoff = (nameoff + namesize + 3) & ~(size_t)3;
        if (dataoff > size || payload > size - dataoff) goto fail;
        offset = (dataoff + payload + 3) & ~(size_t)3;
        if (offset > size) goto fail;
        if (equal(name, "TRAILER!!!")) { trailer = payload == 0; break; }
        if (!archive_node(name, bytes + dataoff, payload, mode)) goto fail;
    }
    if (!trailer) goto fail;
    /* Two boot mounts. There is intentionally no mutable mount syscall yet. */
    const char *names[] = {"tmp", "dev"};
    for (unsigned i = 1; i < 3; ++i) {
        struct vnode *covered = child(vfs_root(), names[i - 1]);
        if (!covered) covered = new_node(vfs_root(), names[i - 1], DT_DIR, &mounts[0]);
        if (!covered || covered->type != DT_DIR) goto fail;
        mounts[i].covered = covered;
        mounts[i].readonly = i == 2;
        mounts[i].root = new_node(0, "", DT_DIR, &mounts[i]);
        if (!mounts[i].root) goto fail;
        covered->mounted = &mounts[i];
    }
    if (!new_node(mounts[2].root, "console", DT_CHR, &mounts[2]) ||
        !new_node(mounts[2].root, "null", DT_CHR, &mounts[2])) goto fail;
    return true;
fail:
    vfs_shutdown();
    return false;
}

/* All regular-file accesses below execute under IRQ exclusion. No backend
 * here sleeps, so a shared offset (dup/fork) advances atomically per call. */
static int64_t regular_pread(struct file *f, void *buffer, size_t size, uint64_t off)
{
    struct vnode *n = f->node;
    if (n->type == DT_DIR) return -E_ISDIR;
    if (off >= n->size) return 0;
    if (size > n->size - off) size = n->size - off;
    if (n->data) { memcpy(buffer, n->data + off, size); return (int64_t)size; }
    size_t done = 0;
    while (done < size) {
        size_t part = PAGE_SIZE - (off & PAGE_MASK);
        if (part > size - done) part = size - done;
        uint64_t page = n->blocks[off / PAGE_SIZE];
        if (page) memcpy((char *)buffer + done, (char *)phys_to_virt(page) + (off & PAGE_MASK), part);
        else memset((char *)buffer + done, 0, part);
        done += part; off += part;
    }
    return (int64_t)done;
}

static int64_t regular_write(struct file *f, const void *buffer, size_t size)
{
    struct vnode *n = f->node;
    if (n->mount->readonly) return -E_ROFS;
    if (n->type == DT_DIR) return -E_ISDIR;
    /* Resolve append at write time, not open time: independent append opens
     * must see the latest end. IRQ exclusion covers selection and mutation. */
    if (f->flags & O_APPEND) f->offset = n->size;
    if (f->offset >= TMP_FILE_PAGES * PAGE_SIZE) return size ? -E_FBIG : 0;
    size_t done = 0;
    while (done < size && f->offset < TMP_FILE_PAGES * PAGE_SIZE) {
        size_t index = f->offset / PAGE_SIZE;
        size_t part = PAGE_SIZE - (f->offset & PAGE_MASK);
        if (part > size - done) part = size - done;
        if (!n->blocks[index]) {
            n->blocks[index] = page_alloc();
            if (!n->blocks[index]) return done ? (int64_t)done : -E_NOMEM;
            memset(phys_to_virt(n->blocks[index]), 0, PAGE_SIZE);
        }
        memcpy((char *)phys_to_virt(n->blocks[index]) + (f->offset & PAGE_MASK),
               (const char *)buffer + done, part);
        done += part; f->offset += part;
        if (f->offset > n->size) n->size = f->offset;
    }
    return (int64_t)done;
}

static int64_t device_read(struct file *f, void *buffer, size_t size)
{
    if (equal(f->node->name, "null") || !size) return 0;
    /* Poll once per scheduler tick when UART has no input. This is a sleeping
     * console fallback until interrupt-driven terminal input is introduced. */
    size_t done = 0;
    while (done < size) {
        int c = console_getc();
        if (c < 0) {
            if (done) break;
            scheduler_sleep(1);
        } else ((char *)buffer)[done++] = (char)c;
    }
    return (int64_t)done;
}

static int64_t device_write(struct file *f, const void *buffer, size_t size)
{
    if (!equal(f->node->name, "null"))
        for (size_t i = 0; i < size; ++i) console_putc(((const char *)buffer)[i]);
    return (int64_t)size;
}

static const struct file_ops regular_ops = {.pread = regular_pread, .write = regular_write};
static const struct file_ops device_ops = {.read = device_read, .write = device_write};

struct file *file_new(const struct file_ops *ops, unsigned flags)
{
    struct file *f = kmalloc(sizeof(*f));
    if (!f) return 0;
    f->ops = ops; f->flags = flags; f->references = 1;
    uint64_t irq = irq_save(); ++live_files; irq_restore(irq);
    return f;
}

void file_get(struct file *f)
{
    if (!f) return;
    uint64_t irq = irq_save(); ++f->references; irq_restore(irq);
}

void file_put(struct file *f)
{
    if (!f) return;
    uint64_t irq = irq_save();
    if (--f->references == 0) {
        /* Pipe release wakes blocked peers and frees storage only after BOTH
         * endpoint descriptions lose their last fd/temporary reference. */
        if (f->ops->release) f->ops->release(f);
        --live_files;
        kfree(f);
    }
    irq_restore(irq);
}

int vfs_openat(struct vnode *root, struct vnode *base, const char *path,
               unsigned flags, struct file **result)
{
    const unsigned allowed = O_ACCMODE | O_CREAT | O_EXCL | O_TRUNC |
                             O_APPEND | O_DIRECTORY | O_NOFOLLOW;
    if ((flags & ~allowed) || (flags & O_ACCMODE) == 3 ||
        ((flags & O_TRUNC) && !(flags & O_ACCMODE))) return -E_INVAL;
    uint64_t irq = irq_save();
    struct vnode *n = 0;
    int error = vfs_resolve(root, base, path, flags & O_NOFOLLOW, &n);
    if (!error && (flags & (O_CREAT | O_EXCL)) == (O_CREAT | O_EXCL)) error = -E_EXIST;
    bool created = false;
    if (error == -E_NOENT && (flags & O_CREAT)) {
        char parent_path[PATH_LIMIT];
        size_t length = strlen(path), cut = length;
        if (!length || length >= PATH_LIMIT || path[length - 1] == '/') { error = -E_INVAL; goto out; }
        while (cut && path[cut - 1] != '/') --cut;
        if (length - cut >= NAME_LIMIT) { error = -E_NAMETOOLONG; goto out; }
        memcpy(parent_path, path, cut); parent_path[cut] = 0;
        struct vnode *parent = base;
        error = cut ? vfs_resolve(root, base, parent_path, false, &parent) : 0;
        if (error) goto out;
        if (parent->type != DT_DIR) { error = -E_NOTDIR; goto out; }
        if (parent->mount->readonly) { error = -E_ROFS; goto out; }
        if (child(parent, path + cut)) { error = -E_NOENT; goto out; }
        n = new_node(parent, path + cut, DT_REG, parent->mount);
        if (!n) { error = -E_NOMEM; goto out; }
        created = true; error = 0;
    }
    if (error) goto out;
    if ((flags & O_DIRECTORY) && n->type != DT_DIR) error = -E_NOTDIR;
    else if ((flags & O_ACCMODE) && n->type == DT_DIR) error = -E_ISDIR;
    else if ((flags & O_ACCMODE) && n->type == DT_REG && n->mount->readonly) error = -E_ROFS;
    if (error) goto rollback;
    struct file *f = file_new(n->type == DT_CHR ? &device_ops : &regular_ops, flags);
    if (!f) { error = -E_NOMEM; goto rollback; }
    f->node = n;
    /* Delay truncation until the open description exists. Allocation failure
     * must not destroy existing data; a newly created vnode rolls back below. */
    if (flags & O_TRUNC) {
        for (unsigned i = 0; i < TMP_FILE_PAGES; ++i) {
            if (n->blocks[i]) page_free(n->blocks[i]);
            n->blocks[i] = 0;
        }
        n->size = 0;
    }
    *result = f;
    goto out;
rollback:
    if (created) { list_remove(&n->all_link); list_remove(&n->sibling); kfree(n); }
out:
    irq_restore(irq);
    return error;
}

struct file *vfs_open(const char *path)
{
    struct file *f = 0;
    vfs_openat(vfs_root(), vfs_root(), path, O_RDONLY, &f);
    return f;
}

size_t file_size(const struct file *f) { return f && f->node ? f->node->size : 0; }

int64_t file_pread(struct file *f, void *buffer, size_t size, uint64_t offset)
{
    if (!f || (f->flags & O_ACCMODE) == O_WRONLY) return -E_BADF;
    if (!f->ops->pread) return -E_SPIPE;
    uint64_t irq = irq_save();
    int64_t result = f->ops->pread(f, buffer, size, offset);
    irq_restore(irq);
    return result;
}

int64_t file_read(struct file *f, void *buffer, size_t size)
{
    if (!f || (f->flags & O_ACCMODE) == O_WRONLY) return -E_BADF;
    uint64_t irq = irq_save();
    int64_t result;
    if (f->ops->read) result = f->ops->read(f, buffer, size);
    else if (f->ops->pread) {
        result = f->ops->pread(f, buffer, size, f->offset);
        if (result > 0) f->offset += (uint64_t)result;
    } else result = -E_BADF;
    irq_restore(irq);
    return result;
}

int64_t file_write(struct file *f, const void *buffer, size_t size)
{
    if (!f || !(f->flags & O_ACCMODE) || !f->ops->write) return -E_BADF;
    uint64_t irq = irq_save();
    int64_t result = f->ops->write(f, buffer, size);
    irq_restore(irq);
    return result;
}

int64_t file_seek(struct file *f, int64_t offset, unsigned whence)
{
    if (!f->node || f->node->type != DT_REG) return -E_SPIPE;
    uint64_t irq = irq_save();
    int64_t base = whence == SEEK_SET ? 0 : whence == SEEK_CUR ?
                   (int64_t)f->offset : (int64_t)file_size(f);
    if (whence > SEEK_END || offset < -base || offset > INT64_MAX - base) {
        irq_restore(irq); return -E_INVAL;
    }
    f->offset = (uint64_t)(base + offset);
    irq_restore(irq);
    return base + offset;
}

int64_t file_getdents(struct file *f, struct directory_entry *out, size_t size)
{
    if (!f->node || f->node->type != DT_DIR) return -E_NOTDIR;
    if (size < sizeof(*out)) return -E_INVAL;
    uint64_t irq = irq_save();
    size_t index = 0, count = 0;
    for (struct list_node *e = f->node->children.next;
         e != &f->node->children && count < size / sizeof(*out); e = e->next) {
        if (index++ < f->offset) continue;
        struct vnode *n = container_of(e, struct vnode, sibling);
        memset(&out[count], 0, sizeof(*out));
        out[count].inode = n->inode; out[count].type = n->type;
        memcpy(out[count].name, n->name, strlen(n->name) + 1);
        ++count;
    }
    f->offset += count;
    irq_restore(irq);
    return (int64_t)(count * sizeof(*out));
}
