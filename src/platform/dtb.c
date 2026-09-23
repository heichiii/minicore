#include "dtb.h"

#include "../runtime.h"

#define FDT_MAGIC 0xd00dfeedU
#define FDT_BEGIN_NODE 1U
#define FDT_END_NODE 2U
#define FDT_PROP 3U
#define FDT_NOP 4U
#define FDT_END 9U

#define FDT_HEADER_SIZE 40U
#define FDT_SUPPORTED_VERSION 17U
#define FDT_MAX_SIZE (16U * 1024U * 1024U)
#define FDT_MAX_DEPTH 32

struct fdt_view {
    const uint8_t *blob;
    size_t total_size;
    const uint8_t *structure;
    size_t structure_size;
    const uint8_t *strings;
    size_t strings_size;
    size_t reservation_offset;
};

struct node {
    const char *name;
    size_t name_length;
    uint32_t address_cells;
    uint32_t size_cells;
    const uint8_t *ranges;
    size_t ranges_length;
    const uint8_t *reg;
    size_t reg_length;
    const uint8_t *compatible;
    size_t compatible_length;
    const uint8_t *device_type;
    size_t device_type_length;
    const uint8_t *status;
    size_t status_length;
    const uint8_t *interrupts;
    size_t interrupts_length;
    const uint8_t *plic_source_count;
    size_t plic_source_count_length;
    bool ranges_present;
    bool reserved_memory;
    bool cpus;
};

static uint32_t read_be32(const void *pointer)
{
    const uint8_t *p = pointer;

    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}

static uint64_t read_be64(const void *pointer)
{
    const uint8_t *p = pointer;

    return ((uint64_t)read_be32(p) << 32) | read_be32(p + 4);
}

static bool range_within(size_t total, size_t offset, size_t length)
{
    return offset <= total && length <= total - offset;
}

static bool add_overflows_u64(uint64_t left, uint64_t right)
{
    return left > UINT64_MAX - right;
}

static bool regions_overlap(size_t first_offset, size_t first_size,
                            size_t second_offset, size_t second_size)
{
    return first_offset < second_offset + second_size &&
           second_offset < first_offset + first_size;
}

static bool bytes_equal_string(const uint8_t *bytes, size_t length,
                               const char *string)
{
    size_t string_length = strlen(string);

    return length == string_length + 1 && bytes[string_length] == '\0' &&
           memcmp(bytes, string, string_length) == 0;
}

static bool name_equal(const struct node *node, const char *name)
{
    size_t length = strlen(name);

    return node->name_length == length &&
           memcmp(node->name, name, length) == 0;
}

static bool compatible_has(const struct node *node, const char *wanted)
{
    size_t wanted_length = strlen(wanted);
    size_t offset = 0;

    while (offset < node->compatible_length) {
        size_t length = 0;

        while (offset + length < node->compatible_length &&
               node->compatible[offset + length] != '\0')
            ++length;
        if (offset + length == node->compatible_length)
            return false;
        if (length == wanted_length &&
            memcmp(node->compatible + offset, wanted, wanted_length) == 0)
            return true;
        offset += length + 1;
    }
    return false;
}

static bool node_enabled(const struct node *node)
{
    if (!node->status)
        return true;
    return bytes_equal_string(node->status, node->status_length, "okay") ||
           bytes_equal_string(node->status, node->status_length, "ok");
}

static enum dtb_error decode_cells(const uint8_t *data, uint32_t cells,
                                   uint64_t *value)
{
    if (cells > 2)
        return DTB_ERR_UNSUPPORTED;

    *value = 0;
    for (uint32_t i = 0; i < cells; ++i)
        *value = (*value << 32) | read_be32(data + i * 4U);
    return DTB_OK;
}

