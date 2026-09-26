/*
 * 启动阶段使用的扁平设备树（DTB/FDT）解析器。
 *
 * 解析顺序为：头部校验 -> 内存保留表 -> 结构块。结构块中的属性名来自
 * 字符串块，属性值则直接引用原始 blob；整个过程不分配堆内存，也不构造
 * 完整的设备树，只将内存、保留区、UART、PLIC、virtio MMIO 和时基频率
 * 等启动信息写入 boot_info。
 *
 * DTB 中的整数以大端编码，每个 cell 占 32 位。本实现用至多两个 cell
 * 表示地址或长度，并通过固定大小的数组限制树深度及可记录的资源数量。
 */
#include "dtb.h"

#include "../runtime.h"

/* 结构块是一串 token；BEGIN/END_NODE 描述层级，PROP 后跟属性数据。 */
#define FDT_MAGIC 0xd00dfeedU
#define FDT_BEGIN_NODE 1U
#define FDT_END_NODE 2U
#define FDT_PROP 3U
#define FDT_NOP 4U
#define FDT_END 9U

/* 使用 v17 的头部布局；大小和深度上限也限制了异常输入的解析开销。 */
#define FDT_HEADER_SIZE 40U
#define FDT_SUPPORTED_VERSION 17U
#define FDT_MAX_SIZE (16U * 1024U * 1024U)
#define FDT_MAX_DEPTH 32

/* 校验后的各块视图。指针均借用 blob，size/offset 的单位均为字节。 */
struct fdt_view {
    const uint8_t *blob;
    size_t total_size;
    const uint8_t *structure;
    size_t structure_size;
    const uint8_t *strings;
    size_t strings_size;
    size_t reservation_offset;
};

/*
 * 当前路径上一个节点的暂存状态，遇到 END_NODE 时才汇总其资源。
 * 属性保留指针和长度，使用时再按对应编码解释；未出现的属性指针为 NULL。
 * address_cells/size_cells 描述本节点的子节点如何编码 reg，解析本节点
 * 自己的 reg 时应读取父节点的这两个字段。
 */
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
    /* 缺少 ranges 与存在但为空含义不同，不能仅用长度区分。 */
    bool ranges_present;
    /* 仅对根节点的直属 reserved-memory、cpus 节点置位。 */
    bool reserved_memory;
    bool cpus;
};

/* 逐字节组装，既处理端序，也避免未对齐的整数访问；调用方保证可读长度。 */
static uint32_t read_be32(const void *pointer)
{
    const uint8_t *p = pointer;

    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}

/* 内存保留表使用 64 位字段，高 32 位在前。 */
static uint64_t read_be64(const void *pointer)
{
    const uint8_t *p = pointer;

    return ((uint64_t)read_be32(p) << 32) | read_be32(p + 4);
}

/* 用减法检查半开区间，避免 offset + length 自身溢出。 */
static bool range_within(size_t total, size_t offset, size_t length)
{
    return offset <= total && length <= total - offset;
}

/* 在计算物理地址或区间末端之前检测无符号加法溢出。 */
static bool add_overflows_u64(uint64_t left, uint64_t right)
{
    return left > UINT64_MAX - right;
}

/* 调用方先验证区间在 blob 内，保证这里的两个末端加法不会溢出。 */
static bool regions_overlap(size_t first_offset, size_t first_size,
                            size_t second_offset, size_t second_size)
{
    return first_offset < second_offset + second_size &&
           second_offset < first_offset + first_size;
}

/* 属性必须恰好包含一个以 NUL 结尾的目标字符串，不接受额外尾随字节。 */
static bool bytes_equal_string(const uint8_t *bytes, size_t length,
                               const char *string)
{
    size_t string_length = strlen(string);

    return length == string_length + 1 && bytes[string_length] == '\0' &&
           memcmp(bytes, string, string_length) == 0;
}

/* 比较完整节点名；不会去掉诸如 memory@80000000 中的单元地址后缀。 */
static bool name_equal(const struct node *node, const char *name)
{
    size_t length = strlen(name);

    return node->name_length == length &&
           memcmp(node->name, name, length) == 0;
}

