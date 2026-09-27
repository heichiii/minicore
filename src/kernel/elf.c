#include "elf.h"

#include "../fs/vfs.h"
#include "../mm/layout.h"
#include "../runtime.h"

#define ELF_MACHINE_RISCV 243U
#define ELF_TYPE_EXEC 2U
#define ELF_PROGRAM_LOAD 1U
#define ELF_PROGRAM_DYNAMIC 2U
#define ELF_PROGRAM_INTERP 3U
#define ELF_FLAG_EXEC 1U
#define ELF_FLAG_WRITE 2U
#define ELF_FLAG_READ 4U
#define ELF_MAX_HEADERS 16U
#define USER_END (UINT64_C(1) << 38)
#define USER_STACK_TOP UINT64_C(0x400000)
#define USER_STACK_PAGES 4U

/* On-disk ELF64 structures; field order and widths match the generic ABI. */
struct elf_header {
    unsigned char identity[16];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint64_t entry;
    uint64_t program_offset;
    uint64_t section_offset;
    uint32_t flags;
    uint16_t header_size;
    uint16_t program_entry_size;
    uint16_t program_count;
    uint16_t section_entry_size;
    uint16_t section_count;
    uint16_t section_names;
};

struct elf_program {
    uint32_t type;
    uint32_t flags;
    uint64_t offset;
    uint64_t virtual_address;
    uint64_t physical_address;
    uint64_t file_size;
    uint64_t memory_size;
    uint64_t alignment;
};

static bool read_exact(struct file *file, void *buffer, size_t size,
                       uint64_t offset)
{
    return file_pread(file, buffer, size, offset) == (int64_t)size;
}

static bool valid_header(const struct elf_header *header, size_t file_length)
{
    static const unsigned char identity[] = {0x7f, 'E', 'L', 'F', 2, 1, 1};
    uint64_t table_size;

    if (memcmp(header->identity, identity, sizeof(identity)) != 0 ||
        header->type != ELF_TYPE_EXEC || header->machine != ELF_MACHINE_RISCV ||
        header->version != 1 || header->header_size != sizeof(*header) ||
        header->program_entry_size != sizeof(struct elf_program) ||
        header->program_count == 0 || header->program_count > ELF_MAX_HEADERS)
        return false;
    table_size = (uint64_t)header->program_count * sizeof(struct elf_program);
    return header->program_offset <= file_length &&
           table_size <= file_length - header->program_offset;
}

static bool load_segment(struct exec_image *image, struct file *file,
                         size_t file_length, const struct elf_program *program)
{
    uint64_t first, end, flags = PTE_U;

    /* Linkers may emit a zero-sized LOAD for an empty data segment. */
    if (program->memory_size == 0)
        return program->file_size == 0;
    if (program->file_size > program->memory_size ||
        (program->flags & ~(ELF_FLAG_READ | ELF_FLAG_WRITE |
                            ELF_FLAG_EXEC)) != 0 ||
        (program->alignment > 1 &&
         (program->alignment & (program->alignment - 1)) != 0) ||
        program->virtual_address >= USER_END ||
        program->memory_size > USER_END - program->virtual_address ||
        program->offset > file_length ||
        program->file_size > file_length - program->offset ||
        ((program->virtual_address ^ program->offset) & PAGE_MASK) != 0 ||
        (program->flags & ELF_FLAG_WRITE &&
         !(program->flags & ELF_FLAG_READ)) ||
        (program->flags & (ELF_FLAG_WRITE | ELF_FLAG_EXEC)) ==
            (ELF_FLAG_WRITE | ELF_FLAG_EXEC))
        return false;
    if (program->flags & ELF_FLAG_READ)
        flags |= PTE_R;
    if (program->flags & ELF_FLAG_WRITE)
        flags |= PTE_W;
    if (program->flags & ELF_FLAG_EXEC)
        flags |= PTE_X;
    if (!(flags & (PTE_R | PTE_X)))
        return false;

    first = program->virtual_address & ~PAGE_MASK;
    if (program->virtual_address + program->memory_size > UINT64_MAX - PAGE_MASK)
        return false;
    end = (program->virtual_address + program->memory_size + PAGE_MASK) &
          ~PAGE_MASK;
    for (uint64_t address = first; address < end; address += PAGE_SIZE) {
        if (!address_space_add_page(&image->as, address, flags, 0, 0))
            return false;
    }

    /* Pages are already zeroed, so only the file-backed prefix needs copying. */
    uint64_t address = program->virtual_address;
    uint64_t offset = program->offset;
    uint64_t remaining = program->file_size;
    while (remaining) {
        uint64_t physical;
        size_t chunk = PAGE_SIZE - (address & PAGE_MASK);

        if (chunk > remaining)
            chunk = (size_t)remaining;
        if (!vm_query(&image->as.table, address, &physical, 0) ||
            !read_exact(file, phys_to_virt(physical), chunk, offset))
            return false;
        address += chunk;
        offset += chunk;
        remaining -= chunk;
    }
    return true;
}

