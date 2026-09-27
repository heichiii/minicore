#include "vfs.h"

#include "../mm/heap.h"
#include "../runtime.h"

#define CPIO_HEADER_SIZE 110U
#define CPIO_MODE_TYPE UINT32_C(0170000)
#define CPIO_MODE_REG UINT32_C(0100000)

static LIST_HEAD(nodes);

static size_t align4(size_t value)
{
    return (value + 3U) & ~(size_t)3U;
}

static bool parse_hex8(const unsigned char *text, uint32_t *value)
{
    uint32_t result = 0;

    for (size_t i = 0; i < 8; ++i) {
        unsigned digit;

        if (text[i] >= '0' && text[i] <= '9')
            digit = text[i] - '0';
        else if (text[i] >= 'a' && text[i] <= 'f')
            digit = text[i] - 'a' + 10;
        else if (text[i] >= 'A' && text[i] <= 'F')
            digit = text[i] - 'A' + 10;
        else
            return false;
        result = (result << 4) | digit;
    }
    *value = result;
    return true;
}

static void clear_nodes(void)
{
    while (!list_empty(&nodes)) {
        struct vnode *node = container_of(nodes.next, struct vnode, link);

        list_remove(&node->link);
        kfree(node);
    }
}

static bool valid_path(const char *path)
{
    const char *component = path;

    if (!path || !*path || *path == '/')
        return false;
    for (const char *cursor = path;; ++cursor) {
        if (*cursor == '/' || *cursor == 0) {
            size_t length = (size_t)(cursor - component);

            if (length == 0 ||
                (length == 1 && component[0] == '.') ||
                (length == 2 && component[0] == '.' && component[1] == '.'))
                return false;
            if (!*cursor)
                return true;
            component = cursor + 1;
        }
    }
}

static bool duplicate_path(const char *path)
{
    size_t length = strlen(path);

    for (struct list_node *entry = nodes.next; entry != &nodes;
         entry = entry->next) {
        struct vnode *node = container_of(entry, struct vnode, link);

        if (strlen(node->path) == length &&
            memcmp(node->path, path, length) == 0)
            return true;
    }
    return false;
}

bool vfs_init(const void *archive, size_t size)
{
    const unsigned char *bytes = archive;
    size_t offset = 0;

    clear_nodes();
    while (offset <= size && size - offset >= CPIO_HEADER_SIZE) {
        const unsigned char *header = bytes + offset;
        uint32_t mode, file_size_value, name_size;
        size_t name_offset, data_offset, next;
        struct vnode *node;

        if (memcmp(header, "070701", 6) != 0 ||
            !parse_hex8(header + 14, &mode) ||
            !parse_hex8(header + 54, &file_size_value) ||
            !parse_hex8(header + 94, &name_size) || name_size == 0)
            goto fail;
        name_offset = offset + CPIO_HEADER_SIZE;
        if (name_offset > size || name_size > size - name_offset ||
            bytes[name_offset + name_size - 1] != 0)
            goto fail;
        data_offset = align4(name_offset + name_size);
        if (data_offset > size || file_size_value > size - data_offset)
            goto fail;
        next = align4(data_offset + file_size_value);
        if (next > size)
            goto fail;
        if (name_size == sizeof("TRAILER!!!") &&
            memcmp(bytes + name_offset, "TRAILER!!!", name_size) == 0)
            return true;

        const char *path = (const char *)bytes + name_offset;
        if (strlen(path) + 1 != name_size ||
            (memcmp(path, ".", 2) != 0 && !valid_path(path)) ||
            duplicate_path(path))
            goto fail;

        node = kmalloc(sizeof(*node));
        if (!node)
            goto fail;
        node->path = (const char *)bytes + name_offset;
        node->data = bytes + data_offset;
        node->size = file_size_value;
        node->mode = mode;
        list_push_back(&nodes, &node->link);
        offset = next;
    }
fail:
    clear_nodes();
    return false;
}

static const char *relative_path(const char *path)
{
    if (!path || path[0] != '/')
        return 0;
    while (*path == '/')
        ++path;
    if (!*path)
        return ".";
    return path;
}

struct vnode *vfs_lookup(const char *path)
{
    const char *relative = relative_path(path);
    size_t length;

    if (!relative)
        return 0;
    length = strlen(relative);
    if ((memcmp(relative, ".", 2) != 0 && !valid_path(relative)))
        return 0;
    for (struct list_node *entry = nodes.next; entry != &nodes;
         entry = entry->next) {
        struct vnode *node = container_of(entry, struct vnode, link);

        if (strlen(node->path) == length &&
            memcmp(node->path, relative, length) == 0)
            return node;
    }
    return 0;
}

static int64_t initramfs_pread(struct file *file, void *buffer, size_t size,
                               uint64_t offset)
{
    if (!buffer || offset > file->node->size)
        return -1;
    if (size > file->node->size - (size_t)offset)
        size = file->node->size - (size_t)offset;
    memcpy(buffer, file->node->data + offset, size);
    return (int64_t)size;
}

static const struct file_ops initramfs_ops = { .pread = initramfs_pread };

struct file *vfs_open(const char *path)
{
    struct vnode *node = vfs_lookup(path);
    struct file *file;

    if (!node || (node->mode & CPIO_MODE_TYPE) != CPIO_MODE_REG)
        return 0;
    file = kmalloc(sizeof(*file));
    if (!file)
        return 0;
    file->ops = &initramfs_ops;
    file->node = node;
    file->references = 1;
    file->permanent = false;
    return file;
}

int64_t file_pread(struct file *file, void *buffer, size_t size,
                   uint64_t offset)
{
    if (!file || !file->ops || !file->ops->pread)
        return -1;
    return file->ops->pread(file, buffer, size, offset);
}

size_t file_size(const struct file *file)
{
    return file && file->node ? file->node->size : 0;
}

void file_get(struct file *file)
{
    if (file && !file->permanent)
        ++file->references;
}

void file_put(struct file *file)
{
    if (file && !file->permanent && --file->references == 0)
        kfree(file);
}
