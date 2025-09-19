#define __USE_MINGW_ANSI_STDIO 1
#include "stdio.h"
#include "malloc.h"
#include "search.h"
#include "string.h"
#include "inttypes.h"


// #define DEBUG_INFO


#ifdef WIN32
    #include "windows.h"
    
    #ifndef FILE_MAP_LARGE_PAGES
    #define FILE_MAP_LARGE_PAGES 0x20000000
    #endif
    
    typedef struct _MEMORY_RANGE_ENTRY {
            PVOID VirtualAddress;
        SIZE_T NumberOfBytes;
    } MEMORY_RANGE_ENTRY, *PMEMORY_RANGE_ENTRY;
    __declspec(dllimport) BOOL WINAPI PrefetchVirtualMemory(HANDLE, ULONG_PTR, PMEMORY_RANGE_ENTRY, ULONG);
    
#else
    #include "fcntl.h"
    #include "unistd.h"
    #include "sys/stat.h"
#endif

struct MSD_array_t
{
    char *s;
    char *e;
};

void sort_forward(struct MSD_array_t *index, struct MSD_array_t *tmp, size_t index_length);
void sort_backward(struct MSD_array_t *index, struct MSD_array_t *tmp, size_t index_length);

int forward_cmp(const void *a, const void *b)
{
    const struct MSD_array_t *x = *(const struct MSD_array_t **)a;
    const struct MSD_array_t *y = *(const struct MSD_array_t **)b;
    return strcmp(x->s, y->s);
}


int backward_cmp(const void *pa, const void *pb)
{
    const struct MSD_array_t *a = pa;
    const struct MSD_array_t *b = pb;
    const char *x = a->e - 1;
    const char *y = b->e - 1;
    while (x >= a->s && y >= b->s && *x == *y)
    {
        --x;
        --y;
    }
    if (x < a->s && y < b->s)
    {
        return 0;
    }
    if (x < a->s)
    {
        return -1;
    }
    if (y < b->s)
    {
        return 1;
    }
    return *x - *y;
}


#define ALIGN_PAD64(buf) ((64 - (size_t)(buf) % 64) % 64)
char *next_newline(char *str, char *end)
{
    if (str == NULL)
    {
        return NULL;
    }

    const char *str_aligned = str + ALIGN_PAD64(str);

    if (str_aligned > end)
    {
        str_aligned = end;
    }

    while (str < str_aligned && *str != '\r' && *str != '\n')
    {
        str++;
    }

    if (str >= end || *str == '\r' || *str == '\n')
    {
        return (char *)str;
    }
    
    __m256i ymm0 = _mm256_set1_epi8('\n');
    #ifdef _WIN32
        __m256i ymmR = _mm256_set1_epi8('\r');
    #endif

    size_t aligned_size = (end - str) & ~0x1F;
    while (aligned_size != 0)
    {
        __m256i ymm2 = _mm256_loadu_si256((__m256i *)(str +  0));
        uint32_t msk0 = _mm256_movemask_epi8(_mm256_cmpeq_epi8(ymm2, ymm0));
        uint32_t msk2 = _mm256_movemask_epi8(_mm256_cmpeq_epi8(ymm2, _mm256_setzero_si256()));
        #ifdef _WIN32
            uint32_t mskR = _mm256_movemask_epi8(_mm256_cmpeq_epi8(ymm2, ymmR));
        #endif
        if (msk0 | msk2
        #ifdef _WIN32
            | mskR
        #endif
        ) {
            str += _tzcnt_u32(msk0 | msk2
            #ifdef _WIN32
                | mskR
            #endif
            );
            return (char *)str;
        }
        str += 32;
        aligned_size -= 32;
    }

    if (str >= end || *str == '\r' || *str == '\n')
    {
        return (char *)str;
    }
    
    while (str < end && *str != '\r' && *str != '\n')
    {
        str++;
    }
    
    return str;
}


