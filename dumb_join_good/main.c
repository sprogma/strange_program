#define __USE_MINGW_ANSI_STDIO 1
#include "stdio.h"
#include "malloc.h"
#include "search.h"
#include "string.h"
#include "inttypes.h"


#include "main.h"


int main(int argc, const char **argv)
{
    if (argc != 3)
    {
        fprintf(stderr, "get %d input files instead of 2\n", argc - 1);
        return 1;
    }
    
    char *content;
    char *output_buffer;
    size_t content_length;
    struct MSD_array_t *index;
    size_t index_length;
    struct input_buffer_t read_buffer;
    struct output_buffer_t write_buffer;
    
    create_read_buffer(&read_buffer, argv[1], &content, &content_length);
    create_write_buffer(&write_buffer, argv[2], &output_buffer, content_length * 3 + 64 * 1024);
    split_on_lines(&index, &index_length, content, content_length);
    char *output_buffer_end = output_buffer;
    
    struct MSD_array_t *ttmp = malloc(sizeof(*ttmp) * index_length);

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
        memcpy(output_buffer_end, content, content_length);
    #endif

    

    free_read_buffer(&read_buffer);
    free_write_buffer(&write_buffer, output_buffer_end - output_buffer);
    
    return 0;
}
