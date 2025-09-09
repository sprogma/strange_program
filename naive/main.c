#include "stdio.h"
#include "malloc.h"
#include "search.h"
#include "string.h"


int forward_cmp(const void *a, const void *b)
{
    const char *x = a;
    const char *y = b;
    return strcmp(x, y);
}


int backward_cmp(const void *a, const void *b)
{
    const char *x = a;
    const char *y = b;
    return strcmp(x, y);
}


int main(int argc, const char **argv)
{
    if (argc != 3)
    {
        fprintf(stderr, "get %d input files instead of 2\n", argc - 1);
        return 1;
    }

    FILE *fi = fopen(argv[1], "rb");
    if (fi == NULL)
    {
        fprintf(stderr, "Cannot find file <%s>\n", argv[1]);
        return 1;
    }

    FILE *fo = fopen(argv[2], "wb");
    if (fo == NULL)
    {
        fprintf(stderr, "Cannot find file <%s>\n", argv[1]);
        return 1;
    }
    
    size_t file_size;
    fseek(fi, 0L, SEEK_END);
    file_size = ftell(fi);
    /* read all file into memory */
    char *content = malloc(file_size + 1);
    rewind(fi);
    fread(content, file_size, 1, fi);
    content[file_size] = 0;

    size_t index_len = 0;
    {
        char *str = content;
        while ((str = strchr(str, '\n')) != NULL)
        {
            index_len++;
            str++;
        }
    }
    char **index = malloc(sizeof(char *) * index_len);
    index_len = 0;
    {
        char *str = content;
        if (str[1] != '\n' && str[1] != '\0')
        {
            index[index_len++] = str;
        }
        while ((str = strchr(str, '\n')) != NULL)
        {
            *str = 0;
            if (str[1] != '\n' && str[1] != '\0')
            {
                index[index_len++] = str + 1;
            }
            str++;
        }
    }

    printf("read %d lines.\n", (int)index_len);

    /* 1. alphabetic */
    {
        rewind(fi);
        qsort(index, index_len, sizeof(*index), forward_cmp);
        for (size_t i = 0; i < index_len; ++i)
        {
            printf("%p\n", index[i]);
            fputs(index[i], fo);
            putc('\n', fo);
        }
    }
    
    /* 2. backwards alphabetic */
    {
        rewind(fi);
        qsort(index, index_len, sizeof(*index), backward_cmp);
        for (size_t i = 0; i < index_len; ++i)
        {
            fputs(index[i], fo);
            putc('\n', fo);
        }
    }

    /* 3. copying */
    fwrite(content, file_size, 1, fo);

    fclose(fi);
    fclose(fo);

    return 0;
}
