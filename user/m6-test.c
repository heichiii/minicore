#include "lib.h"

/* Failures print their source line so QEMU logs identify the violated ABI.
 * Tests run as ordinary U-mode programs: no privileged test-only syscalls. */
static void fail(unsigned line)
{
    char digits[12];
    size_t n = 0;
    do { digits[n++] = '0' + line % 10; line /= 10; } while (line);
    puts("M6 FAIL line ");
    while (n) write(1, &digits[--n], 1);
    puts("\n");
    exit(90);
}
#define CHECK(x) do { if (!(x)) fail(__LINE__); } while (0)

static void joined(long pid, long expected)
{
    long status = -999;
    CHECK(pid > 0 && wait(pid, &status) == pid && status == expected);
}

static void files(void)
{
    char b[256];
    CHECK(open("", 0) == -E_NOENT);
    CHECK(open("/missing", 0) == -E_NOENT);
    CHECK(open("/init/no", 0) == -E_NOTDIR);
    CHECK(open("/init/", 0) == -E_NOTDIR);
    CHECK(open("/init", O_DIRECTORY) == -E_NOTDIR);
    CHECK(open("/init", O_WRONLY) == -E_ROFS);
    CHECK(open("/new", O_CREAT | O_WRONLY) == -E_ROFS);
    CHECK(open((void *)0x500000, 0) == -E_FAULT);
    memset(b, 'x', sizeof(b));
    CHECK(open(b, 0) == -E_NAMETOOLONG);
    CHECK(open("/loop", 0) == -E_LOOP);
    CHECK(open("/link", O_NOFOLLOW) == -E_LOOP);
    CHECK(open("/dangling", O_RDONLY) == -E_NOENT);
    CHECK(open("/etc/message/..", O_RDONLY) == -E_NOTDIR);
    long link = open("/dir-link//./message", O_RDONLY);
    CHECK(link >= 0); close(link);
    link = open("/abs-link", O_RDONLY); CHECK(link >= 0); close(link);
    long fd = open("/link", O_RDONLY);
    CHECK(fd >= 3 && read(fd, b, 6) == 6 && !memcmp(b, "abcdef", 6));
    CHECK(close(fd) == 0 && close(fd) == -E_BADF);
    fd = open("/tmp/data", O_CREAT | O_TRUNC | O_RDWR);
    CHECK(fd >= 3 && write(fd, "abcdef", 6) == 6);
    CHECK(open("/tmp/data", O_CREAT | O_EXCL) == -E_EXIST);
    CHECK(seek(fd, 0, SEEK_SET) == 0);
    long shared = dup(fd), independent = open("/tmp/data", O_RDONLY);
    CHECK(shared >= 3 && independent >= 3);
    CHECK(read(fd, b, 2) == 2 && !memcmp(b, "ab", 2));
    CHECK(read(shared, b, 2) == 2 && !memcmp(b, "cd", 2));
    CHECK(read(independent, b, 1) == 1 && b[0] == 'a');
    CHECK(read(fd, (void *)0x500000, 1) == -E_FAULT);
    CHECK(seek(fd, 0, SEEK_CUR) == 4);
    CHECK(write(independent, "x", 1) == -E_BADF);
    CHECK(dup2(fd, shared) == shared && dup2(fd, fd) == fd);
    CHECK(dup2(-1, shared) == -E_BADF && dup2(fd, FD_COUNT) == -E_BADF);
    long pid = fork();
    CHECK(pid >= 0);
    if (!pid) {
        CHECK(read(shared, b, 1) == 1 && b[0] == 'e');
        close(shared); close(fd); close(independent);
        exit(0);
    }
    joined(pid, 0);
    CHECK(read(fd, b, 1) == 1 && b[0] == 'f');
    CHECK(read(shared, b, 1) == 0);
    CHECK(close(fd) == 0 && seek(shared, 0, SEEK_SET) == 0);
    close(shared); close(independent);
    fd = open("/tmp/data", O_WRONLY | O_APPEND);
    CHECK(fd >= 0 && seek(fd, 0, SEEK_SET) == 0 && write(fd, "g", 1) == 1);
    CHECK(seek(fd, 0, SEEK_CUR) == 7);
    close(fd);
    fd = open("/tmp/data", O_RDWR);
    CHECK(seek(fd, 4097, SEEK_SET) == 4097 && write(fd, "z", 1) == 1);
    CHECK(seek(fd, 4096, SEEK_SET) == 4096 && read(fd, b, 2) == 2 && b[0] == 0 && b[1] == 'z');
    CHECK(seek(fd, 65536, SEEK_SET) == 65536 && write(fd, "z", 1) == -E_FBIG);
    CHECK(seek(fd, -1, SEEK_SET) == -E_INVAL);
    close(fd);
    CHECK(chdir("/tmp") == 0);
    CHECK(sc2(SYS_GETCWD,b,sizeof(b)) == 5 && equal(b, "/tmp"));
    CHECK(sc2(SYS_GETCWD,b,1) == -E_INVAL);
    long dir = open(".", O_DIRECTORY);
    CHECK(dir >= 0);
    fd = openat(dir, "data", O_RDONLY);
    CHECK(fd >= 0); close(fd);
    CHECK(openat(-1, "data", 0) == -E_BADF);
    fd = openat(-1, "/link", 0); CHECK(fd >= 0); close(fd);
    CHECK(chdir("../bin/../tmp/..") == 0);
    CHECK(chdir("../../..") == 0 && sc2(SYS_GETCWD,b,sizeof(b)) == 2 && equal(b,"/"));
    CHECK(chdir("/init") == -E_NOTDIR);
    pid = fork(); CHECK(pid >= 0);
    if (!pid) {
        CHECK(chdir("/dev") == 0);
        CHECK(sc2(SYS_GETCWD,b,sizeof(b)) == 5 && equal(b,"/dev"));
        fd = openat(dir,"data",O_RDONLY); CHECK(fd >= 0); close(fd);
        CHECK(chdir("..") == 0); exit(0);
    }
    joined(pid,0);
    CHECK(sc2(SYS_GETCWD,b,sizeof(b)) == 2 && equal(b,"/"));
    struct directory_entry entry;
    CHECK(sc3(SYS_GETDENTS,dir,0x500000,sizeof(entry)) == -E_FAULT);
    CHECK(sc3(SYS_GETDENTS,dir,&entry,sizeof(entry)) == sizeof(entry));
    CHECK(equal(entry.name,"data") && entry.type == DT_REG);
    close(dir);
    dir = open("/", O_DIRECTORY);
    CHECK(read(dir,b,1) == -E_ISDIR);
    bool bin = false, tmp = false, dev = false;
    while (sc3(SYS_GETDENTS,dir,&entry,sizeof(entry)) == sizeof(entry)) {
        bin |= equal(entry.name,"bin"); tmp |= equal(entry.name,"tmp"); dev |= equal(entry.name,"dev");
    }
    CHECK(bin && tmp && dev); close(dir);
    /* A full fd table must reject truncation without modifying the file. */
    long held[FD_COUNT]; int count = 0;
    while ((fd = open("/dev/null", O_RDWR)) >= 0) held[count++] = fd;
    CHECK(fd == -E_MFILE && count == FD_COUNT - 3);
    CHECK(open("/tmp/data", O_WRONLY | O_TRUNC) == -E_MFILE);
    int32_t ends[2]; CHECK(pipe(ends) == -E_MFILE);
    while (count) close(held[--count]);
    fd = open("/tmp/data", O_RDONLY);
    CHECK(seek(fd,0,SEEK_END) == 4098); close(fd);
    puts("M6 FILE/PATH PASS\n");
}