static bool build_stack(struct exec_image *image,
                        const struct exec_arguments *arguments)
{
    uint64_t argv[EXEC_MAX_ARGS], envp[EXEC_MAX_ENVS];
    uint64_t words[1 + EXEC_MAX_ARGS + 1 + EXEC_MAX_ENVS + 1];
    uint64_t cursor = USER_STACK_TOP;
    size_t count = 0;

    /* The page below this fixed range is deliberately left unmapped. */
    for (size_t i = 0; i < USER_STACK_PAGES; ++i) {
        uint64_t address = USER_STACK_TOP - (USER_STACK_PAGES - i) * PAGE_SIZE;

        if (!address_space_add_page(&image->as, address,
                                    PTE_U | PTE_R | PTE_W, 0, 0))
            return false;
    }
    for (size_t i = arguments->envc; i > 0; --i) {
        size_t length = strlen(arguments->envp[i - 1]) + 1;

        cursor -= length;
        envp[i - 1] = cursor;
        if (!address_space_copy(&image->as,
                                (void *)arguments->envp[i - 1], cursor,
                                length, true))
            return false;
    }
    for (size_t i = arguments->argc; i > 0; --i) {
        size_t length = strlen(arguments->argv[i - 1]) + 1;

        cursor -= length;
        argv[i - 1] = cursor;
        if (!address_space_copy(&image->as,
                                (void *)arguments->argv[i - 1], cursor,
                                length, true))
            return false;
    }
    /* ABI layout: argc, argv pointers, NULL, envp pointers, NULL. */
    words[count++] = arguments->argc;
    for (size_t i = 0; i < arguments->argc; ++i)
        words[count++] = argv[i];
    words[count++] = 0;
    for (size_t i = 0; i < arguments->envc; ++i)
        words[count++] = envp[i];
    words[count++] = 0;
    cursor = (cursor - count * sizeof(uint64_t)) & ~UINT64_C(15);
    if (cursor < USER_STACK_TOP - USER_STACK_PAGES * PAGE_SIZE ||
        !address_space_copy(&image->as, words, cursor,
                            count * sizeof(uint64_t), true))
        return false;
    image->stack_pointer = cursor;
    return true;
}

bool elf_load(const struct page_table *kernel_table, const char *path,
              const struct exec_arguments *arguments,
              struct exec_image *image)
{
    struct elf_header header;
    struct file *file = vfs_open(path);
    bool executable_entry = false;
    bool result = false;
    size_t length;

    memset(image, 0, sizeof(*image));
    if (!file)
        return false;
    length = file_size(file);
    if (!read_exact(file, &header, sizeof(header), 0) ||
        !valid_header(&header, length) ||
        !address_space_init(&image->as, kernel_table))
        goto out;
    for (size_t i = 0; i < header.program_count; ++i) {
        struct elf_program program;
        uint64_t offset = header.program_offset + i * sizeof(program);

        if (!read_exact(file, &program, sizeof(program), offset))
            goto fail;
        if (program.type == ELF_PROGRAM_DYNAMIC ||
            program.type == ELF_PROGRAM_INTERP)
            goto fail;
        if (program.type != ELF_PROGRAM_LOAD)
            continue;
        if (!load_segment(image, file, length, &program))
            goto fail;
        if ((program.flags & ELF_FLAG_EXEC) &&
            header.entry >= program.virtual_address &&
            header.entry - program.virtual_address < program.memory_size)
            executable_entry = true;
    }
    if (!executable_entry || !build_stack(image, arguments))
        goto fail;
    image->entry = header.entry;
    __asm__ volatile("fence.i" : : : "memory");
    result = true;
    goto out;
fail:
    address_space_destroy(&image->as);
out:
    file_put(file);
    return result;
}

void elf_image_destroy(struct exec_image *image)
{
    address_space_destroy(&image->as);
}
