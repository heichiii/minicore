#ifndef MINICORE_FS_VFS_H
#define MINICORE_FS_VFS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "../abi.h"
#include "../kernel/list.h"

struct file;
struct vnode;

/* Every backend consumes kernel buffers. User-pointer validation belongs to
 * the syscall boundary, so filesystems and devices cannot accidentally bypass
 * copy_{from,to}_user. Pipe callbacks may sleep; no VFS lock is held then. */
struct file_ops {
    int64_t (*pread)(struct file *, void *, size_t, uint64_t);
    int64_t (*read)(struct file *, void *, size_t);
    int64_t (*write)(struct file *, const void *, size_t);
    void (*release)(struct file *);
};

/* Mounts own namespace nodes until shutdown. M6 has no unlink/unmount, hence
 * cwd/root and open-file vnode pointers can borrow this stable lifetime. */
struct mount {
    struct vnode *root;
    struct vnode *covered;
    bool readonly;
};

#define TMP_FILE_PAGES 16U
struct vnode {
    struct list_node all_link;
    struct list_node sibling;
    struct list_node children;
    struct vnode *parent;
    struct mount *mount;
    struct mount *mounted;
    char name[NAME_LIMIT];
    uint32_t type;
    uint64_t inode;
    const unsigned char *data; /* Immutable archive payload, including symlinks. */
    size_t size;
    uint64_t blocks[TMP_FILE_PAGES]; /* tmpfs-owned physical pages. */
};

/* An open file description. Each fd and temporary blocking I/O owns one ref.
 * dup/fork share this object (including offset); independent opens do not.
 * Reference/offset changes are serialized by local IRQ exclusion on one hart. */
struct file {
    const struct file_ops *ops;
    struct vnode *node;
    void *private;
    uint64_t offset;
    size_t references;
    unsigned flags;
};

bool vfs_init(const void *archive, size_t size);
void vfs_shutdown(void); /* Only after all processes and open files are gone. */
struct vnode *vfs_root(void);
int vfs_resolve(struct vnode *root, struct vnode *base, const char *path,
                bool nofollow, struct vnode **result);
int vfs_openat(struct vnode *root, struct vnode *base, const char *path,
               unsigned flags, struct file **result);
int vfs_getcwd(struct vnode *root, struct vnode *cwd, char *buffer, size_t size);
struct file *vfs_open(const char *path); /* Kernel loader convenience. */
int64_t file_pread(struct file *, void *, size_t, uint64_t);
int64_t file_read(struct file *, void *, size_t);
int64_t file_write(struct file *, const void *, size_t);
int64_t file_seek(struct file *, int64_t, unsigned);
int64_t file_getdents(struct file *, struct directory_entry *, size_t);
size_t file_size(const struct file *);
struct file *file_new(const struct file_ops *ops, unsigned flags);
void file_get(struct file *);
void file_put(struct file *);
size_t file_live_count(void);

int pipe_create(struct file **reader, struct file **writer);
#endif
