#ifndef MINICORE_USER_LIB_H
#define MINICORE_USER_LIB_H
#include <stdbool.h>
#include "../src/abi.h"
#include "../src/runtime.h"

/* MiniCore's small ABI deliberately returns negative errno directly. There is
 * no libc, TLS errno, dynamic linker, or Linux syscall-number compatibility. */
long call(long number, long a, long b, long c, long d, long e, long f);
#define sc0(n) call(n,0,0,0,0,0,0)
#define sc1(n,a) call(n,(long)(a),0,0,0,0,0)
#define sc2(n,a,b) call(n,(long)(a),(long)(b),0,0,0,0)
#define sc3(n,a,b,c) call(n,(long)(a),(long)(b),(long)(c),0,0,0)
static inline long openat(long dir, const char *path, long flags) { return sc3(SYS_OPENAT,dir,path,flags); }
static inline long open(const char *path, long flags) { return openat(AT_FDCWD,path,flags); }
static inline long close(long fd) { return sc1(SYS_CLOSE,fd); }
static inline long read(long fd, void *p, size_t n) { return sc3(SYS_READ,fd,p,n); }
static inline long write(long fd, const void *p, size_t n) { return sc3(SYS_WRITE,fd,p,n); }
static inline long fork(void) { return sc0(SYS_FORK); }
static inline long dup(long fd) { return sc1(SYS_DUP,fd); }
static inline long dup2(long fd, long to) { return sc2(SYS_DUP2,fd,to); }
static inline long pipe(int32_t *p) { return sc1(SYS_PIPE,p); }
static inline long seek(long fd, long off, long how) { return sc3(SYS_LSEEK,fd,off,how); }
static inline long exec(const char *p, char *const *argv) { return sc3(SYS_EXEC,p,argv,0); }
static inline long wait(long pid, long *status) { return sc3(SYS_WAITPID,pid,status,0); }
static inline long chdir(const char *p) { return sc1(SYS_CHDIR,p); }
static inline long unmap(long p, long n) { return sc2(SYS_MUNMAP,p,n); }
static inline long map(long p, long n, long prot, long flags, long fd, long off) { return call(SYS_MMAP,p,n,prot,flags,fd,off); }
__attribute__((noreturn)) void exit(long status);
int puts(const char *s);
int write_all(long fd, const void *buffer, size_t size);
int equal(const char *a, const char *b);
#endif
