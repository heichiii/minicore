#include "m4_test.h"

#include "list.h"
#include "log.h"
#include "refcount.h"
#include "scheduler.h"
#include "sync.h"
#include "../mm/heap.h"
#include "../mm/page_alloc.h"
#include "../runtime.h"
#include "../platform/platform.h"

static volatile bool register_results[2];
static volatile uint64_t preempt_a;
static volatile uint64_t preempt_b;
static volatile bool sleep_done;
static volatile uint64_t sleep_elapsed;

/* 测试用的四槽环形缓冲区。read 和 write 分别指向下一次读写位置，
 * count 区分下标重合时的满和空；锁同时保护缓冲区状态和等待条件。 */
struct pipe_test {
    struct spinlock lock;
    struct wait_queue readable;
    struct wait_queue writable;
    unsigned data[4];
    size_t read;
    size_t write;
    size_t count;
    unsigned produced_sum;
    unsigned consumed_sum;
    volatile bool producer_done;
    volatile bool consumer_done;
};

static struct pipe_test pipe_test;

/* 两个线程各自用汇编给 s0～s11 写入已知值，主动让出 32 次后核对，
 * 按参数指定的下标报告结果，用于检查跨线程切换的寄存器保存。 */
static void register_worker(void *argument)
{
    uintptr_t index = (uintptr_t)argument;

    register_results[index] = scheduler_register_probe(32);
}

/* A 持续计数直到 B 更新标志，B 要先看到 A 的进展才更新标志。
 * 二者在循环中都不主动让出，单 hart 上需要时钟抢占才能完成握手。 */
static void preempt_worker_a(void *argument)
{
    (void)argument;
    while (preempt_b == 0)
        ++preempt_a;
}

static void preempt_worker_b(void *argument)
{
    (void)argument;
    while (preempt_a == 0)
        ;
    ++preempt_b;
}

/* 记录从请求休眠到实际恢复执行的节拍差。到期只表示线程就绪，所以验证下限 4。 */
static void sleep_worker(void *argument)
{
    uint64_t before = scheduler_ticks();

    (void)argument;
    scheduler_sleep(4);
    sleep_elapsed = scheduler_ticks() - before;
    sleep_done = true;
}

static void producer(void *argument)
{
    struct pipe_test *pipe = argument;

    for (unsigned value = 1; value <= 64; ++value) {
        uint64_t flags = irq_save();

        spin_lock(&pipe->lock);
        /* 满时先入等待队列，释放锁并切走。被唤醒后会重新持锁，须再次检查剩余空间。 */
        while (pipe->count == 4)
            wait_queue_sleep(&pipe->writable, &pipe->lock);
        pipe->data[pipe->write] = value;
        pipe->write = (pipe->write + 1) % 4;
        ++pipe->count;
        pipe->produced_sum += value;
        wait_queue_wake_one(&pipe->readable);
        spin_unlock(&pipe->lock);
        irq_restore(flags);
        if ((value & 3U) == 0)
            scheduler_yield();
    }
    pipe->producer_done = true;
}

static void consumer(void *argument)
{
    struct pipe_test *pipe = argument;

    for (unsigned i = 0; i < 64; ++i) {
        uint64_t flags = irq_save();
        unsigned value;

        spin_lock(&pipe->lock);
        /* 空时等待生产者发布数据；取走一项后唤醒一个等待空槽的写者。 */
        while (pipe->count == 0)
            wait_queue_sleep(&pipe->readable, &pipe->lock);
        value = pipe->data[pipe->read];
        pipe->read = (pipe->read + 1) % 4;
        --pipe->count;
        pipe->consumed_sum += value;
        wait_queue_wake_one(&pipe->writable);
        spin_unlock(&pipe->lock);
        irq_restore(flags);
    }
    pipe->consumer_done = true;
}

static bool object_tests(void)
{
    struct refcount reference;
    LIST_HEAD(head);
    struct list_node a;
    struct list_node b;
    static const char message[] = "m4-log";
    char copy[sizeof(message)];
    void *objects[8];
    size_t baseline = page_free_count();

    /* 初始引用加一后应为 2；连续两次 put，只有最后一次应报告计数归零。 */
    refcount_init(&reference);
    refcount_get(&reference);
    if (refcount_read(&reference) != 2 || refcount_put(&reference) ||
        !refcount_put(&reference))
        return false;
    /* 检查尾插后首尾指针的顺序，再验证全部摘除后链表恢复为空。 */
    list_init(&a);
    list_init(&b);
    list_push_back(&head, &a);
    list_push_back(&head, &b);
    if (head.next != &a || head.prev != &b)
        return false;
    list_remove(&a);
    list_remove(&b);
    if (!list_empty(&head))
        return false;
    /* 同时分配并写入 16 到 2048 字节的八个对象，释放后核对空闲物理页数。 */
    for (size_t i = 0; i < 8; ++i) {
        objects[i] = kmalloc((size_t)1 << (i + 4));
        if (!objects[i])
            return false;
        memset(objects[i], (int)i, (size_t)1 << (i + 4));
    }
    for (size_t i = 0; i < 8; ++i)
        kfree(objects[i]);
    if (page_free_count() != baseline)
        return false;
    /* 连同字符串终止符一起写入，读回时同时检查长度与每个字节。 */
    log_write(message, sizeof(message));
    return log_read(copy, sizeof(copy)) == sizeof(copy) &&
           memcmp(message, copy, sizeof(copy)) == 0;
}

bool run_m4_tests(void)
{
    uint64_t preemptions;

    if (!object_tests())
        return false;
    console_puts("M4 OBJECT PASS\n");
    register_results[0] = false;
    register_results[1] = false;
    if (!thread_create("register-a", register_worker, (void *)0) ||
        !thread_create("register-b", register_worker, (void *)1))
        return false;
    while (!register_results[0] || !register_results[1])
        scheduler_yield();
    scheduler_reap();
    console_puts("M4 SWITCH PASS\n");

    preempt_a = 0;
    preempt_b = 0;
    preemptions = scheduler_preemptions();
    if (!thread_create("preempt-a", preempt_worker_a, 0) ||
        !thread_create("preempt-b", preempt_worker_b, 0))
        return false;
    /* 测试主线程休眠，让两个忙等线程运行，醒来后核对双方进展及抢占计数增量。 */
    scheduler_sleep(6);
    if (preempt_a == 0 || preempt_b == 0 ||
        scheduler_preemptions() <= preemptions)
        return false;
    scheduler_reap();
    console_puts("M4 TIMER PASS\n");

    sleep_done = false;
    sleep_elapsed = 0;
    if (!thread_create("sleeper", sleep_worker, 0))
        return false;
    while (!sleep_done)
        scheduler_yield();
    if (sleep_elapsed < 4)
        return false;
    scheduler_reap();
    console_puts("M4 SLEEP PASS\n");

    memset(&pipe_test, 0, sizeof(pipe_test));
    spinlock_init(&pipe_test.lock);
    wait_queue_init(&pipe_test.readable);
    wait_queue_init(&pipe_test.writable);
    if (!thread_create("producer", producer, &pipe_test) ||
        !thread_create("consumer", consumer, &pipe_test))
        return false;
    while (!pipe_test.producer_done || !pipe_test.consumer_done)
        scheduler_yield();
    scheduler_reap();
    console_puts("M4 QUEUE PASS\n");
    /* 两个线程都完成后，缓冲区应为空，生产和消费的总和都应等于 1 到 64 的和。 */
    return pipe_test.count == 0 &&
           pipe_test.produced_sum == pipe_test.consumed_sum &&
           pipe_test.produced_sum == (64U * 65U) / 2U;
}
