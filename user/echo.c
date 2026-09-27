#include "lib.h"
int main(int argc, char **argv)
{
    for (int i = 1; i < argc; ++i) {
        if (i > 1 && puts(" ")) return 1;
        if (puts(argv[i])) return 1;
    }
    return puts("\n") ? 1 : 0;
}
