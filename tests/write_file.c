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

BENCH(file_output)
{
    const char *name;
    size_t size;
    BENCH_OPTION("10mb.txt")     { size = 10*1024*1024; name = "10mb.txt"; }
    BENCH_OPTION("100mb.txt")    { size = 100*1024*1024; name = "100mb.txt"; }
    BENCH_OPTION("1000mb.txt")    { size = 1000*1024*1024; name = "1000mb.txt"; }


    BENCH_VARIANT("fwrite w size=1")
    {
        BENCH_RUN(0, 1)
        {
            BENCH_START
            {
                FILE *f = fopen(name, "w");

                char *s = calloc(size, 1);

                fwrite(s, 1, size, f);

                printf("!\n");

                free(s);
                fclose(f);
            }
            BENCH_END
        }
    }

    BENCH_VARIANT("fwrite wb size=1")
    {
        BENCH_RUN(0, 1)
        {
            BENCH_START
            {
                FILE *f = fopen(name, "wb");

                char *s = calloc(size, 1);

                fwrite(s, 1, size, f);

                printf("!\n");

                free(s);
                fclose(f);
            }
            BENCH_END
        }
    }

    BENCH_VARIANT("fwrite wb size=total")
    {
        BENCH_RUN(0, 1)
        {
            BENCH_START
            {
                FILE *f = fopen(name, "wb");

                char *s = calloc(size, 1);

                fwrite(s, size, 1, f);

                printf("!\n");

                free(s);
                fclose(f);
            }
            BENCH_END
        }
    }

    #ifndef WIN32
    #error TODO
    BENCH_VARIANT("linux [fuuu] write")
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
    BENCH_VARIANT("Winapi WriteFile")
    {
        BENCH_RUN(0, 1)
        {
            BENCH_START
            {
                HANDLE hF = CreateFile(
                    name,
                    GENERIC_WRITE,
                    0, /* exclusive rights */
                    NULL,
                    CREATE_NEW,
                    FILE_ATTRIBUTE_NORMAL,
                    NULL
                    );
                char *s = calloc(size, 1);

                memset(s, 1, size);

                LARGE_INTEGER totalSize;
                WriteFile(hF, s, size, &totalSize.LowPart, NULL);

                printf("!\n");

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

            HANDLE hFile = CreateFileA(name, GENERIC_WRITE,
                       0, /* exclusive rights */ // FILE_SHARE_READ, //  | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                       NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);

            LARGE_INTEGER fileSizeLi;
            GetFileSizeEx(hFile, &fileSizeLi);

            HANDLE hMap = CreateFileMappingA(hFile, NULL, PAGE_READWRITE, 0, size, NULL);

            char *s = (char *)MapViewOfFile(hMap, FILE_MAP_WRITE, 0, 0, 0);

            memset(s, 0, size);

            printf("!\n");

            // Cleanup
            UnmapViewOfFile((LPCVOID)s);
            CloseHandle(hMap);
            CloseHandle(hFile);
            BENCH_END
        }
    }
    #endif
}
