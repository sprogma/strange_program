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

    #ifdef _WIN32
        HANDLE hFile = CreateFileA(argv[1], GENERIC_READ | GENERIC_WRITE,
                   0, //  | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                   NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

        if (hFile == INVALID_HANDLE_VALUE)
        {
            fprintf(stderr, "Cannot find file <%s>\n", argv[1]);
            return 1;
        }
        LARGE_INTEGER fileSizeLi;
        GetFileSizeEx(hFile, &fileSizeLi);

        HANDLE hMap = CreateFileMappingA(hFile, NULL, PAGE_READWRITE | SEC_COMMIT, 0, 0, NULL);

        content = (char *)MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS | FILE_MAP_COPY, 0, 0, 0);
        
        content_length = (SIZE_T)fileSizeLi.QuadPart;

        MEMORY_RANGE_ENTRY range;
        range.VirtualAddress = (PVOID)content;
        range.NumberOfBytes = content_length;
        PrefetchVirtualMemory(GetCurrentProcess(), 1, &range, 0);
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
    #endif
    

    size_t index_length = 0;
    size_t index_alloc = 128;
    char **index = malloc(sizeof(char *) * index_alloc);
    while (1)
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
        char *res = bad_getline(fi);
        if (res == NULL)
        {
            break;
        }
        index[index_length++] = res;
    }

    printf("read %d lines.\n", (int)index_length);

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
    rewind(fi);
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
