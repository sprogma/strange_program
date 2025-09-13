#include "stdio.h"
#include "errno.h"

#ifdef WIN32
    #include "windows.h"    
#else
    #include "fcntl.h"
    #include "unistd.h"
    #include "sys/stat.h"
#endif

#include "bench.h"

int compare_string_strcmp(const void *a, const void *b) 
{
    const char *s1 = *(const char **)a;
    const char *s2 = *(const char **)b;
    return strcmp(s1, s2);
}

int compare_string_strcmp_and_check_letter(const void *a, const void *b) 
{
    const char *s1 = *(const char **)a;
    const char *s2 = *(const char **)b;
    return (*s1 != *s2 ? *s1 - *s2 : strcmp(s1, s2));
}

void radix(char **s, )

BENCH(sort_string)
{
    char *buffer;
    char **index;
    size_t strings = 0;
    size_t min_string_size = 0;
    size_t max_string_size = 0;
    size_t full_alphabet = 1;
    BENCH_OPTION("1e3 x 1e2") { strings = 1000; min_string_size = max_string_size = 100; }
    BENCH_OPTION("1e3 x 1e5") { strings = 1000; min_string_size = max_string_size = 100000; }
    BENCH_OPTION("1e3 x 1e5 [ab]") { strings = 1000; min_string_size = max_string_size = 100000; full_alphabet = 0; }
    BENCH_OPTION("1e6 x 1e2") { strings = 1000000; min_string_size = max_string_size = 100; }
    BENCH_OPTION("1e6 x 5e2") { strings = 1000000; min_string_size = max_string_size = 500; }
    BENCH_OPTION("5e6 x 5e2") { strings = 5000000; min_string_size = max_string_size = 50; }
    BENCH_OPTION("5e6 x 5e2 [ab]") { strings = 5000000; min_string_size = max_string_size = 50; full_alphabet = 0; }
    BENCH_OPTION("1e3 x [1e2-1e3]") { strings = 1000; min_string_size = 100; max_string_size = 1000; }
    BENCH_OPTION("1e3 x [1e4-1e5]") { strings = 1000; min_string_size = 1000; max_string_size = 100000; }
    BENCH_OPTION("1e5 x [1e2-1e3]") { strings = 100000; min_string_size = 100; max_string_size = 1000; }

    srand(1);
    buffer = malloc(strings * (max_string_size + 1));
    index = malloc(sizeof(*index) * (strings + 1));
    {
        size_t add_zero = 0, len = 0, id = 0, ind = 0, add_strings = strings;
        while (add_strings > 0 || (add_strings == 0 && len > 0))
        {
            if (len == 0)
            {
                if (add_zero)
                {
                    buffer[id++] = '\0';
                }
                add_strings--;
                index[ind++] = buffer + id;
                len = rand() % (max_string_size - min_string_size + 1) + min_string_size;
                add_zero = 1;
            }
            else
            {
                if (full_alphabet)
                {   
                    buffer[id++] = rand() % (128 - ' ') + ' ';
                }
                else
                {
                    buffer[id++] = rand() % 2 + 'a';
                }
                len--;
            }
        }
        buffer[id++] = '\0';
    }



    printf("Run %d %d %d ?\n", (int)strings, (int)min_string_size, (int)max_string_size);

    BENCH_VARIANT("qsort + strcmp")
    {
        BENCH_RUN(0, 1)
        {
            BENCH_START
            {
                qsort(index, strings, sizeof(index), compare_string_strcmp);
            }
            BENCH_END
            free(buffer);
            free(index);
        }
    }

    BENCH_VARIANT("qsort + strcmp + check first letter")
    {
        BENCH_RUN(0, 1)
        {
            BENCH_START
            {
                qsort(index, strings, sizeof(index), compare_string_strcmp_and_check_letter);
            }
            BENCH_END
            free(buffer);
            free(index);
        }
    }

    BENCH_VARIANT("radix sort")
    {
        BENCH_RUN(0, 1)
        {
            BENCH_START
            {
                radix(index, strings);
            }
            BENCH_END
            free(buffer);
            free(index);
        }
    }
}