/* compatible 是多个 NUL 结尾字符串的串联，逐项有界查找目标兼容标识。 */
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

/* 缺省启用；显式 status 只接受 okay/ok。这里只检查该节点自身的状态。 */
static bool node_enabled(const struct node *node)
{
    if (!node->status)
        return true;
    return bytes_equal_string(node->status, node->status_length, "okay") ||
           bytes_equal_string(node->status, node->status_length, "ok");
}

/*
 * 将 0、1 或 2 个大端 cell 合并为 uint64_t；零个 cell 的结果为零。
 * 更宽的编码超出本实现能力。调用方负责检查 data 所属属性的长度。
 */
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

/*
 * reg 中的地址属于父总线地址空间。沿祖先路径逐层应用 ranges，最终得到
 * 根地址空间中的物理地址；根节点的直属设备无需转换。
 * 每层映射必须容纳整个 [address, address + size) 区间。
 */
static enum dtb_error translate_address(const struct node *nodes, int depth,
                                        uint64_t size, uint64_t *address)
{
    for (int bus_depth = depth - 1; bus_depth > 0; --bus_depth) {
        const struct node *bus = &nodes[bus_depth];
        const struct node *parent = &nodes[bus_depth - 1];
        size_t entry_cells;
        size_t entry_size;
        bool matched = false;

        /* 缺少映射不能推断为直通；显式空 ranges 才表示地址保持不变。 */
        if (!bus->ranges_present)
            return DTB_ERR_UNSUPPORTED;
        if (bus->ranges_length == 0)
            continue;
        if (bus->address_cells > 2 || parent->address_cells > 2 ||
            bus->size_cells > 2)
            return DTB_ERR_UNSUPPORTED;

        /* 每项依次是：子总线地址、父总线地址、映射长度。 */
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

            /* 先比较再相减，避免通过地址末端加法判断包含关系时溢出。 */
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

/* 追加一个非空区间；不排序、不合并也不去重，容量由 boot_info 数组决定。 */
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

/*
 * reg 由若干 (地址, 长度) 元组组成，其宽度由父节点的 cell 数决定。
 * 检查属性恰好由完整元组组成后，逐项解码、转换为物理地址并追加到输出。
 */
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

/*
 * 单寄存器窗口设备的辅助入口。虽名为 first，实际仍解析全部 reg：
 * 要求恰有一个非零长度区间，多个非空区间会触发容量错误，而非截取首项。
 */
static enum dtb_error parse_first_reg(const struct node *nodes, int depth,
                                      struct physical_range *range)
{
    size_t count = 0;
    enum dtb_error error = parse_reg(nodes, depth, range, &count, 1);

    if (error != DTB_OK)
        return error;
    return count == 1 ? DTB_OK : DTB_ERR_PROPERTY;
}

/* 节点属性收集完毕后按类型提取资源；不识别的节点不会产生设备记录。 */
static enum dtb_error finish_node(const struct node *nodes, int depth,
                                  struct boot_info *info)
{
    const struct node *node = &nodes[depth];
    bool enabled = node_enabled(node);
    enum dtb_error error;

    /* 通过 device_type 识别内存，允许一个 memory 节点提供多个 reg 区间。 */
    if (enabled && node->device_type &&
        bytes_equal_string(node->device_type, node->device_type_length,
                           "memory")) {
        error = parse_reg(nodes, depth, info->memory, &info->memory_count,
                          BOOT_MAX_MEMORY_RANGES);
        if (error != DTB_OK)
            return error;
    }

    /* 仅收集 /reserved-memory 直属子节点中带 reg 的静态保留区。 */
    if (enabled && depth > 0 && nodes[depth - 1].reserved_memory &&
        node->reg) {
        error = parse_reg(nodes, depth, info->reserved,
                          &info->reserved_count, BOOT_MAX_RESERVED_RANGES);
        if (error != DTB_OK)
            return error;
    }

    if (!enabled)
        return DTB_OK;

    /* boot_info 只容纳一个 UART；这里只支持单个 32 位 interrupts 值。 */
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

    /* PLIC 记录 MMIO 窗口及可选的中断源数量，不解析中断上下文拓扑。 */
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

    /* virtio 可有多个实例，窗口和 IRQ 使用相同下标保存。 */
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

/*
 * 根据头部声明的总长度检查各块的位置、对齐及结构块/字符串块重叠情况。
 * 接口没有传入实际缓冲区长度，因此调用方必须保证头部和声明的 blob
 * 所在内存可读；这些检查不能验证底层内存映射是否真实存在。
 */
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

    /* 按固定字节偏移读取，避免 C 结构体布局或宿主端序影响头部解释。 */
    total_size = read_be32(bytes + 4);
    structure_offset = read_be32(bytes + 8);
    strings_offset = read_be32(bytes + 12);
    reservation_offset = read_be32(bytes + 16);
    version = read_be32(bytes + 20);
    last_compatible_version = read_be32(bytes + 24);
    /* 偏移 28 的 boot_cpuid_phys 当前无需使用。 */
    strings_size = read_be32(bytes + 32);
    structure_size = read_be32(bytes + 36);

    /* 接受 v17，以及声明仍兼容 v17 的更高版本。 */
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

/*
 * 头部指向的内存保留表独立于 /reserved-memory 节点，每项为两个大端
 * uint64_t（物理地址、长度），以 (0, 0) 终止；两者汇入同一保留区数组。
 * 表没有单独的长度字段，必须在每次读取前检查越界和与其他块的重叠。
 */
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

/* 属性的 nameoff 相对于字符串块；确认块内存在 NUL 后才返回字符串指针。 */
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

/* 用于已保证 NUL 结尾的属性名；原始属性值应使用有长度限制的比较函数。 */
static bool string_equal(const char *left, const char *right)
{
    while (*left && *left == *right) {
        ++left;
        ++right;
    }
    return *left == *right;
}

/*
 * 立即读取影响子节点解析的 cell 数，其余设备属性暂存到 END_NODE 处理。
 * 未识别的属性直接忽略；重复的已识别属性会覆盖此前记录。
 */
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

/*
 * 以固定栈遍历结构块：BEGIN_NODE 入栈、PROP 更新栈顶、END_NODE 汇总出栈。
 * depth == -1 表示当前不在任何节点内，根节点占索引 0。祖先状态在子节点
 * 完成前一直保留，以供 reg 解码和 ranges 地址转换使用。
 */
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
            /* 每个节点独立使用缺省 cell 数，不从父节点继承这两个属性。 */
            node->address_cells = 2;
            node->size_cells = 1;
            node->reserved_memory = depth == 1 &&
                                    name_equal(node, "reserved-memory");
            node->cpus = depth == 1 && name_equal(node, "cpus");
            /* 跳过节点名的 NUL，再跳过填充，令下一 token 从 4 字节边界开始。 */
            ++offset;
            offset = (offset + 3U) & ~(size_t)3U;
            /* 结构块只能有一个根节点，且根节点名必须为空字符串。 */
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
            /* PROP token 后依次是值长度、属性名偏移和属性值本体。 */
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
            /* length 不含对齐填充；下一 token 仍从 4 字节边界开始。 */
            offset += length;
            offset = (offset + 3U) & ~(size_t)3U;
        } else if (token == FDT_NOP) {
            continue;
        } else if (token == FDT_END) {
            /* 结束标记仅在已出现根节点且所有节点均闭合时有效。 */
            return saw_root && depth == -1 ? DTB_OK : DTB_ERR_STRUCTURE;
        } else {
            return DTB_ERR_STRUCTURE;
        }
    }
    return DTB_ERR_STRUCTURE;
}

/*
 * 对外解析入口：先清空输出，再依次填入 DTB 自身位置、保留表及树中资源。
 * 遇错立即返回，不回滚已填字段；调用方仅应在返回 DTB_OK 后使用解析结果。
 * 成功只代表已完成本解析器的检查，不保证所有平台必需设备都存在。
 */
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

/* 返回静态错误描述，供启动诊断使用；调用方无需释放返回的字符串。 */
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
