#include "stdio.h"

#include "main.h"

/* 
 * find next \n [and \r too, if compiled on windows, return it's position]
 */
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



int create_read_buffer(struct input_buffer_t *buffer, const char *filename, char **pcontent, size_t *pcontent_length)
{
    size_t content_length;
    char *content;

    #ifdef _WIN32
        buffer->hFile = CreateFileA(
            filename,
            GENERIC_READ,
            FILE_SHARE_READ,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );

        if (buffer->hFile == INVALID_HANDLE_VALUE) {
            printf("CreateFile failed: %lu\n", GetLastError());
            return 1;
        }

        LARGE_INTEGER fileSize;
        if (!GetFileSizeEx(buffer->hFile, &fileSize)) {
            printf("GetFileSizeEx failed: %lu\n", GetLastError());
            CloseHandle(buffer->hFile);
            return 1;
        }

        buffer->hMap = CreateFileMappingA(
            buffer->hFile,
            NULL,
            PAGE_WRITECOPY,
            0,
            0,
            NULL
        );

        if (buffer->hMap == NULL) {
            printf("CreateFileMapping failed: %lu\n", GetLastError());
            CloseHandle(buffer->hFile);
            return 1;
        }
        
        content = MapViewOfFile(buffer->hMap, FILE_MAP_COPY, 0, 0, 0);

        if (content == NULL) {
            printf("MapViewOfFile failed: %lu\n", GetLastError());
            CloseHandle(buffer->hMap);
            CloseHandle(buffer->hFile);
            return 1;
        }
        content_length = fileSize.QuadPart;
    #else
        int fd = open(filename, O_RDONLY);
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

    
    #ifdef DEBUG_INFO
        printf("File size: %zu\n", content_length);
    #endif

    *pcontent = content;
    *pcontent_length = content_length;

    buffer->buffer = content;

    return 0;
}


int split_on_lines(struct MSD_array_t **pindex, size_t *pindex_length, char *content, size_t content_length)
{
    char *content_end = content + content_length;
    
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

    *pindex = index;
    *pindex_length = index_length;

    return 0;
}

int free_read_buffer(struct input_buffer_t *buffer)
{
    #ifdef _WIN32
        UnmapViewOfFile((LPCVOID)buffer->buffer);
        CloseHandle(buffer->hMap);
        CloseHandle(buffer->hFile);
    #else
        free(buffer->buffer);
    #endif
}
