#define __USE_MINGW_ANSI_STDIO 1
#include "stdio.h"
#include "errno.h"
#include "inttypes.h"
#include "omp.h"

#ifdef _WIN32
    #include "windows.h"
#else
    #include "fcntl.h"
    #include "unistd.h"
    #include "sys/stat.h"
#endif

#define BENCH_FUNCTION sort_string
#include "bench.h"







#define TEST_BUILTIN
#define TEST_QSORT
#define TEST_MERGESORT
// #define TEST_RADIX_LSD
#define TEST_RADIX_MSD
#define TEST_RADIX_MSD_CPU
#define TEST_BRUST
// #define TEST_SHELL

/* commented out becouse of speed */
// #define TEST_BATCHER_ODDEVEN
/* not implemented */
// #define TEST_RADIX_MSD_GPU








int compare_size(const void *a, const void *b)
{
    const size_t s1 = *(const size_t *)a;
    const size_t s2 = *(const size_t *)b;
    return (s1 > s2 ? 1 : s1 == s2 ? 0 : -1);
}

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
    free(e);
    free(tmp_s);
    free(tmp_e);
}

struct radix_MSD_array
{
    char *s;
    size_t e;
};

void radix_MSD_no_strlen_fallback_recurse(struct radix_MSD_array *s, struct radix_MSD_array *tmp, size_t letter, size_t length)
{
    size_t bs[256] = {};
    /* create empty baskets */
    /* count first letters */
    for (size_t i = 0; i < length; ++i)
    {
        unsigned char res = 0;
        if (s[i].s[letter] == 0)
        {
            s[i].e = 1;
        }
        if (!s[i].e)
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
        if (!s[i].e)
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
        else
        {
            radix_MSD_no_strlen_fallback_recurse(tmp + start, s + start, letter + 1, end - start);
            memcpy(s + start, tmp + start, sizeof(*s) * (end - start));
        }
    }
}


void radix_MSD_no_strlen_fallback(char **index, size_t length)
{
    /* allocate temporary buffer */
    struct radix_MSD_array *s = malloc(sizeof(*s) * length);
    struct radix_MSD_array *tmp = malloc(sizeof(*tmp) * length);
    for (size_t i = 0; i < length; ++i)
    {
        s[i].s = index[i];
        s[i].e = 0;
    }
    radix_MSD_no_strlen_fallback_recurse(s, tmp, 0, length);
    free(tmp);
    for (size_t i = 0; i < length; ++i)
    {
        index[i] = s[i].s;
    }
    free(s);
}



size_t radix_MSD_qsort_compare_offset = 0;
int radix_MSD_qsort_compare_string_strcmp(const void *a, const void *b)
{
    const struct radix_MSD_array s1 = *(const struct radix_MSD_array *)a;
    const struct radix_MSD_array s2 = *(const struct radix_MSD_array *)b;
    return strcmp(s1.s + radix_MSD_qsort_compare_offset, s2.s + radix_MSD_qsort_compare_offset);
}

