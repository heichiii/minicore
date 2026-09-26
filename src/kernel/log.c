#include "log.h"

#include "sync.h"

#define LOG_CAPACITY 1024U

static char buffer[LOG_CAPACITY];
static size_t head;
static size_t count;
static struct spinlock lock;

void log_init(void)
{
    head = 0;
    count = 0;
    spinlock_init(&lock);
}

void log_write(const char *data, size_t size)
{
    uint64_t flags = irq_save();

    spin_lock(&lock);
    for (size_t i = 0; i < size; ++i) {
        buffer[head] = data[i];
        head = (head + 1) % LOG_CAPACITY;
        if (count < LOG_CAPACITY)
            ++count;
    }
    spin_unlock(&lock);
    irq_restore(flags);
}

size_t log_read(char *data, size_t capacity)
{
    uint64_t flags = irq_save();
    size_t amount;
    size_t start;

    spin_lock(&lock);
    amount = count < capacity ? count : capacity;
    start = (head + LOG_CAPACITY - count) % LOG_CAPACITY;
    for (size_t i = 0; i < amount; ++i)
        data[i] = buffer[(start + i) % LOG_CAPACITY];
    spin_unlock(&lock);
    irq_restore(flags);
    return amount;
}
