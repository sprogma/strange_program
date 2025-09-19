#define __USE_MINGW_ANSI_STDIO 1
#include "stdio.h"
#include "malloc.h"
#include "search.h"
#include "string.h"
#include "assert.h"
#include "sys/stat.h"
#include "grapheme.h"


int compare_pointers(const void *a, const void *b)
{
    const char *x = *(const char **)a;
    const char *y = *(const char **)b;
    return x - y;
}


int forward_strcoll_cmp(const void *a, const void *b)
{
    const char *x = *(const char **)a;
    const char *y = *(const char **)b;
    return strcoll(x, y);
}


char *next_utf8_token(char *s)
{
    if (*s == '\0')
    {
        return s;
    }
    /* 1024 * 1024 is 'infinity' */
    size_t length = grapheme_next_character_break_utf8(s, 1024 * 1024);
    /* using libgrapheme */
    return s + length;
}


int fill_line_array(char ***plines, size_t *length, char *content)
{
    char *ptr = content;
    char *line_begin = content;
    size_t lines_id = 0;
    size_t lines_alloc = 128;
    char **lines = *plines = malloc(sizeof(*lines) * lines_alloc);
    if (lines == NULL)
    {
        return 2;
    }
    while (1)
    {
        if (lines_id == lines_alloc)
        {
            lines_alloc *= 2;
            lines = realloc(lines, sizeof(*lines) * lines_alloc);
            if (lines == NULL)
            {
                free(content);
                free(*plines);
                return 2;
            }
            *plines = lines;
        }
        char *next = next_utf8_token(ptr);
        if (next == ptr || (next - ptr == 1 && *ptr == '\n') || (next - ptr == 2 && *ptr == '\r' && ptr[1] == '\n'))
        {
            lines[lines_id++] = line_begin;
            *ptr = '\0';
            if (next == ptr)
            {
                break;
            }
            ptr = next;
            line_begin = ptr;
        }
        else
        {
            ptr = next;
        }
    }
    
    *length = lines_id;

    for (size_t i = 0; i < lines_id; ++i)
    {
        assert(lines[i] != NULL);
    }
    
    return 0;
}


int read_file_as_lines(const char *filename, char ***plines, size_t *length)
{
    struct stat st;
    stat(filename, &st);
    size_t size = st.st_size;
    /* read all file */
    FILE *f = fopen(filename, "rb");
    if (f == NULL)
    {
        return 1;
    }
    char *content = calloc(1, 4 + size);
    if (content == NULL)
    {
        return 2;
    }
    if (fread(content, size, 1, f) != 1)
    {
        return 3;
    }
    content[size] = 0;
    fclose(f);
    
    fill_line_array(plines, length, content);
    
    return 0;
}


void reverse_string(char *s)
{
    size_t len = strlen(s);
    char *ptr = s;
    while (1)
    {
        char *next = next_utf8_token(ptr);
        /* swap grapheme cluster */
        size_t cluster_len = next - ptr;
        for (size_t i = 0; i + i < cluster_len; ++i)
        {
            size_t tmp = ptr[i];
            ptr[i] = ptr[cluster_len - i - 1];
            ptr[cluster_len - i - 1] = tmp;
        }
        if (ptr == next)
        {
            break;
        }
        ptr = next;
    }
    /* swap all string */
    for (size_t i = 0; i + i < len; ++i)
    {
        size_t tmp = s[i];
        s[i] = s[len - i - 1];
        s[len - i - 1] = tmp;
    }
}


void print_lines(char **lines, size_t length, FILE *file)
{
    for (size_t i = 0; i < length; ++i)
    {
        fputs(lines[i], file);
        #ifdef _WIN32
            fputc('\r', file);
        #endif
        fputc('\n', file);
    }
}


int main(int argc, const char **argv)
{
    if (argc != 3)
    {
        fprintf(stderr, "get %d input files instead of 2\n", argc - 1);
        return 1;
    }
    
    
    FILE *fo = fopen(argv[2], "wb");
    if (fo == NULL)
    {
        fprintf(stderr, "Cannot find file <%s>\n", argv[2]);
        return 1;
    }

    char **lines;
    size_t length;
    int err;

    if ((err = read_file_as_lines(argv[1], &lines, &length)) != 0)
    {
        printf("ReadFileAsLines error %d\n", err);
        return 1;
    }

    printf("read %zu lines.\n", length);

    

    qsort(lines, length, sizeof(*lines), forward_strcoll_cmp);
    
    print_lines(lines, length, fo);



    for (size_t i = 0; i < length; ++i)
    {
        reverse_string(lines[i]);
    }

    qsort(lines, length, sizeof(*lines), forward_strcoll_cmp);

    for (size_t i = 0; i < length; ++i)
    {
        reverse_string(lines[i]);
    }

    print_lines(lines, length, fo);



    
    qsort(lines, length, sizeof(*lines), compare_pointers);

    print_lines(lines, length, fo);


    fclose(fo);

    return 0;
}
