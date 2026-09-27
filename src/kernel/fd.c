#include "fd.h"
#include "process.h"
#include "scheduler.h"
#include "user.h"
#include "../arch/riscv/trap.h"
#include "../fs/vfs.h"

/* fd slots own references to open descriptions. A blocking operation takes an
 * extra reference: the backend may switch threads, but its file stays alive.
 * Syscalls run with interrupts disabled; one user thread exists per process. */
static struct file *lookup(struct process *p, uint64_t fd)
{
    return fd < FD_COUNT ? p->fds[fd] : 0;
}

static int empty_slot(struct process *p)
{
    for (int i = 0; i < FD_COUNT; ++i)
        if (!p->fds[i]) return i;
    return -E_MFILE;
}

static int path_copy(char *path, uint64_t address)
{
    for (unsigned i = 0; i < PATH_LIMIT; ++i) {
        if (address > UINT64_MAX - i ||
            !copy_from_user(path + i, address + i, 1)) return -E_FAULT;
        if (!path[i]) return 0;
    }
    return -E_NAMETOOLONG;
}

int fd_install_stdio(struct process *p)
{
    for (int i = 0; i < 3; ++i) {
        int error = vfs_openat(p->root, p->root, "/dev/console",
                              i ? O_WRONLY : O_RDONLY, &p->fds[i]);
        if (error) return error; /* process_destroy owns any installed prefix. */
    }
    return 0;
}

static int64_t transfer(struct process *p, uint64_t fd, uint64_t address,
                        uint64_t size, bool writing)
{
    struct file *file = lookup(p, fd);
    unsigned char buffer[256];
    uint64_t done = 0;
    int64_t n = 0;
    if (!file || (writing ? (file->flags & O_ACCMODE) == O_RDONLY :
                            (file->flags & O_ACCMODE) == O_WRONLY))
        return -E_BADF;
    /* Validate the entire range before consuming pipe bytes or changing an
     * offset. No other thread can unmap this process's memory while we sleep. */
    if (size > INT64_MAX ||
        !address_space_validate(&p->as, address, size, !writing))
        return -E_FAULT;
    file_get(file);
    while (done < size) {
        size_t chunk = size - done;
        if (chunk > sizeof(buffer)) chunk = sizeof(buffer);
        if (writing) {
            copy_from_user(buffer, address + done, chunk);
            n = file_write(file, buffer, chunk);
        } else {
            n = file_read(file, buffer, chunk);
            if (n > 0) copy_to_user(address + done, buffer, (size_t)n);
        }
        if (n <= 0) break;
        done += (uint64_t)n;
        /* A short read is observable immediately, especially for pipes. */
        if ((size_t)n < chunk || (!writing && file->ops->read)) break;
    }
    file_put(file);
    return done ? (int64_t)done : n;
}

int64_t fd_syscall(struct trap_frame *f)
{
    struct process *p = current_process();
    uint64_t a = f->x[10], b = f->x[11], c = f->x[12];
    struct file *file = lookup(p, a);
    char path[PATH_LIMIT];
    int error, slot;
    switch (f->x[17]) {
    case SYS_READ: return transfer(p, a, b, c, false);
    case SYS_WRITE: return transfer(p, a, b, c, true);
    case SYS_CLOSE:
        if (!file) return -E_BADF;
        p->fds[a] = 0;
        file_put(file);
        return 0;
    case SYS_DUP:
    case SYS_DUP2:
        if (!file) return -E_BADF;
        if (f->x[17] == SYS_DUP) slot = empty_slot(p);
        else slot = b < FD_COUNT ? (int)b : -E_BADF;
        if (slot < 0) return slot;
        if ((uint64_t)slot == a) return slot;
        file_get(file);
        file_put(p->fds[slot]);
        p->fds[slot] = file;
        return slot;
    case SYS_OPENAT: {
        struct vnode *base = p->cwd;
        if ((error = path_copy(path, b))) return error;
        if (path[0] != '/' && (int64_t)a != AT_FDCWD) {
            if (!file) return -E_BADF;
            if (!file->node || file->node->type != DT_DIR) return -E_NOTDIR;
            base = file->node;
        }
        /* Check capacity before O_TRUNC/O_CREAT can mutate the namespace. */
        if ((slot = empty_slot(p)) < 0) return slot;
        if (c > UINT32_MAX) return -E_INVAL;
        error = vfs_openat(p->root, base, path, (unsigned)c, &file);
        if (error) return error;
        p->fds[slot] = file;
        return slot;
    }
    case SYS_PIPE: {
        int32_t pair[2];
        struct file *reader, *writer;
        if (!address_space_validate(&p->as, a, sizeof(pair), true))
            return -E_FAULT;
        pair[0] = empty_slot(p);
        if (pair[0] < 0) return pair[0];
        pair[1] = -1;
        for (int i = pair[0] + 1; i < FD_COUNT; ++i)
            if (!p->fds[i]) { pair[1] = i; break; }
        if (pair[1] < 0) return -E_MFILE;
        if ((error = pipe_create(&reader, &writer))) return error;
        p->fds[pair[0]] = reader;
        p->fds[pair[1]] = writer;
        copy_to_user(a, pair, sizeof(pair));
        return 0;
    }
    case SYS_GETDENTS: {
        struct directory_entry entry;
        uint64_t done = 0;
        if (!file) return -E_BADF;
        if (c < sizeof(entry)) return -E_INVAL;
        if (c > INT64_MAX || !address_space_validate(&p->as, b, c, true))
            return -E_FAULT;
        while (c - done >= sizeof(entry)) {
            int64_t n = file_getdents(file, &entry, sizeof(entry));
            if (n <= 0) return done ? (int64_t)done : n;
            copy_to_user(b + done, &entry, sizeof(entry));
            done += sizeof(entry);
        }
        return (int64_t)done;
    }
    case SYS_CHDIR: {
        struct vnode *node;
        if ((error = path_copy(path, a))) return error;
        error = vfs_resolve(p->root, p->cwd, path, false, &node);
        if (error) return error;
        if (node->type != DT_DIR) return -E_NOTDIR;
        p->cwd = node;
        return 0;
    }
    case SYS_GETCWD:
        error = vfs_getcwd(p->root, p->cwd, path, sizeof(path));
        if (error < 0) return error;
        if (b < (uint64_t)error) return -E_INVAL;
        return copy_to_user(a, path, (size_t)error) ? error : -E_FAULT;
    case SYS_LSEEK:
        return !file ? -E_BADF : c > UINT32_MAX ? -E_INVAL :
            file_seek(file, (int64_t)b, (unsigned)c);
    default: return -E_NOSYS;
    }
}
