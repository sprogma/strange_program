#include "stdio.h"
#include "errno.h"
#include "inttypes.h"
#include "omp.h"

#ifdef WIN32
    #include "windows.h"    
#else
    #include "fcntl.h"
    #include "unistd.h"
    #include "sys/stat.h"
#endif

#define BENCH_FUNCTION sort_string
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

void radix_LSD(char **s, size_t length)
{
    char **temporary_start = malloc(sizeof(*temporary_start) * length);
    char **temporary_end = malloc(sizeof(*temporary_end) * length);

    char **mem_ptr_temporary_start = temporary_start;
    char **mem_ptr_temporary_end = temporary_end;
    
    char **start = s;
    char **end = malloc(sizeof(*end) * length);
    size_t max_length = 0;
    /* calculate end for each string */
    for (size_t i = 0; i < length; ++i)
    {
        size_t res = strlen(start[i]);
        end[i] = start[i] + res;
        if (max_length < res)
        {
            max_length = res;
        }
    }
    size_t bs[256];
    for (size_t item = max_length - 1; item < max_length; --item)
    {
        memset(bs, 0, sizeof(bs));
        /* count elements */
        // printf("new cycle item=%d\n", (int)item);
        for (size_t id = 0; id < length; ++id)
        {
            unsigned char res = 0;
            if ((size_t)(end[id] - start[id]) > item)
            {
                res = start[id][item];
            }
            // printf("%d\n", res);
            bs[res]++;
        }
        /* calculate shifts */
        for (size_t i = 1; i < sizeof(bs) / sizeof(*bs); ++i)
        {
            bs[i] += bs[i - 1];
            // printf("TABLE: bs[%d] = %d\n", (int)i, (int)bs[i]);
        }
        /* sort elements */
        for (size_t id = length - 1; id < length; --id)
        {
            unsigned char res = 0;
            if ((size_t)(end[id] - start[id]) > item)
            {
                res = start[id][item];
            }
            // printf("res = %d\n", res);
            bs[res]--;
            // printf("write: %d -> %d\n", (int)id, (int)bs[res]);
            temporary_start[bs[res]] = start[id];
            temporary_start[bs[res]] = start[id];
        }
        /* swap fields */
        {
            char **tmp;
            
            tmp = start;
            start = temporary_start;
            temporary_start = tmp;
            
            tmp = end;
            end = temporary_end;
            temporary_end = tmp;
        }
    }
    /* right data is in start PTR. */
    if (start != s)
    {
        memcpy(s, start, sizeof(*s) * length);
    }
    free(mem_ptr_temporary_start);
    free(mem_ptr_temporary_end);
    // printf("Min string: %s\n", start[0]);
}


void radix_MSD_no_fallback_recurse(char **s, char **e, char **tmp_s, char **tmp_e, size_t letter, size_t length)
{
    // printf("Level: %d:%d\n", (int)letter, (int)length);
    size_t bs[256] = {};
    /* create empty baskets */
    /* count first letters */
    for (size_t i = 0; i < length; ++i)
    {
        unsigned char res = 0;
        if ((size_t)(e[i] - s[i]) > letter)
        {
            res = s[i][letter];
        }
        bs[res]++;
    }
    /* calculate offsets */
    for (size_t i = 1; i < 256; ++i)
    {
        bs[i] += bs[i - 1];
    }
    /* sort elements */
    for (size_t i = length - 1; i < length; --i)
    {
        unsigned char res = 0;
        if ((size_t)(e[i] - s[i]) > letter)
        {
            res = s[i][letter];
        }
        bs[res]--;
        tmp_s[bs[res]] = s[i];
        tmp_e[bs[res]] = e[i];
    }
    /* copy back first group */
    {
        memcpy(s, tmp_s, sizeof(*e) * bs[1]);
        memcpy(e, tmp_e, sizeof(*e) * bs[1]);
    }
    /* call recursive sorts, if block */
    for (size_t i = 1; i < 256; ++i)
    {
        size_t start = bs[i];
        size_t end = (i + 1 == 256 ? length : bs[i + 1]);
        if (end - start == 0) 
        { }
        else if (end - start == 1)
        {
            s[start] = tmp_s[start];
            e[start] = tmp_e[start];
        }
        else
        {
            radix_MSD_no_fallback_recurse(tmp_s + start, tmp_e + start, s + start, e + start, letter + 1, end - start);
            memcpy(s + start, tmp_s + start, sizeof(*s) * (end - start));
            memcpy(e + start, tmp_e + start, sizeof(*e) * (end - start));
        }
    }
}


