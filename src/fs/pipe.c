#include "vfs.h"
#include "../kernel/sync.h"
#include "../mm/heap.h"

#define PIPE_CAPACITY 512U

/* There are exactly two open descriptions, one per direction. Duplicating an
 * fd changes file.references, not readers/writers. Only final file_put closes
 * an endpoint, which is essential for EOF and EPIPE to survive dup and fork. */
struct pipe {
    struct spinlock lock;
    struct wait_queue readable, writable;
    size_t head, used;
    bool reader, writer;
    unsigned char data[PIPE_CAPACITY];
};

static int64_t pipe_read(struct file *f, void *buffer, size_t size)
{
    struct pipe *p = f->private;
    if (!size) return 0;
    spin_lock(&p->lock);
    while (!p->used && p->writer)
        wait_queue_sleep(&p->readable, &p->lock);
    if (size > p->used) size = p->used;
    for (size_t i = 0; i < size; ++i) {
        ((unsigned char *)buffer)[i] = p->data[p->head];
        p->head = (p->head + 1) % PIPE_CAPACITY;
    }
    p->used -= size;
    wait_queue_wake_all(&p->writable);
    spin_unlock(&p->lock);
    return (int64_t)size; /* zero means all writers closed and buffer drained */
}

static int64_t pipe_write(struct file *f, const void *buffer, size_t size)
{
    struct pipe *p = f->private;
    if (!size) return 0;
    spin_lock(&p->lock);
    while (p->used == PIPE_CAPACITY && p->reader)
        wait_queue_sleep(&p->writable, &p->lock);
    if (!p->reader) { spin_unlock(&p->lock); return -E_PIPE; }
    if (size > PIPE_CAPACITY - p->used) size = PIPE_CAPACITY - p->used;
    for (size_t i = 0; i < size; ++i)
        p->data[(p->head + p->used + i) % PIPE_CAPACITY] = ((const unsigned char *)buffer)[i];
    p->used += size;
    wait_queue_wake_all(&p->readable);
    spin_unlock(&p->lock);
    return (int64_t)size;
}

static void pipe_release(struct file *f)
{
    struct pipe *p = f->private;
    spin_lock(&p->lock);
    if ((f->flags & O_ACCMODE) == O_RDONLY) p->reader = false;
    else p->writer = false;
    wait_queue_wake_all(&p->readable);
    wait_queue_wake_all(&p->writable);
    bool last = !p->reader && !p->writer;
    spin_unlock(&p->lock);
    if (last) kfree(p);
}

static const struct file_ops pipe_ops = {
    .read = pipe_read, .write = pipe_write, .release = pipe_release,
};

int pipe_create(struct file **reader, struct file **writer)
{
    struct pipe *p = kmalloc(sizeof(*p));
    if (!p) return -E_NOMEM;
    struct file *r = file_new(&pipe_ops, O_RDONLY);
    if (!r) { kfree(p); return -E_NOMEM; }
    p->reader = true;
    spinlock_init(&p->lock);
    wait_queue_init(&p->readable); wait_queue_init(&p->writable);
    r->private = p;
    struct file *w = file_new(&pipe_ops, O_WRONLY);
    if (!w) { file_put(r); return -E_NOMEM; }
    w->private = p; p->writer = true;
    *reader = r; *writer = w;
    return 0;
}