static enum dtb_error translate_address(const struct node *nodes, int depth,
                                        uint64_t size, uint64_t *address)
{
    for (int bus_depth = depth - 1; bus_depth > 0; --bus_depth) {
        const struct node *bus = &nodes[bus_depth];
        const struct node *parent = &nodes[bus_depth - 1];
        size_t entry_cells;
        size_t entry_size;
        bool matched = false;

        if (!bus->ranges_present)
            return DTB_ERR_UNSUPPORTED;
        if (bus->ranges_length == 0)
            continue;
        if (bus->address_cells > 2 || parent->address_cells > 2 ||
            bus->size_cells > 2)
            return DTB_ERR_UNSUPPORTED;

        entry_cells = (size_t)bus->address_cells + parent->address_cells +
                      bus->size_cells;
        entry_size = entry_cells * 4U;
        if (entry_size == 0 || bus->ranges_length % entry_size != 0)
            return DTB_ERR_PROPERTY;

        for (size_t offset = 0; offset < bus->ranges_length;
             offset += entry_size) {
            const uint8_t *entry = bus->ranges + offset;
            uint64_t child_address;
            uint64_t parent_address;
            uint64_t range_size;
            enum dtb_error error;

            error = decode_cells(entry, bus->address_cells, &child_address);
            if (error != DTB_OK)
                return error;
            entry += bus->address_cells * 4U;
            error = decode_cells(entry, parent->address_cells,
                                 &parent_address);
            if (error != DTB_OK)
                return error;
            entry += parent->address_cells * 4U;
            error = decode_cells(entry, bus->size_cells, &range_size);
            if (error != DTB_OK)
                return error;

            if (*address >= child_address && size <= range_size &&
                *address - child_address <= range_size - size) {
                uint64_t displacement = *address - child_address;

                if (add_overflows_u64(parent_address, displacement))
                    return DTB_ERR_PROPERTY;
                *address = parent_address + displacement;
                matched = true;
                break;
            }
        }
        if (!matched)
            return DTB_ERR_PROPERTY;
    }
    return DTB_OK;
}

static enum dtb_error add_range(struct physical_range *ranges, size_t *count,
                                size_t capacity, uint64_t base, uint64_t size)
{
    if (size == 0)
        return DTB_OK;
    if (add_overflows_u64(base, size))
        return DTB_ERR_PROPERTY;
    if (*count == capacity)
        return DTB_ERR_CAPACITY;
    ranges[*count].base = base;
    ranges[*count].size = size;
    ++*count;
    return DTB_OK;
}

static enum dtb_error parse_reg(const struct node *nodes, int depth,
                                struct physical_range *ranges, size_t *count,
                                size_t capacity)
{
    const struct node *node = &nodes[depth];
    const struct node *parent;
    size_t tuple_cells;
    size_t tuple_size;

    if (!node->reg)
        return DTB_ERR_PROPERTY;
    if (depth == 0)
        return DTB_ERR_STRUCTURE;
    parent = &nodes[depth - 1];
    if (parent->address_cells > 2 || parent->size_cells > 2)
        return DTB_ERR_UNSUPPORTED;
    tuple_cells = (size_t)parent->address_cells + parent->size_cells;
    tuple_size = tuple_cells * 4U;
    if (tuple_size == 0 || node->reg_length == 0 ||
        node->reg_length % tuple_size != 0)
        return DTB_ERR_PROPERTY;

    for (size_t offset = 0; offset < node->reg_length; offset += tuple_size) {
        uint64_t address;
        uint64_t size;
        enum dtb_error error;

        error = decode_cells(node->reg + offset, parent->address_cells,
                             &address);
        if (error != DTB_OK)
            return error;
        error = decode_cells(node->reg + offset + parent->address_cells * 4U,
                             parent->size_cells, &size);
        if (error != DTB_OK)
            return error;
        error = translate_address(nodes, depth, size, &address);
        if (error != DTB_OK)
            return error;
        error = add_range(ranges, count, capacity, address, size);
        if (error != DTB_OK)
            return error;
    }
    return DTB_OK;
}

static enum dtb_error parse_first_reg(const struct node *nodes, int depth,
                                      struct physical_range *range)
{
    size_t count = 0;
    enum dtb_error error = parse_reg(nodes, depth, range, &count, 1);