void radix_MSD_no_fallback(char **s, size_t length)
{
    /* allocate temporary buffer */
    char **e = malloc(sizeof(*e) * length);
    char **tmp_s = malloc(sizeof(*tmp_s) * length);
    char **tmp_e = malloc(sizeof(*tmp_e) * length);
    for (size_t i = 0; i < length; ++i)
    {
        e[i] = s[i] + strlen(s[i]);
    }
    radix_MSD_no_fallback_recurse(s, e, tmp_s, tmp_e, 0, length);
    free(tmp_s);
    free(tmp_e);
}


struct radix_MSD_array
{
    char *s;
    char *e;
};


size_t radix_MSD_qsort_compare_offset = 0;
int radix_MSD_qsort_compare_string_strcmp(const void *a, const void *b) 
{
    const char *s1 = *(const char **)a;
    const char *s2 = *(const char **)b;
    return strcmp(s1 + radix_MSD_qsort_compare_offset, s2 + radix_MSD_qsort_compare_offset);
}

void radix_MSD_recurse(struct radix_MSD_array *s, struct radix_MSD_array *tmp, size_t letter, size_t length)
{
    // printf("Level: %d:%d\n", (int)letter, (int)length);
    size_t bs[256] = {};
    /* create empty baskets */
    /* count first letters */
    for (size_t i = 0; i < length; ++i)
    {
        unsigned char res = 0;
        if ((size_t)(s[i].e - s[i].s) > letter)
        {
            res = s[i].s[letter];
        }
        bs[res]++;
    }
    /* calculate offsets */
    for (size_t i = 1; i < 256; ++i)
    {
        bs[i] += bs[i - 1];
    }
    /* sort elements */
    for (size_t i = length - 1; i < length; --i)
    {
        unsigned char res = 0;
        if ((size_t)(s[i].e - s[i].s) > letter)
        {
            res = s[i].s[letter];
        }
        bs[res]--;
        tmp[bs[res]] = s[i];
    }
    /* copy back first group */
    {
        memcpy(s, tmp, sizeof(*s) * bs[1]);
    }
    /* call recursive sorts, if block */
    for (size_t i = 1; i < 256; ++i)
    {
        size_t start = bs[i];
        size_t end = (i + 1 == 256 ? length : bs[i + 1]);
        if (end - start == 0) 
        { }
        else if (end - start == 1)
        {
            s[start] = tmp[start];
        }
        else if (end - start < 8192)
        {
            radix_MSD_qsort_compare_offset = letter + 1;
            qsort(s + start, end - start, sizeof(*s), radix_MSD_qsort_compare_string_strcmp);
        }
        else
        {
            radix_MSD_recurse(tmp + start, s + start, letter + 1, end - start);
            memcpy(s + start, tmp + start, sizeof(*s) * (end - start));
        }
    }
}


void radix_MSD(char **index, size_t length)
{
    /* allocate temporary buffer */
    struct radix_MSD_array *s = malloc(sizeof(*s) * length);
    struct radix_MSD_array *tmp = malloc(sizeof(*tmp) * length);
    for (size_t i = 0; i < length; ++i)
    {
        s[i].s = index[i];
        s[i].e = index[i] + strlen(index[i]);
    }
    radix_MSD_recurse(s, tmp, 0, length);
    for (size_t i = 0; i < length; ++i)
    {
        index[i] = s[i].s;
    }
    free(tmp);
    free(s);
}