static void pipes(void)
{
    int32_t p[2]; char data[1200], b[300];
    for (size_t i = 0; i < sizeof(data); ++i) data[i] = (char)(i % 97);
    CHECK(pipe((void *)0x500000) == -E_FAULT);
    CHECK(pipe(p) == 0);
    CHECK(read(p[1],b,1) == -E_BADF && write(p[0],b,1) == -E_BADF);
    CHECK(seek(p[0],0,SEEK_SET) == -E_SPIPE);
    long copy = dup(p[1]); CHECK(copy >= 0);
    close(p[1]);
    CHECK(write(copy,"q",1) == 1);
    CHECK(read(p[0],(void *)0x500000,1) == -E_FAULT);
    CHECK(read(p[0],b,1) == 1 && b[0] == 'q');
    close(copy); CHECK(read(p[0],b,1) == 0); close(p[0]);
    CHECK(pipe(p) == 0); close(p[0]);
    CHECK(write(p[1],"x",1) == -E_PIPE); close(p[1]);
    CHECK(pipe(p) == 0);
    long pid = fork(); CHECK(pid >= 0);
    if (!pid) {
        close(p[0]);
        CHECK(write_all(p[1],data,sizeof(data)) == 0);
        close(p[1]); exit(0);
    }
    close(p[1]);
    size_t done = 0; long n;
    while ((n = read(p[0],b,sizeof(b))) > 0) {
        CHECK(done + (size_t)n <= sizeof(data) && !memcmp(b,data + done,(size_t)n));
        done += (size_t)n;
    }
    CHECK(n == 0 && done == sizeof(data)); close(p[0]); joined(pid,0);
    /* Reader sleeps on an empty pipe; child exit closes the final writer. */
    CHECK(pipe(p) == 0); pid = fork(); CHECK(pid >= 0);
    if (!pid) { close(p[0]); sc0(SYS_YIELD); exit(0); }
    close(p[1]); CHECK(read(p[0],b,1) == 0); close(p[0]); joined(pid,0);
    /* Writer sleeps on a full pipe, then wakes when the final reader closes. */
    CHECK(pipe(p) == 0);
    CHECK(write_all(p[1],data,512) == 0);
    pid = fork(); CHECK(pid >= 0);
    if (!pid) { close(p[1]); sc0(SYS_YIELD); close(p[0]); exit(0); }
    close(p[0]); CHECK(write(p[1],"x",1) == -E_PIPE); close(p[1]); joined(pid,0);
    /* Concurrent forked writers share one endpoint description. Closing the
     * parent's copy must not destroy it; every child byte appears once. */
    CHECK(pipe(p) == 0);
    long children[4];
    for (int i = 0; i < 4; ++i) {
        children[i] = fork(); CHECK(children[i] >= 0);
        if (!children[i]) {
            close(p[0]); char byte = (char)i;
            for (int j = 0; j < 24; ++j) {
                CHECK(write(p[1],&byte,1) == 1); sc0(SYS_YIELD);
            }
            close(p[1]); exit(0);
        }
    }
    close(p[1]); unsigned counts[4] = {0};
    while ((n = read(p[0],b,sizeof(b))) > 0)
        for (long i = 0; i < n; ++i) { CHECK((unsigned char)b[i] < 4); ++counts[(unsigned)b[i]]; }
    CHECK(n == 0); close(p[0]);
    for (int i = 0; i < 4; ++i) { CHECK(counts[i] == 24); joined(children[i],0); }
    puts("M6 PIPE PASS\n");
}

