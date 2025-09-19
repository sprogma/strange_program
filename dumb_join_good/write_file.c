#include "stdio.h"

#include "main.h"

int create_write_buffer(struct output_buffer_t *buffer, const char *filename, char **poutput_buffer, size_t buffer_length)
{
    char *output_buffer;
    #if defined(_WIN32)
        buffer->hFile = CreateFileA(
            filename,
            GENERIC_READ | GENERIC_WRITE,
            0, // Exclusive read
            NULL,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );
        if (buffer->hFile == INVALID_HANDLE_VALUE)
        {
            printf("CreateFile error %ld\n", GetLastError());
            return 0;
        }
        // size_t min_size = GetLargePageMinimum();
        // buffer_length += min_size - (result_length % min_size);
        // printf("Min=%zu\n", min_size);
        // printf("Res=%zu\n", buffer_length);
        LARGE_INTEGER buffer_size = {.QuadPart = buffer_length};
        HANDLE hMapFile = CreateFileMapping(
            buffer->hFile,
            NULL,
            PAGE_READWRITE
            | SEC_COMMIT /* sec commit is enabled by default */
            // | SEC_NOCACHE /* only slow down program */
            // | SEC_LARGE_PAGES /* can't use it with file on disk? */
            ,
            buffer_size.HighPart,
            buffer_size.LowPart,
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
        buffer->file = fopen(filename, "wb");
        if (buffer->file == NULL)
        {
            fprintf(stderr, "Cannot find file <%s>\n", argv[1]);
            return 1;
        }
        output_buffer = malloc(content_length * 3);
    #endif

    *poutput_buffer = output_buffer;
    buffer->buffer = output_buffer;
    
    return 0;
}

int free_write_buffer(struct output_buffer_t *buffer, size_t bytes_to_write)
{
    #ifdef _WIN32
        (void)bytes_to_write;
        //TODO : truncate resulting file on bytes_to_write bytes.
        UnmapViewOfFile((LPCVOID)buffer->buffer);
        CloseHandle(buffer->hMap);
        CloseHandle(buffer->hFile);
    #else
        #error Error: check this code
        free(buffer->buffer);
        close(buffer->file);
    #endif
}