size_t radix_MSD_int16_recurse_bs[128][256*256] = {};
void radix_MSD_int16_recurse(struct radix_MSD_array *s, struct radix_MSD_array *tmp, size_t letter, size_t length)
{
    size_t *bs = radix_MSD_int16_recurse_bs[letter];
    memset(bs, 0, sizeof(*bs) * 256 * 256);
    /* create empty baskets */
    /* count first letters */
    for (size_t i = 0; i < length; ++i)
    {
        unsigned char resA = 0, resB = 0;
        if ((size_t)(s[i].e - s[i].s) > 2 * letter + 1)
        {
            resB = s[i].s[2 * letter + 1];
        }
        if ((size_t)(s[i].e - s[i].s) > 2 * letter)
        {
            resA = s[i].s[2 * letter];
        }
        uint16_t res = (resA << 8) | resB;
        bs[res]++;
    }
    /* calculate offsets */
    for (size_t i = 1; i < 256*256; ++i)
    {
        bs[i] += bs[i - 1];
    }
    /* sort elements */
    for (size_t i = length - 1; i < length; --i)
    {
        unsigned char resA = 0, resB = 0;
        if ((size_t)(s[i].e - s[i].s) > 2 * letter + 1)
        {
            resB = s[i].s[2 * letter + 1];
        }
        if ((size_t)(s[i].e - s[i].s) > 2 * letter)
        {
            resA = s[i].s[2 * letter];
        }
        uint16_t res = (resA << 8) | resB;
        bs[res]--;
        tmp[bs[res]] = s[i];
    }
    /* copy back first group */
    /* call recursive sorts, if block */
    for (size_t i = 0; i < 256*256; ++i)
    {
        size_t start = bs[i];
        size_t end = (i + 1 == 256*256 ? length : bs[i + 1]);
        if ((i & 0xFF) == 0 && end - start > 1)
        {
            memcpy(s + start, tmp + start, sizeof(*s) * (end - start));
            continue;
        }
        if (end - start == 0) 
        { }
        else if (end - start == 1)
        {
            s[start] = tmp[start];
        }
        else if (end - start < 8192)
        {
            radix_MSD_qsort_compare_offset = letter + 1;
            qsort(s + start, end - start, sizeof(*s), radix_MSD_qsort_compare_string_strcmp);
        }
        // else if (end - start < 512)
        // {
        //     radix_MSD_recurse(tmp + start, s + start, 2 * letter + 1, end - start);
        //     memcpy(s + start, tmp + start, sizeof(*s) * (end - start));
        // }
        else
        {
            radix_MSD_int16_recurse(tmp + start, s + start, letter + 1, end - start);
            memcpy(s + start, tmp + start, sizeof(*s) * (end - start));
        }
    }
}


void radix_MSD_int16(char **index, size_t length)
{
    /* allocate temporary buffer */
    struct radix_MSD_array *s = malloc(sizeof(*s) * length);
    struct radix_MSD_array *tmp = malloc(sizeof(*tmp) * length);
    for (size_t i = 0; i < length; ++i)
    {
        s[i].s = index[i];
        s[i].e = index[i] + strlen(index[i]);
    }
    if (length < 2048)
    {   
        radix_MSD_recurse(s, tmp, 0, length);
    }
    else
    {
        radix_MSD_int16_recurse(s, tmp, 0, length);
    }
    for (size_t i = 0; i < length; ++i)
    {
        index[i] = s[i].s;
    }
    free(tmp);
    free(s);
}



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
    BENCH_OPTION("5e6 x 5e1") { strings = 5000000; min_string_size = max_string_size = 50; }
    BENCH_OPTION("5e6 x 5e1 [ab]") { strings = 5000000; min_string_size = max_string_size = 50; full_alphabet = 0; }
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
                    buffer[id++] = rand() % (127 - ' ') + ' ';
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


    
    BENCH_VARIANT("builtin + strcmp")
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

    BENCH_VARIANT("builtin + check first byte")
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

    BENCH_VARIANT("radix LSD")
    {
        BENCH_RUN(0, 1)
        {
            BENCH_START
            {
                radix_LSD(index, strings);
            }
            BENCH_END
            free(buffer);
            free(index);
        }
    }
    

    BENCH_VARIANT("radix MSD no fallback")
    {
        BENCH_RUN(0, 1)
        {
            BENCH_START
            {
                radix_MSD_no_fallback(index, strings);
            }
            BENCH_END
            free(buffer);
            free(index);
        }
    }
    

    BENCH_VARIANT("radix MSD")
    {
        BENCH_RUN(0, 1)
        {
            BENCH_START
            {
                radix_MSD(index, strings);
            }
            BENCH_END
            free(buffer);
            free(index);
        }
    }
    

    BENCH_VARIANT("radix MSD int16")
    {
        BENCH_RUN(0, 1)
        {
            BENCH_START
            {
                radix_MSD_int16(index, strings);
            }
            BENCH_END
            free(buffer);
            free(index);
        }
    }
}
