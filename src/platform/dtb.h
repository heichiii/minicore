#ifndef MINICORE_DTB_H
#define MINICORE_DTB_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BOOT_MAX_MEMORY_RANGES 8
#define BOOT_MAX_RESERVED_RANGES 16
#define BOOT_MAX_VIRTIO_DEVICES 16

struct physical_range {
    uint64_t base;
    uint64_t size;
};

struct boot_info {
    struct physical_range memory[BOOT_MAX_MEMORY_RANGES];
    size_t memory_count;

    struct physical_range reserved[BOOT_MAX_RESERVED_RANGES];
    size_t reserved_count;

    struct physical_range uart;
    uint32_t uart_irq;
    struct physical_range plic;
    uint32_t plic_source_count;
    struct physical_range virtio[BOOT_MAX_VIRTIO_DEVICES];
    uint32_t virtio_irqs[BOOT_MAX_VIRTIO_DEVICES];
    size_t virtio_count;

    uint64_t timebase_frequency;
    uintptr_t dtb_base;
    size_t dtb_size;
    bool uart_present;
    bool plic_present;
};

enum dtb_error {
    DTB_OK,
    DTB_ERR_ARGUMENT,
    DTB_ERR_MAGIC,
    DTB_ERR_VERSION,
    DTB_ERR_BOUNDS,
    DTB_ERR_STRUCTURE,
    DTB_ERR_PROPERTY,
    DTB_ERR_UNSUPPORTED,
    DTB_ERR_CAPACITY,
};

enum dtb_error dtb_parse(const void *blob, struct boot_info *info);
const char *dtb_error_string(enum dtb_error error);

#endif
