#include "stdlib.h"
#include "inttypes.h"
#include "string.h"

#include "main.h"


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
    /* copy back first group, it is ended strings */
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