    if (error != DTB_OK)
        return error;
    return count == 1 ? DTB_OK : DTB_ERR_PROPERTY;
}

static enum dtb_error finish_node(const struct node *nodes, int depth,
                                  struct boot_info *info)
{
    const struct node *node = &nodes[depth];
    bool enabled = node_enabled(node);
    enum dtb_error error;

    if (enabled && node->device_type &&
        bytes_equal_string(node->device_type, node->device_type_length,
                           "memory")) {
        error = parse_reg(nodes, depth, info->memory, &info->memory_count,
                          BOOT_MAX_MEMORY_RANGES);
        if (error != DTB_OK)
            return error;
    }

    if (enabled && depth > 0 && nodes[depth - 1].reserved_memory &&
        node->reg) {
        error = parse_reg(nodes, depth, info->reserved,
                          &info->reserved_count, BOOT_MAX_RESERVED_RANGES);
        if (error != DTB_OK)
            return error;
    }

    if (!enabled)
        return DTB_OK;

    if (compatible_has(node, "ns16550a") ||
        compatible_has(node, "sifive,uart0")) {
        if (info->uart_present)
            return DTB_ERR_CAPACITY;
        error = parse_first_reg(nodes, depth, &info->uart);
        if (error != DTB_OK)
            return error;
        if (node->interrupts) {
            if (node->interrupts_length != 4)
                return DTB_ERR_UNSUPPORTED;
            info->uart_irq = read_be32(node->interrupts);
        }
        info->uart_present = true;
    }

    if (compatible_has(node, "sifive,plic-1.0.0") ||
        compatible_has(node, "riscv,plic0")) {
        if (info->plic_present)
            return DTB_ERR_CAPACITY;
        error = parse_first_reg(nodes, depth, &info->plic);
        if (error != DTB_OK)
            return error;
        if (node->plic_source_count) {
            if (node->plic_source_count_length != 4)
                return DTB_ERR_PROPERTY;
            info->plic_source_count = read_be32(node->plic_source_count);
        }
        info->plic_present = true;
    }

    if (compatible_has(node, "virtio,mmio")) {
        if (info->virtio_count == BOOT_MAX_VIRTIO_DEVICES)
            return DTB_ERR_CAPACITY;
        error = parse_first_reg(nodes, depth,
                                &info->virtio[info->virtio_count]);
        if (error != DTB_OK)
            return error;
        if (node->interrupts) {
            if (node->interrupts_length != 4)
                return DTB_ERR_UNSUPPORTED;
            info->virtio_irqs[info->virtio_count] =
                read_be32(node->interrupts);
        }
        ++info->virtio_count;
    }

    return DTB_OK;
}

static enum dtb_error validate_header(const void *blob, struct fdt_view *view)
{
    const uint8_t *bytes = blob;
    uint32_t total_size;
    uint32_t structure_offset;
    uint32_t strings_offset;
    uint32_t reservation_offset;
    uint32_t version;
    uint32_t last_compatible_version;
    uint32_t strings_size;
    uint32_t structure_size;

    if (!blob)
        return DTB_ERR_ARGUMENT;
    if (read_be32(bytes) != FDT_MAGIC)
        return DTB_ERR_MAGIC;

    total_size = read_be32(bytes + 4);
    structure_offset = read_be32(bytes + 8);
    strings_offset = read_be32(bytes + 12);
    reservation_offset = read_be32(bytes + 16);
    version = read_be32(bytes + 20);
    last_compatible_version = read_be32(bytes + 24);
    strings_size = read_be32(bytes + 32);
    structure_size = read_be32(bytes + 36);

