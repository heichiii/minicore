#ifndef MINICORE_FS_VFS_H
#define MINICORE_FS_VFS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../kernel/list.h"

struct file;

struct file_ops {
    int64_t (*pread)(struct file *file, void *buffer, size_t size,
                     uint64_t offset);
    int64_t (*write)(struct file *file, uint64_t user_buffer, size_t size);
};

/* Archive-backed namespace object; path and data borrow immutable archive bytes. */
struct vnode {
    struct list_node link;
    const char *path;
    const unsigned char *data;
    size_t size;
    uint32_t mode;
};

struct file {
    const struct file_ops *ops;
    struct vnode *node;
    size_t references;
    bool permanent;
};

/* Parse and mount one immutable newc archive as the root namespace. */
bool vfs_init(const void *archive, size_t size);
struct vnode *vfs_lookup(const char *path);
struct file *vfs_open(const char *path);
int64_t file_pread(struct file *file, void *buffer, size_t size,
                   uint64_t offset);
size_t file_size(const struct file *file);
void file_get(struct file *file);
void file_put(struct file *file);

#endif