void radix_MSD_recurse(struct radix_MSD_array *s, struct radix_MSD_array *tmp, size_t letter, size_t length)
{
    // printf("Level: %d:%d\n", (int)letter, (int)length);
    size_t bs[256] = {};
    /* create empty baskets */
    /* count first letters */
    for (size_t i = 0; i < length; ++i)
    {
        if (s[i].s[letter] == 0)
        {
            s[i].e = -1;
        }
        unsigned char res = 0;
        if (s[i].e != (size_t)-1)
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
        if (s[i].e != (size_t)-1)
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
        else if (end - start < 1024)
        {
            radix_MSD_qsort_compare_offset = letter;
            memcpy(s + start, tmp + start, sizeof(*s) * (end - start));
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
    if (length < 1024)
    {
        qsort(index, length, sizeof(*index), compare_string_strcmp);
        return;
    }
    /* allocate temporary buffer */
    struct radix_MSD_array *s = malloc(sizeof(*s) * length);
    struct radix_MSD_array *tmp = malloc(sizeof(*tmp) * length);
    for (size_t i = 0; i < length; ++i)
    {
        s[i].s = index[i];
        s[i].e = 0;
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
        if (s[i].e != (size_t)-1)
        {
            resA = s[i].s[letter];
        }
        if (s[i].e != (size_t)-1 && resA != 0)
        {
            resB = s[i].s[letter + 1];
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
        if (s[i].s[letter] == 0)
        {
            s[i].e = -1;
        }
        if (s[i].e != (size_t)-1)
        {
            resB = s[i].s[letter + 1];
        }
        if (s[i].s[letter + 1] == 0)
        {
            s[i].e = -1;
        }
        if (s[i].e != (size_t)-1)
        {
            resA = s[i].s[letter];
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
        else if (end - start < 1024)
        {
            memcpy(s + start, tmp + start, sizeof(*s) * (end - start));
            qsort(s + start, end - start, sizeof(*s), radix_MSD_qsort_compare_string_strcmp);
        }
        // else if (end - start < 512)
        // {
        //     radix_MSD_recurse(tmp + start, s + start, 2 * letter + 1, end - start);
        //     memcpy(s + start, tmp + start, sizeof(*s) * (end - start));
        // }
        else
        {
            radix_MSD_int16_recurse(tmp + start, s + start, letter + 2, end - start);
            memcpy(s + start, tmp + start, sizeof(*s) * (end - start));
        }
    }
}


void radix_MSD_int16(char **index, size_t length)
{
    if (length < 1024)
    {
        qsort(index, length, sizeof(*index), compare_string_strcmp);
        return;
    }
    /* allocate temporary buffer */
    struct radix_MSD_array *s = malloc(sizeof(*s) * length);
    struct radix_MSD_array *tmp = malloc(sizeof(*tmp) * length);
    for (size_t i = 0; i < length; ++i)
    {
        s[i].s = index[i];
        s[i].e = 0;
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


size_t radix_MSD_cpu_block_bs[128][256];
size_t radix_MSD_cpu_block_starts[128];
void radix_MSD_cpu_recurse19(struct radix_MSD_array *s, struct radix_MSD_array *tmp, size_t letter, size_t length)
{
    /* 1. if input is less than 16384 */
    if (length <= 524288) // 65536)
    {
        radix_MSD_no_strlen_fallback_recurse(s, tmp, letter, length);
        return;
    }
    /* 2. else - split array on blocks for each thread */
    size_t blocks_count = omp_get_max_threads(); //omp_get_num_threads();
    size_t *block_starts = radix_MSD_cpu_block_starts;
    block_starts[0] = 0;
    for (size_t i = 1; i < blocks_count; ++i)
    {
        block_starts[i] = block_starts[i - 1] + length / blocks_count;
        block_starts[i] &= ~0x3F;
    }
    /* start region: */
    #pragma omp parallel // num_threads(2)
    {
        size_t id = omp_get_thread_num();
        /* 3. initializate buffers */
        size_t *bs = radix_MSD_cpu_block_bs[id];
        memset(bs, 0, sizeof(*bs) * 256);
        /* 4. count letters */
        ssize_t start = block_starts[id];
        ssize_t end = (id + 1 < blocks_count ? block_starts[id + 1] : length);
        for (ssize_t i = start; i < end; ++i)
        {
            if (s[i].s[letter] == 0)
            {
                s[i].e = -1;
            }
            unsigned char res = 0;
            if (s[i].e != (size_t)-1)
            {
                res = s[i].s[letter];
            }
            bs[res]++;
        }
        /* 6. syncronize buffers between threads */
        #pragma omp barrier
        #pragma omp single
        {
            size_t begin = 0;
            for (size_t backet = 0; backet < 256; ++backet)
            {
                for (size_t tid = 0; tid < blocks_count; ++tid)
                {
                    begin += radix_MSD_cpu_block_bs[tid][backet];
                    radix_MSD_cpu_block_bs[tid][backet] = begin;
                }
            }
        }
        #pragma omp barrier
        /* 7. place elements into tmp */
        for (ssize_t i = end - 1; i >= start; --i)
        {
            unsigned char res = 0;
            if (s[i].e != (size_t)-1)
            {
                res = s[i].s[letter];
            }
            bs[res]--;
            tmp[bs[res]] = s[i];
        }
    }

    // printf("END!\n");
    size_t glob_bs[256];
    memcpy(glob_bs, radix_MSD_cpu_block_bs[0], sizeof(glob_bs));
    /* copy back first group */
    {
        memcpy(s, tmp, sizeof(*s) * glob_bs[1]);
    }
    /* call recursive sorts, if block */
    for (size_t i = 1; i < 256; ++i)
    {
        size_t start = glob_bs[i];
        size_t end = (i + 1 == 256 ? length : glob_bs[i + 1]);
        // printf("[%zu] -> s[%zu %zu]!\n", i, start, end);

        if (end - start == 0)
        { }
        else if (end - start == 1)
        {
            s[start] = tmp[start];
        }
        else if (end - start < 1024)
        {
            radix_MSD_qsort_compare_offset = letter;
            memcpy(s + start, tmp + start, sizeof(*s) * (end - start));
            qsort(s + start, end - start, sizeof(*s), radix_MSD_qsort_compare_string_strcmp);
        }
        else
        {
            memcpy(s + start, tmp + start, sizeof(*s) * (end - start));
            radix_MSD_cpu_recurse19(s + start, tmp + start, letter + 1, end - start);
        }
    }
}


void radix_MSD_cpu19(char **index, size_t length)
{
    if (length < 1024)
    {
        qsort(index, length, sizeof(*index), compare_string_strcmp);
        return;
    }
    /* allocate temporary buffer */
    struct radix_MSD_array *s = malloc(sizeof(*s) * length);
    struct radix_MSD_array *tmp = malloc(sizeof(*tmp) * length);
    for (size_t i = 0; i < length; ++i)
    {
        s[i].s = index[i];
        s[i].e = 0;
    }
    radix_MSD_cpu_recurse19(s, tmp, 0, length);
    for (size_t i = 0; i < length; ++i)
    {
        index[i] = s[i].s;
    }
    free(tmp);
    free(s);
}


void radix_MSD_cpu_recurse17(struct radix_MSD_array *s, struct radix_MSD_array *tmp, size_t letter, size_t length)
{
    /* 1. if input is less than 16384 */
    if (length <= 131072)
    {
        radix_MSD_no_strlen_fallback_recurse(s, tmp, letter, length);
        return;
    }
    /* 2. else - split array on blocks for each thread */
    size_t blocks_count = omp_get_max_threads(); //omp_get_num_threads();
    size_t *block_starts = radix_MSD_cpu_block_starts;
    block_starts[0] = 0;
    for (size_t i = 1; i < blocks_count; ++i)
    {
        block_starts[i] = block_starts[i - 1] + length / blocks_count;
        block_starts[i] &= ~0x3F;
    }
    /* start region: */
    #pragma omp parallel // num_threads(2)
    {
        size_t id = omp_get_thread_num();
        /* 3. initializate buffers */
        size_t *bs = radix_MSD_cpu_block_bs[id];
        memset(bs, 0, sizeof(*bs) * 256);
        /* 4. count letters */
        ssize_t start = block_starts[id];
        ssize_t end = (id + 1 < blocks_count ? block_starts[id + 1] : length);
        for (ssize_t i = start; i < end; ++i)
        {
            if (s[i].s[letter] == 0)
            {
                s[i].e = -1;
            }
            unsigned char res = 0;
            if (s[i].e != (size_t)-1)
            {
                res = s[i].s[letter];
            }
            bs[res]++;
        }
        /* 6. syncronize buffers between threads */
        #pragma omp barrier
        #pragma omp single
        {
            size_t begin = 0;
            for (size_t backet = 0; backet < 256; ++backet)
            {
                for (size_t tid = 0; tid < blocks_count; ++tid)
                {
                    begin += radix_MSD_cpu_block_bs[tid][backet];
                    radix_MSD_cpu_block_bs[tid][backet] = begin;
                }
            }
        }
        #pragma omp barrier
        /* 7. place elements into tmp */
        for (ssize_t i = end - 1; i >= start; --i)
        {
            unsigned char res = 0;
            if (s[i].e != (size_t)-1)
            {
                res = s[i].s[letter];
            }
            bs[res]--;
            tmp[bs[res]] = s[i];
        }
    }

    // printf("END!\n");
    size_t glob_bs[256];
    memcpy(glob_bs, radix_MSD_cpu_block_bs[0], sizeof(glob_bs));
    /* copy back first group */
    {
        memcpy(s, tmp, sizeof(*s) * glob_bs[1]);
    }
    /* call recursive sorts, if block */
    for (size_t i = 1; i < 256; ++i)
    {
        size_t start = glob_bs[i];
        size_t end = (i + 1 == 256 ? length : glob_bs[i + 1]);
        // printf("[%zu] -> s[%zu %zu]!\n", i, start, end);

        if (end - start == 0)
        { }
        else if (end - start == 1)
        {
            s[start] = tmp[start];
        }
        else if (end - start < 1024)
        {
            radix_MSD_qsort_compare_offset = letter;
            memcpy(s + start, tmp + start, sizeof(*s) * (end - start));
            qsort(s + start, end - start, sizeof(*s), radix_MSD_qsort_compare_string_strcmp);
        }
        else
        {
            memcpy(s + start, tmp + start, sizeof(*s) * (end - start));
            radix_MSD_cpu_recurse17(s + start, tmp + start, letter + 1, end - start);
        }
    }
}


void radix_MSD_cpu17(char **index, size_t length)
{
    if (length < 1024)
    {
        qsort(index, length, sizeof(*index), compare_string_strcmp);
        return;
    }
    /* allocate temporary buffer */
    struct radix_MSD_array *s = malloc(sizeof(*s) * length);
    struct radix_MSD_array *tmp = malloc(sizeof(*tmp) * length);
    for (size_t i = 0; i < length; ++i)
    {
        s[i].s = index[i];
        s[i].e = 0;
    }
    radix_MSD_cpu_recurse17(s, tmp, 0, length);
    for (size_t i = 0; i < length; ++i)
    {
        index[i] = s[i].s;
    }
    free(tmp);
    free(s);
}



// void radix_MSD_gpu_recurse(struct radix_MSD_array *s, struct radix_MSD_array *tmp, size_t letter, size_t length)
// {
//     /* 1. if input is less than 16384 */
//     if (length <= 16384)
//     {
//         radix_MSD_int16_recurse(s, tmp, letter, length);
//     }
//     /* 2. else - split array on blocks for each thread */
//     // size_t blocks_count = length / 256;
//     // size_t (*bs)[256] = malloc(sizeof(*bs) * blocks_count);
//     // printf("Level: %d:%d\n", (int)letter, (int)length);
//     size_t bs[256] = {};
//     /* create empty baskets */
//     /* count first letters */
//     for (size_t i = 0; i < length; ++i)
//     {
//         if (s[i].s[letter] == 0)
//         {
//             s[i].e = -1;
//         }
//         unsigned char res = 0;
//         if (s[i].e != (size_t)-1)
//         {
//             res = s[i].s[letter];
//         }
//         bs[res]++;
//     }
//     /* calculate offsets */
//     for (size_t i = 1; i < 256; ++i)
//     {
//         bs[i] += bs[i - 1];
//     }
//     /* sort elements */
//     for (size_t i = length - 1; i < length; --i)
//     {
//         unsigned char res = 0;
//         if (s[i].e != (size_t)-1)
//         {
//             res = s[i].s[letter];
//         }
//         bs[res]--;
//         tmp[bs[res]] = s[i];
//     }
//     /* copy back first group */
//     {
//         memcpy(s, tmp, sizeof(*s) * bs[1]);
//     }
//     /* call recursive sorts, if block */
//     for (size_t i = 1; i < 256; ++i)
//     {
//         size_t start = bs[i];
//         size_t end = (i + 1 == 256 ? length : bs[i + 1]);
//         if (end - start == 0)
//         { }
//         else if (end - start == 1)
//         {
//             s[start] = tmp[start];
//         }
//         else if (end - start < 1024)
//         {
//             radix_MSD_qsort_compare_offset = letter;
//             memcpy(s + start, tmp + start, sizeof(*s) * (end - start));
//             qsort(s + start, end - start, sizeof(*s), radix_MSD_qsort_compare_string_strcmp);
//         }
//         else
//         {
//             radix_MSD_recurse(tmp + start, s + start, letter + 1, end - start);
//             memcpy(s + start, tmp + start, sizeof(*s) * (end - start));
//         }
//     }
// }
//
//
// void radix_MSD_gpu(char **index, size_t length)
// {
//     if (length < 1024)
//     {
//         qsort(index, length, sizeof(*index), compare_string_strcmp);
//         return;
//     }
//     /* allocate temporary buffer */
//     struct radix_MSD_array *s = malloc(sizeof(*s) * length);
//     struct radix_MSD_array *tmp = malloc(sizeof(*tmp) * length);
//     for (size_t i = 0; i < length; ++i)
//     {
//         s[i].s = index[i];
//         s[i].e = 0;
//     }
//     radix_MSD_gpu_recurse(s, tmp, 0, length);
//     for (size_t i = 0; i < length; ++i)
//     {
//         index[i] = s[i].s;
//     }
//     free(tmp);
//     free(s);
// }



void insertion_sort(char **s, size_t length)
{
    for (size_t n = length - 1; n < length; --n)
    {
        for (size_t m = n + 1; m < length && strcmp(s[m], s[m-1]) < 0; ++m)
        {
            char *tmp = s[m - 1];
            s[m - 1] = s[m];
            s[m] = tmp;
        }
    }
}


void simple_qsort(char **s, size_t length)
{
    #define SWAP_PTR(a, b) { char *tmp = (b); (b) = (a); (a) = tmp; }
    if (length <= 1)
    {
        return;
    }
    if (length == 2)
    {
        if (strcmp(s[0], s[1]) > 0)
        {
            SWAP_PTR(s[0], s[1])
        }
        return;
    }
    if (length <= 16)
    {
        insertion_sort(s, length);
        return;
    }
    char *p = s[0];
    char **less_begin = s + 1;
    char **greater_begin = s + 1;
    for (size_t i = 1; i < length; ++i)
    {
        int res = strcmp(p, s[i]);
        /* TODO: replace <= on <, and fix bugs */
        if (res >= 0) /* p < s[i] */
        {
            SWAP_PTR(*greater_begin, s[i]);
            greater_begin++;
        }
        else if (res < 0) /* p > s[i] */
        {
            /* nothing to do */
        }
        else /* p == s[i] */
        {
            /* TODO: here is some bug */
            if (less_begin != greater_begin)
            {
                SWAP_PTR(*less_begin, s[i]);
            }
            less_begin++;
            SWAP_PTR(*greater_begin, s[i]);
            greater_begin++;
        }
    }
    /* now all data is: [eq ... eq lt ... lt gt ... gt] */
    size_t equal_size = less_begin - s;
    size_t less_size = greater_begin - less_begin;
    size_t greater_size = length - equal_size - less_size;
    for (size_t i = 0; i < equal_size; ++i)
    {
        SWAP_PTR(s[i], greater_begin[~i])
    }
    simple_qsort(s, less_size);
    simple_qsort(greater_begin, greater_size);
    #undef SWAP_PTR
}

void merge(char **dest, char **a, char **b, size_t la, size_t lb)
{
    while (la && lb)
    {
        // printf("%p %p\n", *a, *b);
        if (strcmp(*a, *b) <= 0)
        {
            *dest++ = *a++;
            la--;
        }
        else
        {
            *dest++ = *b++;
            lb--;
        }
    }
    while (la--)
    {
        *dest++ = *a++;
    }
    while (lb--)
    {
        *dest++ = *b++;
    }
}

void merge_sort_recurse(char **dest, char **s, size_t length)
{
    if (length <= 1)
    {
        return;
    }
    // printf("%p <- %p [%d]\n", dest, s, (int)length);
    if (length == 2)
    {
        if (strcmp(s[0], s[1]) > 0)
        {
            dest[0] = s[1];
            dest[1] = s[0];
        }
        else
        {
            dest[0] = s[0];
            dest[1] = s[1];
        }
        return;
    }
    if (length < 16)
    {
        insertion_sort(s, length);
        memcpy(dest, s, sizeof(*dest) * length);
        return;
    }
    size_t ll = length >> 1;
    size_t rl = length - ll;
    merge_sort_recurse(dest, s, ll);
    merge_sort_recurse(dest + ll, s + ll, rl);
    merge(s, dest, dest + ll, ll, rl);
    memcpy(dest, s, sizeof(*dest) * length);
}

void merge_sort(char **index, size_t length)
{
    char **dest = malloc(sizeof(*dest) * length);
    merge_sort_recurse(dest, index, length);
    free(dest);
}




#define L (128 - 3)
#define CNT 2048
struct brust0_node_equal
{
    char *data[6];
    size_t size;
    struct brust0_node_equal *next;
};

struct brust0_node
{
    void *data[L];
    size_t leaf;
    struct brust0_node_equal *equal;
    size_t size;
};

struct brust0_node_equal brust0_equal_pool[1024 * 16];
size_t                  brust0_equal_pool_used;
struct brust0_node *brust0_pool;
size_t            brust0_pool_allocated;
size_t            brust0_pool_used;

struct brust0_node_equal *brust0_node_equal_allocate()
{
    struct brust0_node_equal *ptr = &brust0_equal_pool[brust0_equal_pool_used++];
    // if (brust0_equal_pool_used > sizeof(brust0_equal_pool) / sizeof(*brust0_equal_pool))
    // {
    //     printf("Out of pool\n");
    //     exit(1);
    // }
    memset(ptr, 0, sizeof(*ptr));
    return ptr;
}

struct brust0_node *brust0_node_allocate()
{
    struct brust0_node *ptr = &brust0_pool[brust0_pool_used++];
    // if (brust0_pool_used > brust0_pool_allocated)
    // {
    //     printf("Out of pool\n");
    //     exit(1);
    // }
    memset(ptr, 0, sizeof(*ptr));
    ptr->leaf = -1;
    return ptr;
}

void brust0_node_brust(struct brust0_node *node, size_t letter);
void brust0_node_insert(struct brust0_node *node, char *string, size_t letter);

void brust0_node_add_equal(struct brust0_node *node, char *string)
{
    if (node->equal == NULL || node->equal->size == 6)
    {
        struct brust0_node_equal *ptr = brust0_node_equal_allocate();
        ptr->next = node->equal;
        node->equal = ptr;
    }
    node->equal->data[node->equal->size++] = string;
}

void brust0_node_brust(struct brust0_node *node, size_t letter)
{
    char *brust0_node_data[L];
    memcpy(brust0_node_data, node->data, sizeof(brust0_node_data));
    memset(node->data, 0, sizeof(node->data));
    node->leaf = 0;
    node->size = 0;
    for (size_t i = 0; i < L; ++i)
    {
        /* TODO: make little function unrolling [or compiler do so?] */
        brust0_node_insert(node, brust0_node_data[i], letter);
    }
}

void brust0_node_insert(struct brust0_node *node, char *string, size_t letter)
{
    unsigned char c = string[letter];
    if (c == 0)
    {
        brust0_node_add_equal(node, string);
        return;
    }
    c -= ' ';
    if (!node->leaf)
    {
    insert_to_child:
        if (node->data[c] == NULL)
        {
            node->data[c] = brust0_node_allocate();
        }
        brust0_node_insert(node->data[c], string, letter + 1);
        return;
    }
    if (node->size == L)
    {
        brust0_node_brust(node, letter);
        goto insert_to_child;
    }
    node->data[node->size++] = string;
}

char **brust0_node_read(struct brust0_node *node, char **dest)
{
    /* equal */
    struct brust0_node_equal *ptr = node->equal;
    while (ptr)
    {
        for (size_t i = 0; i < ptr->size; ++i)
        {
            *dest++ = ptr->data[i];
        }
        ptr = ptr->next;
    }
    /* other */
    if (!node->leaf)
    {
        for (size_t i = 0; i < 125; ++i)
        {
            if (node->data[i])
            {
                dest = brust0_node_read(node->data[i], dest);
            }
        }
        return dest;
    }
    qsort((char **)node->data, node->size, sizeof(char *), compare_string_strcmp);
    for (size_t i = 0; i < node->size; ++i)
    {
        *dest++ = node->data[i];
    }
    return dest;
}

void brust0_sort(char **index, size_t length)
{
    brust0_pool_allocated = 1024 * CNT;
    brust0_pool = malloc(sizeof(*brust0_pool) * brust0_pool_allocated);
    brust0_pool_used = 0;
    brust0_equal_pool_used = 0;
    struct brust0_node *root = brust0_node_allocate();
    for (size_t i = 0; i < length; ++i)
    {
        brust0_node_insert(root, index[i], 0);
    }
    brust0_node_read(root, index);
    free(brust0_pool);
}
#undef L
#undef CNT

#define L (256 - 3)
#define CNT 1024
struct brust1_node_equal
{
    char *data[6];
    size_t size;
    struct brust1_node_equal *next;
};

struct brust1_node
{
    void *data[L];
    size_t leaf;
    struct brust1_node_equal *equal;
    size_t size;
};

struct brust1_node_equal brust1_equal_pool[1024 * 16];
size_t                  brust1_equal_pool_used;
struct brust1_node *brust1_pool;
size_t            brust1_pool_allocated;
size_t            brust1_pool_used;

struct brust1_node_equal *brust1_node_equal_allocate()
{
    struct brust1_node_equal *ptr = &brust1_equal_pool[brust1_equal_pool_used++];
    // if (brust1_equal_pool_used > sizeof(brust1_equal_pool) / sizeof(*brust1_equal_pool))
    // {
    //     printf("Out of pool\n");
    //     exit(1);
    // }
    memset(ptr, 0, sizeof(*ptr));
    return ptr;
}

struct brust1_node *brust1_node_allocate()
{
    struct brust1_node *ptr = &brust1_pool[brust1_pool_used++];
    // if (brust1_pool_used > brust1_pool_allocated)
    // {
    //     printf("Out of pool\n");
    //     exit(1);
    // }
    memset(ptr, 0, sizeof(*ptr));
    ptr->leaf = -1;
    return ptr;
}

void brust1_node_brust(struct brust1_node *node, size_t letter);
void brust1_node_insert(struct brust1_node *node, char *string, size_t letter);

void brust1_node_add_equal(struct brust1_node *node, char *string)
{
    if (node->equal == NULL || node->equal->size == 6)
    {
        struct brust1_node_equal *ptr = brust1_node_equal_allocate();
        ptr->next = node->equal;
        node->equal = ptr;
    }
    node->equal->data[node->equal->size++] = string;
}

void brust1_node_brust(struct brust1_node *node, size_t letter)
{
    char *brust1_node_data[L];
    memcpy(brust1_node_data, node->data, sizeof(brust1_node_data));
    memset(node->data, 0, sizeof(node->data));
    node->leaf = 0;
    node->size = 0;
    for (size_t i = 0; i < L; ++i)
    {
        /* TODO: make little function unrolling [or compiler do so?] */
        brust1_node_insert(node, brust1_node_data[i], letter);
    }
}

void brust1_node_insert(struct brust1_node *node, char *string, size_t letter)
{
    unsigned char c = string[letter];
    if (c == 0)
    {
        brust1_node_add_equal(node, string);
        return;
    }
    if (!node->leaf)
    {
    insert_to_child:
        if (node->data[c] == NULL)
        {
            node->data[c] = brust1_node_allocate();
        }
        brust1_node_insert(node->data[c], string, letter + 1);
        return;
    }
    if (node->size == L)
    {
        brust1_node_brust(node, letter);
        goto insert_to_child;
    }
    node->data[node->size++] = string;
}

char **brust1_node_read(struct brust1_node *node, char **dest)
{
    /* equal */
    struct brust1_node_equal *ptr = node->equal;
    while (ptr)
    {
        for (size_t i = 0; i < ptr->size; ++i)
        {
            *dest++ = ptr->data[i];
        }
        ptr = ptr->next;
    }
    /* other */
    if (!node->leaf)
    {
        for (size_t i = 0; i < 128; ++i)
        {
            if (node->data[i])
            {
                dest = brust1_node_read(node->data[i], dest);
            }
        }
        return dest;
    }
    qsort((char **)node->data, node->size, sizeof(char *), compare_string_strcmp);
    for (size_t i = 0; i < node->size; ++i)
    {
        *dest++ = node->data[i];
    }
    return dest;
}

void brust1_sort(char **index, size_t length)
{
    brust1_pool_allocated = 1024 * CNT;
    brust1_pool = malloc(sizeof(*brust1_pool) * brust1_pool_allocated);
    brust1_pool_used = 0;
    brust1_equal_pool_used = 0;
    struct brust1_node *root = brust1_node_allocate();
    for (size_t i = 0; i < length; ++i)
    {
        brust1_node_insert(root, index[i], 0);
    }
    brust1_node_read(root, index);
    free(brust1_pool);
}
#undef L
#undef CNT


#define L (1024 - 3)
#define CNT 512
struct brust2_node_equal
{
    char *data[6];
    size_t size;
    struct brust2_node_equal *next;
};

struct brust2_node
{
    void *data[L];
    size_t leaf;
    struct brust2_node_equal *equal;
    size_t size;
};

struct brust2_node_equal brust2_equal_pool[2048 * 16];
size_t                  brust2_equal_pool_used;
struct brust2_node *brust2_pool;
size_t            brust2_pool_allocated;
size_t            brust2_pool_used;

struct brust2_node_equal *brust2_node_equal_allocate()
{
    struct brust2_node_equal *ptr = &brust2_equal_pool[brust2_equal_pool_used++];
    // if (brust2_equal_pool_used > sizeof(brust2_equal_pool) / sizeof(*brust2_equal_pool))
    // {
    //     printf("Out of pool\n");
    //     exit(1);
    // }
    memset(ptr, 0, sizeof(*ptr));
    return ptr;
}

struct brust2_node *brust2_node_allocate()
{
    struct brust2_node *ptr = &brust2_pool[brust2_pool_used++];
    // if (brust2_pool_used > brust2_pool_allocated)
    // {
    //     printf("Out of pool\n");
    //     exit(1);
    // }
    memset(ptr, 0, sizeof(*ptr));
    ptr->leaf = -1;
    return ptr;
}

void brust2_node_brust(struct brust2_node *node, size_t letter);
void brust2_node_insert(struct brust2_node *node, char *string, size_t letter);

void brust2_node_add_equal(struct brust2_node *node, char *string)
{
    if (node->equal == NULL || node->equal->size == 6)
    {
        struct brust2_node_equal *ptr = brust2_node_equal_allocate();
        ptr->next = node->equal;
        node->equal = ptr;
    }
    node->equal->data[node->equal->size++] = string;
}

void brust2_node_brust(struct brust2_node *node, size_t letter)
{
    char *brust2_node_data[L];
    memcpy(brust2_node_data, node->data, sizeof(brust2_node_data));
    memset(node->data, 0, sizeof(node->data));
    node->leaf = 0;
    node->size = 0;
    for (size_t i = 0; i < L; ++i)
    {
        /* TODO: make little function unrolling [or compiler do so?] */
        brust2_node_insert(node, brust2_node_data[i], letter);
    }
}

void brust2_node_insert(struct brust2_node *node, char *string, size_t letter)
{
    unsigned char c = string[letter];
    if (c == 0)
    {
        brust2_node_add_equal(node, string);
        return;
    }
    if (!node->leaf)
    {
    insert_to_child:
        if (node->data[c] == NULL)
        {
            node->data[c] = brust2_node_allocate();
        }
        brust2_node_insert(node->data[c], string, letter + 1);
        return;
    }
    if (node->size == L)
    {
        brust2_node_brust(node, letter);
        goto insert_to_child;
    }
    node->data[node->size++] = string;
}

char **brust2_node_read(struct brust2_node *node, char **dest)
{
    /* equal */
    struct brust2_node_equal *ptr = node->equal;
    while (ptr)
    {
        for (size_t i = 0; i < ptr->size; ++i)
        {
            *dest++ = ptr->data[i];
        }
        ptr = ptr->next;
    }
    /* other */
    if (!node->leaf)
    {
        /* TODO: switch on 256 without loosing speed */
        for (size_t i = 0; i < 128; ++i)
        {
            if (node->data[i])
            {
                dest = brust2_node_read(node->data[i], dest);
            }
        }
        return dest;
    }
    qsort((char **)node->data, node->size, sizeof(char *), compare_string_strcmp);
    for (size_t i = 0; i < node->size; ++i)
    {
        *dest++ = node->data[i];
    }
    return dest;
}

void brust2_sort(char **index, size_t length)
{
    brust2_pool_allocated = 1024 * CNT;
    brust2_pool = malloc(sizeof(*brust2_pool) * brust2_pool_allocated);
    brust2_pool_used = 0;
    brust2_equal_pool_used = 0;
    struct brust2_node *root = brust2_node_allocate();
    for (size_t i = 0; i < length; ++i)
    {
        brust2_node_insert(root, index[i], 0);
    }
    brust2_node_read(root, index);
    free(brust2_pool);
}
#undef CNT
#undef L

#define L (2048 - 3)
#define CNT 256
struct brust3_node_equal
{
    char *data[6];
    size_t size;
    struct brust3_node_equal *next;
};

struct brust3_node
{
    void *data[L];
    size_t leaf;
    struct brust3_node_equal *equal;
    size_t size;
};

struct brust3_node_equal brust3_equal_pool[2048 * 16];
size_t                  brust3_equal_pool_used;
struct brust3_node *brust3_pool;
size_t            brust3_pool_allocated;
size_t            brust3_pool_used;

struct brust3_node_equal *brust3_node_equal_allocate()
{
    struct brust3_node_equal *ptr = &brust3_equal_pool[brust3_equal_pool_used++];
    // if (brust3_equal_pool_used > sizeof(brust3_equal_pool) / sizeof(*brust3_equal_pool))
    // {
    //     printf("Out of pool\n");
    //     exit(1);
    // }
    memset(ptr, 0, sizeof(*ptr));
    return ptr;
}

struct brust3_node *brust3_node_allocate()
{
    struct brust3_node *ptr = &brust3_pool[brust3_pool_used++];
    // if (brust3_pool_used > brust3_pool_allocated)
    // {
    //     printf("Out of pool\n");
    //     exit(1);
    // }
    memset(ptr, 0, sizeof(*ptr));
    ptr->leaf = -1;
    return ptr;
}

void brust3_node_brust(struct brust3_node *node, size_t letter);
void brust3_node_insert(struct brust3_node *node, char *string, size_t letter);

void brust3_node_add_equal(struct brust3_node *node, char *string)
{
    if (node->equal == NULL || node->equal->size == 6)
    {
        struct brust3_node_equal *ptr = brust3_node_equal_allocate();
        ptr->next = node->equal;
        node->equal = ptr;
    }
    node->equal->data[node->equal->size++] = string;
}

void brust3_node_brust(struct brust3_node *node, size_t letter)
{
    char *brust3_node_data[L];
    memcpy(brust3_node_data, node->data, sizeof(brust3_node_data));
    memset(node->data, 0, sizeof(node->data));
    node->leaf = 0;
    node->size = 0;
    for (size_t i = 0; i < L; ++i)
    {
        /* TODO: make little function unrolling [or compiler do so?] */
        brust3_node_insert(node, brust3_node_data[i], letter);
    }
}

void brust3_node_insert(struct brust3_node *node, char *string, size_t letter)
{
    unsigned char c = string[letter];
    if (c == 0)
    {
        brust3_node_add_equal(node, string);
        return;
    }
    if (!node->leaf)
    {
    insert_to_child:
        if (node->data[c] == NULL)
        {
            node->data[c] = brust3_node_allocate();
        }
        brust3_node_insert(node->data[c], string, letter + 1);
        return;
    }
    if (node->size == L)
    {
        brust3_node_brust(node, letter);
        goto insert_to_child;
    }
    node->data[node->size++] = string;
}

char **brust3_node_read(struct brust3_node *node, char **dest)
{
    /* equal */
    struct brust3_node_equal *ptr = node->equal;
    while (ptr)
    {
        for (size_t i = 0; i < ptr->size; ++i)
        {
            *dest++ = ptr->data[i];
        }
        ptr = ptr->next;
    }
    /* other */
    if (!node->leaf)
    {
        /* TODO: switch on 256 without loosing speed */
        for (size_t i = 0; i < 128; ++i)
        {
            if (node->data[i])
            {
                dest = brust3_node_read(node->data[i], dest);
            }
        }
        return dest;
    }
    qsort((char **)node->data, node->size, sizeof(char *), compare_string_strcmp);
    for (size_t i = 0; i < node->size; ++i)
    {
        *dest++ = node->data[i];
    }
    return dest;
}

void brust3_sort(char **index, size_t length)
{
    brust3_pool_allocated = 1024 * CNT;
    brust3_pool = malloc(sizeof(*brust3_pool) * brust3_pool_allocated);
    brust3_pool_used = 0;
    brust3_equal_pool_used = 0;
    struct brust3_node *root = brust3_node_allocate();
    for (size_t i = 0; i < length; ++i)
    {
        brust3_node_insert(root, index[i], 0);
    }
    brust3_node_read(root, index);
    free(brust3_pool);
}
#undef CNT
#undef L


void shell_pass(char **array, size_t size, size_t step)
{
    for (size_t i = step; i < size; ++i) {
        size_t from = i - step;
        for (size_t j = from; j <= from && strcmp(array[j], array[j + step]) > 0; j -= step)
        {
            char *temp = array[j];
            array[j] = array[j + step];
            array[j + step] = temp;
        }
    }
}


void shell_sort_shell(char **index, size_t length)
{
    for (size_t step = length / 2; step > 0; step /= 2)
    {
        shell_pass(index, length, step);
    }
}

/* IT IS TOO SLOW! */
// void shell_sort_hibbard(char **index, size_t length)
// {
//     for (size_t step = __builtin_log2(length) * 2; step > 0; step /= 2)
//     {
//         shell_pass(index, length, step - 1);
//     }
// }

size_t steps[2048];
void shell_sort_pratt(char **index, size_t length)
{
    size_t id = 0;
    for (size_t x = 1; x < length; x <<= 1)
    {
        for (size_t y = x; y < length; y *= 3)
        {
            steps[id++] = y;
        }
    }
    qsort(steps, id, sizeof(*steps), compare_size);
    for (size_t step = id - 1; step < id; --step)
    {
        shell_pass(index, length, steps[step]);
    }
}

void shell_sort_tokuda(char **index, size_t length)
{
    for (size_t step = length / 2; step > 1; step /= 2.25)
    {
        shell_pass(index, length, step);
    }
    shell_pass(index, length, 1);
}

// static inline int batcher_odd_even_strcmp(const char *a, const char *b)
// {
//     if (a == b) return 0;
//     if (a == NULL) return 1;
//     if (b == NULL) return -1;
//     return strcmp(a, b);
// }
//
// void batcher_odd_even_sort(char **arr, size_t n)
// {
//     if (n <= 1) return;
//
//     size_t p = 1LLU << (64 - __builtin_clzll(n));
//     while (p < n)
//     {
//         p <<= 1LLU;
//     }
//
//     char **buf = malloc(p * sizeof(char *));
//     memcpy(buf, arr, sizeof(*buf) * n);
//     memset(buf + n, 0, sizeof(*buf) * (p - n));
//
//     for (size_t block = 2; block <= p; block <<= 1)
//     {
//         #pragma omp parallel for
//         for (size_t lo = 0; lo < p; lo += block)
//         {
//             // printf("Thread %d/%d!\n", omp_get_thread_num(), omp_get_num_threads());
//             /* sort block */
//             for (size_t k = block >> 1; k > 0; k >>= 1)
//             {
//                 for (size_t j = k; j > 0; j >>= 1)
//                 {
//                     for (size_t i = 0; i + j < block; ++i)
//                     {
//                         if ((i & k) == 0)
//                         {
//                             size_t idx1 = lo + i;
//                             size_t idx2 = lo + i + j;
//                             if (batcher_odd_even_strcmp(buf[idx1], buf[idx2]) > 0)
//                             {
//                                 char *tmp = buf[idx1];
//                                 buf[idx1] = buf[idx2];
//                                 buf[idx2] = tmp;
//                             }
//                         }
//                     }
//                 }
//             }
//         }
//     }
//
//     memcpy(arr, buf, sizeof(*arr) * n);
//     free(buf);
// }



void assert_sorted(char **index, size_t length)
{
    for (size_t i = 1; i < length; ++i)
    {
        if (strcmp(index[i - 1], index[i]) > 0)
        {
            // for (size_t t = 0; t < 10; ++t)
            // {
            //     printf("[%zu]=%s\n", t, index[t]);
            // }

            printf("%s at %s is wrong:\n", current_variant, current_option);
            printf("%s\n", index[i - 1]);
            printf(">\n");
            printf("%s\n", index[i]);
            exit(8);
        }
    }
}


void create(char *buffer, char **index, size_t strings, size_t min_string_size, size_t max_string_size, size_t full_alphabet)
{
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



    printf("Run %d %d %d ?\n", (int)strings, (int)min_string_size, (int)max_string_size);

    #define ALLOCATE_STRING \
        buffer = malloc(strings * (max_string_size + 1)); \
        index = malloc(sizeof(*index) * (strings + 1));
        
    #define BUILD_STRING \
        create(buffer, index, strings, min_string_size, max_string_size, full_alphabet);
        
    ALLOCATE_STRING

#ifdef TEST_BUILTIN
    BENCH_VARIANT("builtin + strcmp")
    {
        BENCH_RUN(0, 1)
        {
            BUILD_STRING
            BENCH_START
            {
                qsort(index, strings, sizeof(*index), compare_string_strcmp);
            }
            BENCH_END
            assert_sorted(index, strings);
        }
        free(buffer);
        free(index);
    }

    BENCH_VARIANT("builtin + check first byte")
    {
        BENCH_RUN(0, 1)
        {
            BUILD_STRING
            BENCH_START
            {
                qsort(index, strings, sizeof(*index), compare_string_strcmp_and_check_letter);
            }
            BENCH_END
            assert_sorted(index, strings);
        }
        free(buffer);
        free(index);
    }
#endif

#ifdef TEST_RADIX_LSD
    BENCH_VARIANT("radix LSD")
    {
        BENCH_RUN(0, 1)
        {
            BUILD_STRING
            BENCH_START
            {
                radix_LSD(index, strings);
            }
            BENCH_END
            assert_sorted(index, strings);
        }
        free(buffer);
        free(index);
    }
#endif


#ifdef TEST_RADIX_MSD
    BENCH_VARIANT("radix MSD no fallback")
    {
        BENCH_RUN(0, 1)
        {
            BUILD_STRING
            BENCH_START
            {
                radix_MSD_no_fallback(index, strings);
            }
            BENCH_END
            assert_sorted(index, strings);
        }
        free(buffer);
        free(index);
    }


    BENCH_VARIANT("radix MSD no sqort,strlen")
    {
        BENCH_RUN(0, 1)
        {
            BUILD_STRING
            BENCH_START
            {
                radix_MSD_no_strlen_fallback(index, strings);
            }
            BENCH_END
            assert_sorted(index, strings);
        }
        free(buffer);
        free(index);
    }


    BENCH_VARIANT("radix MSD")
    {
        BENCH_RUN(0, 1)
        {
            BUILD_STRING
            BENCH_START
            {
                radix_MSD(index, strings);
            }
            BENCH_END
            assert_sorted(index, strings);
        }
        free(buffer);
        free(index);
    }


    BENCH_VARIANT("radix MSD int16")
    {
        BENCH_RUN(0, 1)
        {
            BUILD_STRING
            BENCH_START
            {
                radix_MSD_int16(index, strings);
            }
            BENCH_END
            assert_sorted(index, strings);
        }
        free(buffer);
        free(index);
    }
#endif


#ifdef TEST_RADIX_MSD_CPU
    BENCH_VARIANT("radix MSD cpu 2^{19}")
    {
        BENCH_RUN(0, 1)
        {
            BUILD_STRING
            BENCH_START
            {
                radix_MSD_cpu19(index, strings);
            }
            BENCH_END
            assert_sorted(index, strings);
        }
        free(buffer);
        free(index);
    }

    BENCH_VARIANT("radix MSD cpu 2^{17}")
    {
        BENCH_RUN(0, 1)
        {
            BUILD_STRING
            BENCH_START
            {
                radix_MSD_cpu17(index, strings);
            }
            BENCH_END
            assert_sorted(index, strings);
        }
        free(buffer);
        free(index);
    }
#endif


#ifdef TEST_RADIX_MSD_GPU
    BENCH_VARIANT("radix MSD gpu")
    {
        BENCH_RUN(0, 1)
        {
            BUILD_STRING
            BENCH_START
            {
                radix_MSD_gpu(index, strings);
            }
            BENCH_END
            assert_sorted(index, strings);
        }
        free(buffer);
        free(index);
    }
#endif


#ifdef TEST_QSORT
    BENCH_VARIANT("simple qsort")
    {
        BENCH_RUN(0, 1)
        {
            BUILD_STRING
            BENCH_START
            {
                simple_qsort(index, strings);
            }
            BENCH_END
            assert_sorted(index, strings);
        }
        free(buffer);
        free(index);
    }
#endif


#ifdef TEST_MERGESORT
    BENCH_VARIANT("simple mergesort")
    {
        BENCH_RUN(0, 1)
        {
            BUILD_STRING
            BENCH_START
            {
                merge_sort(index, strings);
            }
            BENCH_END
            assert_sorted(index, strings);
        }
        free(buffer);
        free(index);
    }
#endif


#ifdef TEST_BRUST
    BENCH_VARIANT("brust0 128 sort")
    {
        BENCH_RUN(0, 1)
        {
            BUILD_STRING
            BENCH_START
            {
                brust0_sort(index, strings);
            }
            BENCH_END
            assert_sorted(index, strings);
        }
        free(buffer);
        free(index);
    }

    BENCH_VARIANT("brust1 256 sort")
    {
        BENCH_RUN(0, 1)
        {
            BUILD_STRING
            BENCH_START
            {
                brust1_sort(index, strings);
            }
            BENCH_END
            assert_sorted(index, strings);
        }
        free(buffer);
        free(index);
    }


    BENCH_VARIANT("brust2 1k sort")
    {
        BENCH_RUN(0, 1)
        {
            BUILD_STRING
            BENCH_START
            {
                brust2_sort(index, strings);
            }
            BENCH_END
            assert_sorted(index, strings);
        }
        free(buffer);
        free(index);
    }


    BENCH_VARIANT("brust3 2k sort")
    {
        BENCH_RUN(0, 1)
        {
            BUILD_STRING
            BENCH_START
            {
                brust3_sort(index, strings);
            }
            BENCH_END
            assert_sorted(index, strings);
        }
        free(buffer);
        free(index);
    }
#endif

#ifdef TEST_SHELL
    BENCH_VARIANT("shell shell")
    {
        BENCH_RUN(0, 1)
        {
            BUILD_STRING
            BENCH_START
            {
                shell_sort_shell(index, strings);
            }
            BENCH_END
            assert_sorted(index, strings);
        }
        free(buffer);
        free(index);
    }

    /* IT WAS TOO SLOW! */
    // BENCH_VARIANT("shell hibbard")
    // {
    //     BENCH_RUN(0, 1)
    //     {
    //         BUILD_STRING
    //         BENCH_START
    //         {
    //             shell_sort_hibbard(index, strings);
    //         }
    //         BENCH_END
    //         assert_sorted(index, strings);
    //         free(buffer);
    //         free(index);
    //     }
    // }

    BENCH_VARIANT("shell pratt")
    {
        BENCH_RUN(0, 1)
        {
            BUILD_STRING
            BENCH_START
            {
                shell_sort_pratt(index, strings);
            }
            BENCH_END
            assert_sorted(index, strings);
        }
        free(buffer);
        free(index);
    }

    BENCH_VARIANT("shell tokuda")
    {
        BENCH_RUN(0, 1)
        {
            BUILD_STRING
            BENCH_START
            {
                shell_sort_tokuda(index, strings);
            }
            BENCH_END
            assert_sorted(index, strings);
        }
        free(buffer);
        free(index);
    }
#endif

// #ifdef TEST_BATCHER_ODDEVEN
//     BENCH_VARIANT("Batcher Odd-Even [parallel]")
//     {
//         BENCH_RUN(0, 1)
//         {
//             BENCH_START
//             {
//                 batcher_odd_even_sort(index, strings);
//             }
//             BENCH_END
//             assert_sorted(index, strings);
//         }
//        free(buffer);
//        free(index);
//     }
// #endif

}