static void memory(void)
{
    const long flags = MAP_PRIVATE | MAP_ANONYMOUS;
    long base = sc1(SYS_BRK,0);
    CHECK(base == 0x800000);
    CHECK(sc1(SYS_BRK,base + 4097) == base + 4097);
    char *heap = (char *)base;
    CHECK(heap[0] == 0 && heap[4096] == 0); heap[0] = 17;
    long at = map(0,12288,PROT_READ | PROT_WRITE,flags,-1,0);
    CHECK(at >= 0x10000000);
    char *p = (char *)at; CHECK(p[0] == 0 && p[8192] == 0);
    p[0] = 11; p[8192] = 22;
    CHECK(map(at,4096,PROT_READ,flags | MAP_FIXED_NOREPLACE,-1,0) == -E_EXIST);
    CHECK(map(0,4096,PROT_WRITE | PROT_EXEC,flags,-1,0) == -E_INVAL);
    CHECK(map(0,0,PROT_READ,flags,-1,0) == -E_INVAL);
    CHECK(map(0,-1,PROT_READ,flags,-1,0) == -E_INVAL);
    CHECK(map(0,4096,PROT_READ,flags,0,0) == -E_INVAL);
    CHECK(unmap(at + 1,4096) == -E_INVAL);
    CHECK(unmap(0x10000,4096) == -E_PERM);
    CHECK(unmap(base,4096) == -E_PERM);
    CHECK(unmap(at + 4096,4096) == 0);
    CHECK(write(1,(void *)(at + 4096),1) == -E_FAULT);
    CHECK(p[0] == 11 && p[8192] == 22);
    CHECK(map(at + 4096,4096,PROT_READ | PROT_WRITE,flags | MAP_FIXED_NOREPLACE,-1,0) == at + 4096);
    CHECK(p[4096] == 0);
    long pid = fork(); CHECK(pid >= 0);
    if (!pid) {
        CHECK(sc1(SYS_BRK,0) == base + 4097 && heap[0] == 17 && p[0] == 11);
        heap[0] = 33; p[0] = 44;
        CHECK(unmap(at,12288) == 0 && sc1(SYS_BRK,base) == base);
        exit(0);
    }
    joined(pid,0); CHECK(heap[0] == 17 && p[0] == 11);
    CHECK(unmap(at,12288) == 0 && unmap(at,12288) == 0);
    CHECK(sc1(SYS_BRK,base) == base && write(1,heap,1) == -E_FAULT);
    CHECK(sc1(SYS_BRK,base - 1) == -E_NOMEM);
    /* Prefix and suffix cuts retain the untouched bytes and reservations. */
    at = map(0,16384,PROT_READ | PROT_WRITE,flags,-1,0); CHECK(at > 0);
    CHECK(unmap(at,4096) == 0 && unmap(at + 12288,4096) == 0);
    CHECK(map(at,4096,PROT_READ,flags | MAP_FIXED_NOREPLACE,-1,0) == at);
    CHECK(unmap(at,16384) == 0);
    at = map(0,4096,PROT_NONE,flags,-1,0); CHECK(at > 0);
    CHECK(write(1,(void *)at,1) == -E_FAULT && unmap(at,4096) == 0);
    long fd = open("/link",O_RDONLY); CHECK(fd >= 0);
    at = map(0,8192,PROT_READ | PROT_WRITE,MAP_PRIVATE,fd,0); CHECK(at > 0);
    CHECK(seek(fd,0,SEEK_CUR) == 0);
    p = (char *)at; CHECK(!memcmp(p,"abcdef",6) && p[4096] == 0);
    p[0] = 'z'; char c;
    CHECK(read(fd,&c,1) == 1 && c == 'a'); close(fd);
    CHECK(p[0] == 'z' && unmap(at,8192) == 0);
    at = map(0,4096,PROT_READ,flags,-1,0); CHECK(at > 0);
    fd = open("/link",O_RDONLY); CHECK(fd >= 0);
    CHECK(read(fd,(void *)at,1) == -E_FAULT); close(fd);
    pid = fork(); CHECK(pid >= 0);
    if (!pid) { *(volatile char *)at = 1; exit(91); }
    joined(pid,-1);
    pid = fork(); CHECK(pid >= 0);
    if (!pid) {
        /* exec must discard inherited VMAs and pages, while retaining fds. */
        char *args[] = {"echo","exec drops mappings",0};
        exec("/bin/echo",args); exit(94);
    }
    joined(pid,0); CHECK(unmap(at,4096) == 0);
    puts("M6 VM PASS\n");
}

