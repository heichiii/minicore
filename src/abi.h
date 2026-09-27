#ifndef MINICORE_ABI_H
#define MINICORE_ABI_H

/* Shared custom ABI: keep kernel and freestanding userspace in agreement.
 * a7 selects the call, a0..a5 carry arguments, a0 is a signed result. */
#define SYS_WRITE 1
#define SYS_EXIT 2
#define SYS_YIELD 3
#define SYS_GETPID 4
#define SYS_EXEC 5
#define SYS_WAITPID 6
#define SYS_FORK 7
#define SYS_OPENAT 8
#define SYS_CLOSE 9
#define SYS_READ 10
#define SYS_DUP 11
#define SYS_DUP2 12
#define SYS_GETDENTS 13
#define SYS_PIPE 14
#define SYS_CHDIR 15
#define SYS_GETCWD 16
#define SYS_BRK 17
#define SYS_MMAP 18
#define SYS_MUNMAP 19
#define SYS_LSEEK 20

#define E_PERM 1
#define E_NOENT 2
#define E_IO 5
#define E_2BIG 7
#define E_BADF 9
#define E_CHILD 10
#define E_NOMEM 12
#define E_FAULT 14
#define E_EXIST 17
#define E_NOTDIR 20
#define E_ISDIR 21
#define E_INVAL 22
#define E_MFILE 24
#define E_FBIG 27
#define E_SPIPE 29
#define E_ROFS 30
#define E_PIPE 32
#define E_NAMETOOLONG 36
#define E_NOSYS 38
#define E_LOOP 40

#define AT_FDCWD (-100)
#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR 2
#define O_ACCMODE 3
#define O_CREAT 0x40
#define O_EXCL 0x80
#define O_TRUNC 0x200
#define O_APPEND 0x400
#define O_DIRECTORY 0x10000
#define O_NOFOLLOW 0x20000
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#define PROT_NONE 0
#define PROT_READ 1
#define PROT_WRITE 2
#define PROT_EXEC 4
#define MAP_PRIVATE 2
#define MAP_ANONYMOUS 0x20
#define MAP_FIXED_NOREPLACE 0x100000
#define FD_COUNT 32
#define PATH_LIMIT 256
#define NAME_LIMIT 64
#define DT_DIR 4
#define DT_REG 8
#define DT_LNK 10
#define DT_CHR 2

#ifndef __ASSEMBLER__
#include <stdint.h>
/* getdents emits whole records only; offset is an entry index shared by dup. */
struct directory_entry {
    uint64_t inode;
    uint32_t type;
    uint32_t reserved;
    char name[NAME_LIMIT];
};
#endif
#endif
