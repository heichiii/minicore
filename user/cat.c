#include "lib.h"
int main(int argc, char **argv)
{
    char buffer[256];
    long fd = argc > 1 ? open(argv[1], O_RDONLY) : 0;
    if (fd < 0) return 1;
    long n;
    while ((n = read(fd, buffer, sizeof(buffer))) > 0)
        if (write_all(1, buffer, (size_t)n)) return 1;
    if (fd) close(fd);
    return n < 0 ? 1 : 0;
}