    if (version < FDT_SUPPORTED_VERSION ||
        last_compatible_version > FDT_SUPPORTED_VERSION)
        return DTB_ERR_VERSION;
    if (total_size < FDT_HEADER_SIZE || total_size > FDT_MAX_SIZE)
        return DTB_ERR_BOUNDS;
    if (structure_offset < FDT_HEADER_SIZE ||
        strings_offset < FDT_HEADER_SIZE ||
        reservation_offset < FDT_HEADER_SIZE ||
        (structure_offset & 3U) != 0 || (reservation_offset & 7U) != 0 ||
        !range_within(total_size, structure_offset, structure_size) ||
        !range_within(total_size, strings_offset, strings_size) ||
        !range_within(total_size, reservation_offset, 16) ||
        regions_overlap(structure_offset, structure_size, strings_offset,
                        strings_size))
        return DTB_ERR_BOUNDS;

    view->blob = bytes;
    view->total_size = total_size;
    view->structure = bytes + structure_offset;
    view->structure_size = structure_size;
    view->strings = bytes + strings_offset;
    view->strings_size = strings_size;
    view->reservation_offset = reservation_offset;
    return DTB_OK;
}

static enum dtb_error parse_reservations(const struct fdt_view *view,
                                         struct boot_info *info)
{
    size_t offset = view->reservation_offset;

    for (;;) {
        uint64_t address;
        uint64_t size;
        enum dtb_error error;

        if (!range_within(view->total_size, offset, 16) ||
            regions_overlap(offset, 16,
                            (size_t)(view->structure - view->blob),
                            view->structure_size) ||
            regions_overlap(offset, 16,
                            (size_t)(view->strings - view->blob),
                            view->strings_size))
            return DTB_ERR_BOUNDS;
        address = read_be64(view->blob + offset);
        size = read_be64(view->blob + offset + 8);
        offset += 16;
        if (address == 0 && size == 0)
            return DTB_OK;
        error = add_range(info->reserved, &info->reserved_count,
                          BOOT_MAX_RESERVED_RANGES, address, size);
        if (error != DTB_OK)
            return error;
    }
}

static enum dtb_error property_name(const struct fdt_view *view,
                                    uint32_t offset, const char **name)
{
    if (offset >= view->strings_size)
        return DTB_ERR_BOUNDS;
    for (size_t i = offset; i < view->strings_size; ++i) {
        if (view->strings[i] == '\0') {
            *name = (const char *)view->strings + offset;
            return DTB_OK;
        }
    }
    return DTB_ERR_BOUNDS;
}

static bool string_equal(const char *left, const char *right)
{
    while (*left && *left == *right) {
        ++left;
        ++right;
    }
    return *left == *right;
}

static enum dtb_error record_property(struct node *node, const char *name,
                                      const uint8_t *value, size_t length,
                                      struct boot_info *info)
{
    if (string_equal(name, "#address-cells")) {
        if (length != 4)
            return DTB_ERR_PROPERTY;
        node->address_cells = read_be32(value);
    } else if (string_equal(name, "#size-cells")) {
        if (length != 4)
            return DTB_ERR_PROPERTY;
        node->size_cells = read_be32(value);
    } else if (string_equal(name, "ranges")) {
        node->ranges = value;
        node->ranges_length = length;
        node->ranges_present = true;
    } else if (string_equal(name, "reg")) {
        node->reg = value;
        node->reg_length = length;
    } else if (string_equal(name, "compatible")) {
        node->compatible = value;
        node->compatible_length = length;
    } else if (string_equal(name, "device_type")) {
        node->device_type = value;
        node->device_type_length = length;
    } else if (string_equal(name, "status")) {
        node->status = value;
        node->status_length = length;
    } else if (string_equal(name, "interrupts")) {
        node->interrupts = value;
        node->interrupts_length = length;
    } else if (string_equal(name, "riscv,ndev")) {
        node->plic_source_count = value;
        node->plic_source_count_length = length;
    } else if (node->cpus && string_equal(name, "timebase-frequency")) {
        if (length != 4)
            return DTB_ERR_PROPERTY;
        info->timebase_frequency = read_be32(value);
    }
    return DTB_OK;
}

