#include "stdio.h"
#include "errno.h"

#ifdef WIN32
    #include "windows.h"

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

#include "bench.h"

BENCH(file_input)
{
    const char *name;
    BENCH_OPTION("10mb.txt")     { name = "10mb.txt"; }
    BENCH_OPTION("100mb.txt")    { name = "100mb.txt"; }


    BENCH_VARIANT("fread r size=1")
    {
        BENCH_RUN(0, 1)
        {
            BENCH_START
            {
                FILE *f = fopen(name, "r");
                
                fseek(f, 0, SEEK_END);
                size_t size = ftell(f);
                rewind(f);
                
                char *s = calloc(2 * size, 1);
                size_t total = fread(s, 1, 2 * size, f);

                printf("%d\n", (unsigned int)s[total - 1]);

                free(s);
                fclose(f);
            }
            BENCH_END
        }
    }

    BENCH_VARIANT("fread rb size=1")
    {
        BENCH_RUN(0, 1)
        {
            BENCH_START
            {
                FILE *f = fopen(name, "rb");
                
                fseek(f, 0, SEEK_END);
                size_t size = ftell(f);
                rewind(f);
                
                char *s = calloc(size, 1);
                (void)fread(s, 1, size, f);

                printf("%d\n", (unsigned int)s[size - 1]);

                free(s);
                fclose(f);
            }
            BENCH_END
        }
    }

    BENCH_VARIANT("fread rb size=total")
    {
        BENCH_RUN(0, 1)
        {
            BENCH_START
            {
                FILE *f = fopen(name, "rb");
                
                fseek(f, 0, SEEK_END);
                size_t size = ftell(f);
                rewind(f);
                
                char *s = calloc(size, 1);

                (void)fread(s, size, 1, f);

                printf("%d\n", (unsigned int)s[size - 1]);

                free(s);
                fclose(f);
            }
            BENCH_END
        }
    }

    #ifndef WIN32    
    BENCH_VARIANT("linux [fuuu] read")
    {
        BENCH_RUN(0, 1)
        {
            BENCH_START
            {
                int fd = open(name, O_RDONLY);
                
                struct stat st;
                
                fstat(fd, &st);
                off_t file_size = st.st_size;
                char *buffer = calloc(file_size, 1);

                ssize_t bytes_read;
                size_t total_bytes_read = 0;
                while ((bytes_read = read(fd, buffer + total_bytes_read, file_size - total_bytes_read)) > 0) 
                {
                    total_bytes_read += bytes_read;
                }

                if (bytes_read == -1)
                {
                    exit(1);
                }

                printf("%d\n", (unsigned int)buffer[total_bytes_read - 1]);

                free(buffer);
                close(fd);
            }
            BENCH_END
        }
    }
    #endif
    
    #ifdef WIN32
    BENCH_VARIANT("Winapi ReadFile single")
    {
        BENCH_RUN(0, 1)
        {
            BENCH_START
            {
                HANDLE hF = CreateFile(
                    name,
                    GENERIC_READ,
                    FILE_SHARE_READ,
                    NULL,
                    OPEN_EXISTING,
                    FILE_ATTRIBUTE_NORMAL,
                    NULL
                    );

                LARGE_INTEGER fileSize, totalSize;
                if (!GetFileSizeEx(hF, &fileSize)) 
                {
                    CloseHandle(hF);
                    return;
                }
                
                char *s = calloc(fileSize.QuadPart, 1);

                ReadFile(hF, s, fileSize.QuadPart, &totalSize.LowPart, NULL);

                printf("%d\n", (unsigned int)s[totalSize.QuadPart - 1]);

                free(s);                
                CloseHandle(hF);
            }
            BENCH_END
        }
    }
    #endif

    #ifdef WIN32
    BENCH_VARIANT("Winapi CreateFileMapping")
    {
        BENCH_RUN(0, 1)
        {
            BENCH_START    

            HANDLE hFile = CreateFileA(name, GENERIC_READ,
                       FILE_SHARE_READ, //  | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                       NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

            LARGE_INTEGER fileSizeLi;
            GetFileSizeEx(hFile, &fileSizeLi);

            HANDLE hMap = CreateFileMappingA(hFile, NULL, PAGE_READONLY, 0, 0, NULL);

            const char *s = (const char *)MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
            SIZE_T size = (SIZE_T)fileSizeLi.QuadPart;

            
            MEMORY_RANGE_ENTRY range;
            range.VirtualAddress = (PVOID)s;
            range.NumberOfBytes = size;
            PrefetchVirtualMemory(GetCurrentProcess(), 1, &range, 0);
            
            printf("%d\n", (unsigned int)s[size - 1]);

            // Cleanup
            UnmapViewOfFile((LPCVOID)s);
            CloseHandle(hMap);
            CloseHandle(hFile);
            BENCH_END
        }
    }
    #endif

    #ifdef WIN32
    BENCH_VARIANT("Winapi CreateFileMapping + copy")
    {
        BENCH_RUN(0, 1)
        {
            BENCH_START    

            HANDLE hFile = CreateFileA(name, GENERIC_READ,
                       FILE_SHARE_READ, //  | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                       NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

            LARGE_INTEGER fileSizeLi;
            GetFileSizeEx(hFile, &fileSizeLi);

            HANDLE hMap = CreateFileMappingA(hFile, NULL, PAGE_READONLY, 0, 0, NULL);

            const char *s = (const char *)MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
            SIZE_T size = (SIZE_T)fileSizeLi.QuadPart;
            char *ss = calloc(size, 1);

            
            MEMORY_RANGE_ENTRY range;
            range.VirtualAddress = (PVOID)s;
            range.NumberOfBytes = size;
            PrefetchVirtualMemory(GetCurrentProcess(), 1, &range, 0);
            memcpy(ss, s, size);
            UnmapViewOfFile((LPCVOID)s);
            
            printf("%d\n", (unsigned int)ss[size - 1]);

            // Cleanup
            CloseHandle(hMap);
            CloseHandle(hFile);
            BENCH_END
        }
    }
    #endif
}
