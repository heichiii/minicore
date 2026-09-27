#ifndef MINICORE_FD_H
#define MINICORE_FD_H
#include <stdint.h>
struct process;
struct trap_frame;
int fd_install_stdio(struct process *);
int64_t fd_syscall(struct trap_frame *);
#endif