static void shell(const char *line, long expected)
{
    long pid = fork(); CHECK(pid >= 0);
    if (!pid) {
        char *argv[] = {"sh", "-c", (char *)line, 0};
        exec("/bin/sh",argv); exit(92);
    }
    joined(pid,expected);
}

static void shells(void)
{
    shell("echo hello world|upper>/tmp/out",0);
    shell("cat</tmp/out|cat|cat>/tmp/copy",0);
    shell("echo again>>/tmp/copy",0);
    char buffer[64] = {0};
    long fd = open("/tmp/copy",O_RDONLY); CHECK(fd >= 0);
    CHECK(read(fd,buffer,sizeof(buffer)) == 18);
    /* Check exact bytes; EOF does not append a terminator for read callers. */
    CHECK(!memcmp(buffer,"HELLO WORLD\nagain\n",18));
    close(fd);
    shell("cat</missing|upper>/tmp/error",1);
    shell("echo x|",2);
    /* Exercise the shell's stdin loop and parent-side cd as well as -c. */
    fd = open("/tmp/script",O_CREAT | O_TRUNC | O_RDWR); CHECK(fd >= 0);
    const char *script = "cd /tmp\necho script>script-out\ncat<script-out|upper>script-copy\n";
    CHECK(write_all(fd,script,strlen(script)) == 0 && seek(fd,0,SEEK_SET) == 0);
    long script_pid = fork(); CHECK(script_pid >= 0);
    if (!script_pid) {
        CHECK(dup2(fd,0) == 0); close(fd);
        char *args[] = {"sh",0}; exec("/bin/sh",args); exit(95);
    }
    close(fd); joined(script_pid,0);
    fd = open("/tmp/script-copy",O_RDONLY); CHECK(fd >= 0);
    CHECK(read(fd,buffer,sizeof(buffer)) == 7 && !memcmp(buffer,"SCRIPT\n",7)); close(fd);
    CHECK(chdir("/bin") == 0);
    long pid = fork(); CHECK(pid >= 0);
    if (!pid) { char *args[] = {"echo","relative exec",0}; exec("echo",args); exit(93); }
    joined(pid,0); CHECK(chdir("/") == 0);
    puts("M6 SHELL PASS\n");
}

int main(void)
{
    files(); pipes(); memory(); shells();
    puts("M6 USER PASS\n");
    return 0;
}
