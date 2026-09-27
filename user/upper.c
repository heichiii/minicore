#include "lib.h"
int main(void)
{
    char buffer[256];
    long n;
    while ((n = read(0, buffer, sizeof(buffer))) > 0) {
        for (long i = 0; i < n; ++i)
            if (buffer[i] >= 'a' && buffer[i] <= 'z') buffer[i] -= 'a' - 'A';
        if (write_all(1, buffer, (size_t)n)) return 1;
    }
    return n < 0 ? 1 : 0;
}
