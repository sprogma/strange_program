#include "bench.h"

BENCH(strlen)
{
    int n;
    // BENCH_OPTION("n=1")      { n = 1; }
    BENCH_OPTION("n=10")     { n = 10; }
    BENCH_OPTION("n=100")    { n = 100; }
    BENCH_OPTION("n=1e3")    { n = 1000; }
    BENCH_OPTION("n=1e5")    { n = 100000; }
    BENCH_OPTION("n=1e7")    { n = 10000000; }
    char *s = calloc(n, 1);
    if (n != 1)
    {
        memset(s, 'a', n - 1);
    }
    BENCH_VARIANT("gnu lib")
    {
        BENCH_RUN(256, 512)
        {
            BENCH_START
            do_not_optimize += strlen(s);
            BENCH_END
        }
        free(s);
    }
    BENCH_VARIANT("gnu lib x2")
    {
        BENCH_RUN(256, 512)
        {
            BENCH_START
            do_not_optimize += strlen(s);
            do_not_optimize += strlen(s + 1);
            BENCH_END
        }
        free(s);
    }
    BENCH_VARIANT("gnu lib x3")
    {
        BENCH_RUN(256, 512)
        {
            BENCH_START
            do_not_optimize += strlen(s);
            do_not_optimize += strlen(s + 2);
            do_not_optimize += strlen(s + 3);
            BENCH_END
        }
        free(s);
    }
    BENCH_VARIANT("gnu lib x4")
    {
        BENCH_RUN(256, 512)
        {
            BENCH_START
            do_not_optimize += strlen(s);
            do_not_optimize += strlen(s + 1);
            do_not_optimize += strlen(s + 2);
            do_not_optimize += strlen(s + 3);
            BENCH_END
        }
        free(s);
    }
}

BENCH(memcpy)
{
    int n;
    BENCH_OPTION("n=1")      { n = 1; }
    BENCH_OPTION("n=10")     { n = 10; }
    BENCH_OPTION("n=100")    { n = 100; }
    BENCH_OPTION("n=1e3")    { n = 1000; }
    BENCH_OPTION("n=1e5")    { n = 100000; }
    BENCH_OPTION("n=1e7")    { n = 10000000; }
    char *s = calloc(n, 1);
    char *d = calloc(n, 1);
    if (n != 1)
    {
        memset(s, 'a', n - 1);
    }
    BENCH_VARIANT("gnu lib")
    {
        BENCH_RUN(32, 256)
        {
            BENCH_START
            memcpy(d, s, n);
            BENCH_END
        }
        do_not_optimize += d[n - 1];
        free(s);
        free(d);
    }
    BENCH_VARIANT("gnu lib x2")
    {
        BENCH_RUN(32, 256)
        {
            BENCH_START
            memcpy(d, s, n);
            memcpy(d, s, n);
            BENCH_END
        }
        do_not_optimize += d[n - 1];
        free(s);
        free(d);
    }
}
