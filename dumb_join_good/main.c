#define __USE_MINGW_ANSI_STDIO 1
#include "stdio.h"
#include "malloc.h"
#include "search.h"
#include "string.h"


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

int forward_cmp(const void *a, const void *b)
{
    const char *x = *(const char **)a;
    const char *y = *(const char **)b;
    return strcmp(x, y);
}


int backward_cmp(const void *pa, const void *pb)
{
    const char *a = *(const char **)pa;
    const char *b = *(const char **)pb;
    const char *x = a + strlen(a);
    const char *y = b + strlen(b);
    while (x >= a && y >= b && *x == *y)
    {
        --x;
        --y;
    }
    if (x < a && y < b)
    {
        return 0;
    }
    if (x < a)
    {
        return -1;
    }
    if (y < b)
    {
        return 1;
    }
    return *x - *y;
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
        fprintf(stderr, "Cannot find file <%s>\n", argv[1]);
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
        
        LPVOID view = MapViewOfFile(hMap, FILE_MAP_COPY, 0, 0, 0);

        if (view == NULL) {
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

    content_end = content_end + content_length;

    printf("File size: %zu\n", content_length);

    /* 1. read lines */
    size_t index_length = 0;
    size_t index_alloc = 128;
    char **index = malloc(sizeof(char *) * index_alloc);
    {
        char *s = content;
        printf("%p < %p < %p\n", content, s, content_end);
        while (s < content_end)
        {
            if (index_length == index_alloc)
            {
                index_alloc = index_alloc * 2;
                char **new_ptr = realloc(index, sizeof(char *) * index_alloc);
                if (new_ptr == NULL)
                {
                    for (size_t i = 0; i < index_length; ++i)
                    {
                        free(index[i]);
                    }
                    free(index);
                    printf("NOT ENOUGTH MEMORY\n");
                    return 1;
                }
                index = new_ptr;
            }
            if (!(index_length & 0xFFFF))
            {
                printf("read s[%zu]\n", index_length);
            }
            printf(">%d\n", __LINE__);
            char *res = s;
            printf("%d\n", *s);
            while (*s != '\r' && *s != '\n' && s < content_end)
            {
                printf("%p < %p < %p\n", content, s, content_end);
                printf("s=<%c>\n", *s);
                s++;
            }
            printf(">%d\n", __LINE__);
            // printf("%p < %p < %p\n", content, s, content_end);
            if (s >= content_end)
            {
                printf(">%d\n", __LINE__);
                if (s - res > 0)
                {
                    index[index_length] = malloc(s - res + 1);
                    memcpy(index[index_length], res, s - res);
                    index[index_length][s - res] = 0;
                    index_length++;
                }
                printf(">%d\n", __LINE__);
                break;
            }
            printf(">%d\n", __LINE__);
            *s++ = 0;
            printf(">%d\n", __LINE__);
            if (s < content_end && *s == '\n')
            {
                *s++ = 0;
            }
            printf(">%d\n", __LINE__);
            if (s - res > 0)
            {
                index[index_length++] = res;
            }
            printf(">%d\n", __LINE__);
        }
    }

    printf("read %d lines.\n", (int)index_length);

    printf("[0]%s\n", index[0]);
    printf("[1]%s\n", index[1]);
    printf("[2]%s\n", index[2]);

    /* 1. alphabetic */
    {
        qsort(index, index_length, sizeof(*index), forward_cmp);
        for (size_t i = 0; i < index_length; ++i)
        {
            fputs(index[i], fo);
            putc('\n', fo);
        }
    }
    /* 2. backwards alphabetic */
    {
        qsort(index, index_length, sizeof(*index), backward_cmp);
        for (size_t i = 0; i < index_length; ++i)
        {
            fputs(index[i], fo);
            putc('\n', fo);
        }
    }

    /* 3. copying */
    FILE *fi = fopen(argv[1], "rb");
    {
        char c;
        while ((c = getc(fi)) != EOF)
        {
            putc(c, fo);
        }
    }

    #ifdef _WIN32
        // Cleanup
        UnmapViewOfFile((LPCVOID)content);
        CloseHandle(hMap);
        CloseHandle(hFile);
    #else
        free(content);
        close(fd);
    #endif

    fclose(fo);
    
    return 0;
}
