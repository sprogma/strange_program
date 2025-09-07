#include "stdio.h"


int main(int argc, const char **argv)
{
    if (argc != 3)
    {
        fprintf(stderr, "get %d input files instead of 2\n", argc - 1);
        return 1;
    }

    FILE *fi = fopen(argv[1], "r");
    if (fi == NULL)
    {
        fprintf(stderr, "Cannot find file <%s>\n", argv[1]);
        return 1;
    }

    FILE *fo = fopen(argv[2], "w");
    if (fo == NULL)
    {
        fprintf(stderr, "Cannot find file <%s>\n", argv[1]);
        return 1;
    }

    int n = 4096;
    while (n == 4096)
    {
        char buf[4096];
        n = fread(buf, 1, 4096, fi);
        fwrite(buf, 1, n, fo);
    }

    fclose(fi);
    fclose(fo);

    return 0;
}