int main(int argc, const char **argv)
{
    if (argc != 3)
    {
        fprintf(stderr, "get %d input files instead of 2\n", argc - 1);
        return 1;
    }

    size_t content_length;
    char *content;
    char *content_end;

    #ifdef _WIN32
        HANDLE hFile = CreateFileA(
            argv[1],
            GENERIC_READ,
            FILE_SHARE_READ,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );

        if (hFile == INVALID_HANDLE_VALUE) {
            printf("CreateFile failed: %lu\n", GetLastError());
            return 1;
        }

        // Получаем размер файла
        LARGE_INTEGER fileSize;
        if (!GetFileSizeEx(hFile, &fileSize)) {
            printf("GetFileSizeEx failed: %lu\n", GetLastError());
            CloseHandle(hFile);
            return 1;
        }

        // Создаём отображение с защитой PAGE_WRITECOPY
        HANDLE hMap = CreateFileMappingA(
            hFile,
            NULL,
            PAGE_WRITECOPY,
            0,
            0, // отображаем весь файл
            NULL
        );

        if (hMap == NULL) {
            printf("CreateFileMapping failed: %lu\n", GetLastError());
            CloseHandle(hFile);
            return 1;
        }
        
        content = MapViewOfFile(hMap, FILE_MAP_COPY, 0, 0, 0);

        if (content == NULL) {
            printf("MapViewOfFile failed: %lu\n", GetLastError());
            CloseHandle(hMap);
            CloseHandle(hFile);
            return 1;
        }
        content_length = fileSize.QuadPart;
    #else
        int fd = open(argv[1], O_RDONLY);
        if (fd < 0)
        {
            fprintf(stderr, "Cannot find file <%s>\n", argv[1]);
            return 1;
        }

        struct stat st;

        fstat(fd, &st);
        off_t file_size = st.st_size;
        content = calloc(file_size, 1);

        ssize_t bytes_read;
        size_t total_bytes_read = 0;
        while ((bytes_read = read(fd, content + total_bytes_read, file_size - total_bytes_read)) > 0) 
        {
            total_bytes_read += bytes_read;
        }

        if (bytes_read == -1)
        {
            printf("Error while reading\n");
            return 1;
        }
        
        content_length = file_size;
    #endif



    content_end = content + content_length;

    
    #ifdef DEBUG_INFO
        printf("File size: %zu\n", content_length);
    #endif

    char *output_buffer;

    #if defined(_WIN32)
        HANDLE hOut = CreateFileA(
            argv[2],
            GENERIC_READ | GENERIC_WRITE,
            0, // Exclusive read
            NULL,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );
        if (hOut == INVALID_HANDLE_VALUE)
        {
            printf("CreateFile error %ld\n", GetLastError());
            return 0;
        }
        size_t result_length = content_length * 3 + 64 * 1024;
        // size_t min_size = GetLargePageMinimum();
        // result_length += min_size - (result_length % min_size);
        // printf("Min=%zu\n", min_size);
        // printf("Res=%zu\n", result_length);
        unsigned sizeHi = result_length >> 32ULL;
        unsigned sizeLow = result_length & 0xFFFFFFFFULL;
        HANDLE hMapFile = CreateFileMapping(
            hOut,
            NULL,
            PAGE_READWRITE
            | SEC_COMMIT /* sec commit is enabled by default */
            // | SEC_NOCACHE /* only slow down program */
            // | SEC_LARGE_PAGES /* can't use it with file on disk? */
            ,
            sizeHi,
            sizeLow,
            NULL
        );
        if (hMapFile == INVALID_HANDLE_VALUE)
        {
            printf("CreateFileMapping error %ld\n", GetLastError());
            return 0;
        }
        LPVOID mapBaseAddress = MapViewOfFile(
            hMapFile,
            FILE_MAP_ALL_ACCESS, // | FILE_MAP_LARGE_PAGES,
            0,
            0,
            0
        );
        if (mapBaseAddress == NULL)
        {
            printf("MapViewOfFile error %ld\n", GetLastError());
            return 0;
        }
        output_buffer = mapBaseAddress;
    #else
        FILE *fo = fopen(argv[2], "wb");
        if (fo == NULL)
        {
            fprintf(stderr, "Cannot find file <%s>\n", argv[1]);
            return 1;
        }
        output_buffer = malloc(content_length * 3);
    #endif
    
    char *output_buffer_end = output_buffer;
    



    /* 1. read lines */
    size_t index_length = 0;
    size_t index_alloc = 128;
    struct MSD_array_t *index = malloc(sizeof(*index) * index_alloc);
    {
        char *s = content;
        while (s < content_end)
        {
            if (index_length == index_alloc)
            {
                index_alloc = index_alloc * 2;
                void *new_ptr = realloc(index, sizeof(*index) * index_alloc);
                if (new_ptr == NULL)
                {
                    free(index);
                    printf("NOT ENOUGTH MEMORY\n");
                    return 1;
                }
                index = new_ptr;
            }
            char *res = s;

            s = next_newline(s, content_end);
            
            if (s >= content_end)
            {
                s = content_end;
            }

            char *end = s;
            while (res < s &&
                !(('a' <= *res && *res <= 'z') || ('A' <= *res && *res <= 'Z') || ('0' <= *res && *res <= '9'))
            )
            {
                res++;
            }
            while (end > res &&
                !(('a' <= *end && *end <= 'z') || ('A' <= *end && *end <= 'Z') || ('0' <= *end && *end <= '9'))
            )
            {
                end--;
            }
            if (end != res)
            {
                end++;
            }
            
            if (end - res > 0)
            {
                index[index_length].s = res;
                index[index_length].e = end;
                index_length++;
            }
            /* skip starting commas */
            if (s < content_end)
            {
                // *s++ = 0;
                s++;
                if (s < content_end && *s == '\n')
                {
                    // *s++ = 0;
                    s++;
                }
            }
        }
    }
    struct MSD_array_t *ttmp = malloc(sizeof(*ttmp) * index_length);
    // memcpy(index_bak, index, sizeof(*index_bak) * index_length);

    #ifdef DEBUG_INFO
        printf("read %d lines.\n", (int)index_length);
        printf("INDEP PTR: %p\n", index);

        printf("[0]%*.*s\n", (int)(index[0].e - index[0].s), (int)(index[0].e - index[0].s), index[0].s);
        printf("[1]%*.*s\n", (int)(index[1].e - index[1].s), (int)(index[1].e - index[1].s), index[1].s);
        printf("[2]%*.*s\n", (int)(index[2].e - index[2].s), (int)(index[2].e - index[2].s), index[2].s);
    #endif

    /* 1. alphabetic */
    sort_forward(index, ttmp, index_length);
    for (size_t i = 0; i < index_length; ++i)
    {
        /* insert to output_buffer */
        memcpy(output_buffer_end, index[i].s, index[i].e - index[i].s);
        output_buffer_end += index[i].e - index[i].s;
        #ifdef _WIN32
            *output_buffer_end++ = '\r';
        #endif
        *output_buffer_end++ = '\n';
    }
    /* 2. backwards alphabetic */
    // memcpy(index, index_bak, sizeof(*index) * index_length);
    sort_backward(index, ttmp, index_length);
    for (size_t i = 0; i < index_length; ++i)
    {
        memcpy(output_buffer_end, index[i].s, index[i].e - index[i].s);
        output_buffer_end += index[i].e - index[i].s;
        #ifdef _WIN32
            *output_buffer_end++ = '\r';
        #endif
        *output_buffer_end++ = '\n';
    }

    /* 3. copying */
    #ifdef _WIN32
        memcpy(output_buffer_end, content, content_length);
    #else
        fwrite(output_buffer, output_buffer_end - output_buffer, 1, fo);
        fwrite(content, content_length, 1, fo);
    #endif

    #ifdef _WIN32
        // Cleanup
        UnmapViewOfFile((LPCVOID)content);
        UnmapViewOfFile((LPCVOID)output_buffer);
        CloseHandle(hMap);
        CloseHandle(hMapFile);
        CloseHandle(hFile);
        CloseHandle(hOut);
    #else
        free(content);
        close(fd);
        fclose(fo);
    #endif

    
    return 0;
}





