#ifndef MINICORE_KERNEL_ELF_H
#define MINICORE_KERNEL_ELF_H

#include <stddef.h>
#include <stdint.h>

#include "../mm/address_space.h"

#define EXEC_MAX_ARGS 8U
#define EXEC_MAX_ENVS 8U
#define EXEC_STRING_SIZE 64U

struct exec_arguments {
    size_t argc;
    size_t envc;
    char argv[EXEC_MAX_ARGS][EXEC_STRING_SIZE];
    char envp[EXEC_MAX_ENVS][EXEC_STRING_SIZE];
};

/* A fully prepared, inactive image.  The caller either commits or destroys it. */
struct exec_image {
    struct address_space as;
    uint64_t entry;
    uint64_t stack_pointer;
};

bool elf_load(const struct page_table *kernel_table, const char *path,
              const struct exec_arguments *arguments,
              struct exec_image *image);
void elf_image_destroy(struct exec_image *image);

#endif