static enum dtb_error parse_structure(const struct fdt_view *view,
                                      struct boot_info *info)
{
    struct node nodes[FDT_MAX_DEPTH];
    size_t offset = 0;
    int depth = -1;
    bool saw_root = false;

    while (range_within(view->structure_size, offset, 4)) {
        uint32_t token = read_be32(view->structure + offset);

        offset += 4;
        if (token == FDT_BEGIN_NODE) {
            size_t name_start = offset;
            struct node *node;

            if (depth + 1 == FDT_MAX_DEPTH)
                return DTB_ERR_CAPACITY;
            while (offset < view->structure_size &&
                   view->structure[offset] != '\0')
                ++offset;
            if (offset == view->structure_size)
                return DTB_ERR_BOUNDS;
            ++depth;
            node = &nodes[depth];
            memset(node, 0, sizeof(*node));
            node->name = (const char *)view->structure + name_start;
            node->name_length = offset - name_start;
            node->address_cells = 2;
            node->size_cells = 1;
            node->reserved_memory = depth == 1 &&
                                    name_equal(node, "reserved-memory");
            node->cpus = depth == 1 && name_equal(node, "cpus");
            ++offset;
            offset = (offset + 3U) & ~(size_t)3U;
            if (depth == 0) {
                if (saw_root || node->name_length != 0)
                    return DTB_ERR_STRUCTURE;
                saw_root = true;
            }
        } else if (token == FDT_END_NODE) {
            enum dtb_error error;

            if (depth < 0)
                return DTB_ERR_STRUCTURE;
            error = finish_node(nodes, depth, info);
            if (error != DTB_OK)
                return error;
            --depth;
        } else if (token == FDT_PROP) {
            uint32_t length;
            uint32_t name_offset;
            const char *name;
            enum dtb_error error;

            if (depth < 0 || !range_within(view->structure_size, offset, 8))
                return DTB_ERR_STRUCTURE;
            length = read_be32(view->structure + offset);
            name_offset = read_be32(view->structure + offset + 4);
            offset += 8;
            if (!range_within(view->structure_size, offset, length))
                return DTB_ERR_BOUNDS;
            error = property_name(view, name_offset, &name);
            if (error != DTB_OK)
                return error;
            error = record_property(&nodes[depth], name,
                                    view->structure + offset, length, info);
            if (error != DTB_OK)
                return error;
            offset += length;
            offset = (offset + 3U) & ~(size_t)3U;
        } else if (token == FDT_NOP) {
            continue;
        } else if (token == FDT_END) {
            return saw_root && depth == -1 ? DTB_OK : DTB_ERR_STRUCTURE;
        } else {
            return DTB_ERR_STRUCTURE;
        }
    }
    return DTB_ERR_STRUCTURE;
}

enum dtb_error dtb_parse(const void *blob, struct boot_info *info)
{
    struct fdt_view view;
    enum dtb_error error;

    if (!info)
        return DTB_ERR_ARGUMENT;
    memset(info, 0, sizeof(*info));
    error = validate_header(blob, &view);
    if (error != DTB_OK)
        return error;

    info->dtb_base = (uintptr_t)blob;
    info->dtb_size = view.total_size;
    error = parse_reservations(&view, info);
    if (error != DTB_OK)
        return error;
    return parse_structure(&view, info);
}

const char *dtb_error_string(enum dtb_error error)
{
    switch (error) {
    case DTB_OK:
        return "ok";
    case DTB_ERR_ARGUMENT:
        return "invalid argument";
    case DTB_ERR_MAGIC:
        return "bad magic";
    case DTB_ERR_VERSION:
        return "unsupported version";
    case DTB_ERR_BOUNDS:
        return "out of bounds";
    case DTB_ERR_STRUCTURE:
        return "malformed structure";
    case DTB_ERR_PROPERTY:
        return "malformed property";
    case DTB_ERR_UNSUPPORTED:
        return "unsupported encoding";
    case DTB_ERR_CAPACITY:
        return "boot info capacity exceeded";
    }
    return "unknown error";
}