void forward_msd_fast_recurse(struct MSD_array_t *s, struct MSD_array_t *tmp, size_t letter, size_t length)
{
    size_t bs[128] = {};
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
    for (size_t i = 1; i < 128; ++i)
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
    for (size_t i = 1; i < 128; ++i)
    {
        size_t start = bs[i];
        size_t end = (i + 1 == 128 ? length : bs[i + 1]);
        if (end - start == 0)
        { }
        else if (end - start == 1)
        {
            s[start] = tmp[start];
        }
        else
        {
            forward_msd_fast_recurse(tmp + start, s + start, letter + 1, end - start);
            memcpy(s + start, tmp + start, sizeof(*s) * (end - start));
        }
    }
}


void forward_msd_fast(struct MSD_array_t *index, struct MSD_array_t *tmp, size_t length)
{
    forward_msd_fast_recurse(index, tmp, 0, length);
}






void sort_forward(struct MSD_array_t *index, struct MSD_array_t *tmp, size_t index_length)
{
    forward_msd_fast(index, tmp, index_length);
}



void backward_msd_fast_recurse(struct MSD_array_t *s, struct MSD_array_t *tmp, size_t letter, size_t length)
{
    size_t bs[128] = {};
    /* create empty baskets */
    /* count first letters */
    for (size_t i = 0; i < length; ++i)
    {
        unsigned char res = 0;
        if (s[i].e - letter >= s[i].s)
        {
            res = s[i].e[-letter];
        }
        bs[res]++;
    }
    /* calculate offsets */
    for (size_t i = 1; i < 128; ++i)
    {
        bs[i] += bs[i - 1];
    }
    /* sort elements */
    for (size_t i = length - 1; i < length; --i)
    {
        unsigned char res = 0;
        if (s[i].e - letter >= s[i].s)
        {
            res = s[i].e[-letter];
        }
        bs[res]--;
        tmp[bs[res]] = s[i];
    }
    /* copy back first group */
    {
        memcpy(s, tmp, sizeof(*s) * bs[1]);
    }
    /* call recursive sorts, if block */
    for (size_t i = 1; i < 128; ++i)
    {
        size_t start = bs[i];
        size_t end = (i + 1 == 128 ? length : bs[i + 1]);
        if (end - start == 0)
        { }
        else if (end - start == 1)
        {
            s[start] = tmp[start];
        }
        else
        {
            backward_msd_fast_recurse(tmp + start, s + start, letter + 1, end - start);
            memcpy(s + start, tmp + start, sizeof(*s) * (end - start));
        }
    }
}


void backward_msd_fast(struct MSD_array_t *index, struct MSD_array_t *tmp, size_t length)
{
    backward_msd_fast_recurse(index, tmp, 1, length);
}

void sort_backward(struct MSD_array_t *index, struct MSD_array_t *tmp, size_t length)
{
    backward_msd_fast(index, tmp, length);
}

