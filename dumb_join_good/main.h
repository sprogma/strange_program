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


struct input_buffer_t
{
    #ifdef _WIN32
        HANDLE hFile;
        HANDLE hMap;
        char *buffer;
    #else
        char *buffer;
    #endif
};

struct output_buffer_t
{
    #ifdef _WIN32
        HANDLE hFile;
        HANDLE hMap;
        char *buffer;
    #else
        FILE *file;
        char *buffer;
    #endif
};


int create_read_buffer(struct input_buffer_t *buffer, const char *filename, char **content, size_t *content_length);
int free_read_buffer(struct input_buffer_t *buffer);
int create_write_buffer(struct output_buffer_t *buffer, const char *filename, char **output_buffer, size_t buffer_length);
int free_write_buffer(struct output_buffer_t *buffer, size_t bytes_to_write);
int split_on_lines(struct MSD_array_t **index, size_t *index_length, char *content, size_t content_length);
