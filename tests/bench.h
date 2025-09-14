#include "math.h"
#include "time.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

#ifdef WIN32
    #include "windows.h"
    #include "realtimeapiset.h"
#endif

#include "setjmp.h"
#include "signal.h"

/* best benchmark library ! [prototype] */

#define TABLE_VARIANT_COLUMN_SIZE 22
#define TABLE_OPTION_COLUMN_SIZE 16
#define COLOR_OUTPUT
#define SHOW_TOTAL_AVERAGE_TIME
#define SHOW_TOTAL_WORST_TIME
#define SHOW_LITTLE_TIME_IN_MICROSECONDS
#define USE_LOG_SCALE_IN_GISTOGRAM

#define RUN_WARMUP_GLOBAL_SCALE 1
#define RUN_MEASURES_GLOBAL_SCALE 1
// #define RUN_WARMUP_GLOBAL_SCALE 2
// #define RUN_MEASURES_GLOBAL_SCALE 2

/* not works on linux */
// #define MIN_INSTEAD_OF_MEAN


typedef void (*bench_fn)();
void complete_benchmark(void (*fn)(), const char *testing_name);

bench_fn __attribute__((section(".bench_section"))) __attribute__((used)) wrapper_end_address = NULL;

#define BENCH(name) \
    void bench_##name(); \
    void wrapper_##name() \
    { \
        complete_benchmark(bench_##name, #name); \
    } \
    bench_fn __attribute__((section(".bench_section"))) __attribute__((used)) wrapper_address##name = &wrapper_##name; \
    void bench_##name()
#define BENCH_OPTION(string) \
    if (current_option == NULL) \
    { \
        option_id++; \
        option_names[option_id] = string; \
    } \
    for(;current_option == NULL && !option_completed[option_id];current_option = string, option_completed[option_id] = 1)
#ifdef WIN32
    #ifdef MIN_INSTEAD_OF_MEAN
        #define RESET_COUNTER bestbench_counter_total = 0x7fffffffffffffffll;
    #else
        #define RESET_COUNTER bestbench_counter_total = 0;
    #endif
#else
    #define RESET_COUNTER bestbench_counter_total = 0.0;
#endif
#define BENCH_VARIANT(string) \
    variant_id++; \
    variant_names[variant_id] = string; \
    RESET_COUNTER; \
    if (!variant_completed[variant_id] && current_option == NULL) \
    { \
        abort(); \
    } \
    for (;!variant_completed[variant_id];current_variant = string, abort())
#define BENCH_RUN(warm, measure) \
            for (bench_run = -RUN_WARMUP_GLOBAL_SCALE * (warm); bench_run < RUN_MEASURES_GLOBAL_SCALE * (measure); ++bench_run)
#ifdef MIN_INSTEAD_OF_MEAN
    #ifdef WIN32
        #define BENCH_START \
                    if (bench_run >= 0) \
                    { \
                        QueryPerformanceCounter(&bestbench_counter_storage); \
                        bestbench_counter = -bestbench_counter_storage.QuadPart; \
                    }
        #define BENCH_END \
                    if (bench_run >= 0) \
                    { \
                        QueryPerformanceCounter(&bestbench_counter_storage); \
                        bestbench_counter += bestbench_counter_storage.QuadPart; \
                        if (bestbench_counter_total > bestbench_counter) \
                        { \
                            bestbench_counter_total = bestbench_counter; \
                        } \
                    }
    #else
        #error cannot use min instead of mean on linux
    #endif
#else
    #ifdef WIN32
        #define BENCH_START \
                    if (bench_run >= 0) \
                    { \
                        QueryPerformanceCounter(&bestbench_counter_storage); \
                        bestbench_counter_total -= bestbench_counter_storage.QuadPart; \
                    }
        #define BENCH_END \
                    if (bench_run >= 0) \
                    { \
                        QueryPerformanceCounter(&bestbench_counter_storage); \
                        bestbench_counter_total += bestbench_counter_storage.QuadPart; \
                    }
    #else
        #define BENCH_START \
                    if (bench_run >= 0) \
                    { \
                        clock_gettime(CLOCK_MONOTONIC, &bestbench_counter_storage); \
                        bestbench_counter_total -= (double)bestbench_counter_storage.tv_sec + (double)bestbench_counter_storage.tv_nsec / 1e9; \
                    }
        #define BENCH_END \
                    if (bench_run >= 0) \
                    { \
                        clock_gettime(CLOCK_MONOTONIC, &bestbench_counter_storage); \
                        bestbench_counter_total += (double)bestbench_counter_storage.tv_sec + (double)bestbench_counter_storage.tv_nsec / 1e9; \
                    }
    #endif
#endif

int option_id = 0;
int variant_id = 0;
int bench_run = 0;
const char *current_variant = NULL;
const char *current_option = NULL;
const char *option_names[64] = {};
const char *variant_names[64] = {};
/* variant option */
double measurements_best[64] = {};
double measurements_worst[64] = {};
double measurements[64][64] = {};
int option_completed[64] = {};
int variant_completed[64] = {};
long long bestbench_counter = 0;
#ifdef WIN32
    LARGE_INTEGER bestbench_counter_storage;
    long long bestbench_counter_total = 0;
#else
    struct timespec bestbench_counter_storage;
    double bestbench_counter_total = 0;
#endif
volatile int do_not_optimize = 0;
// use longjmp to return index of table there is current option and variant


#ifdef WIN32
jmp_buf buf;
#else
sigjmp_buf buf;
#endif

void handler(int sig)
{
    if (sig == SIGABRT)
    {
        if (!current_option)
        {
            variant_completed[variant_id] = 1;
            memset(option_completed, 0, sizeof(option_completed));
        }
        else
        {
            #ifdef WIN32
                QueryPerformanceFrequency(&bestbench_counter_storage);
                measurements[variant_id][option_id] = (double)bestbench_counter_total / (double)bestbench_counter_storage.QuadPart
            #else
                measurements[variant_id][option_id] = bestbench_counter_total
            #endif
            #ifdef MIN_INSTEAD_OF_MEAN
                ;
            #else
                / (double)bench_run;
            #endif
            bestbench_counter = 0;
        }
        option_id = 0;
        #ifdef WIN32
        longjmp(buf, 0);
        #else
        siglongjmp(buf, 0);
        #endif
    }
}



void complete_benchmark(void (*fn)(), const char *testing_name)
{
    memset(option_names, 0, sizeof(option_names));
    memset(variant_names, 0, sizeof(variant_names));
    memset(option_completed, 0, sizeof(option_completed));
    memset(variant_completed, 0, sizeof(variant_completed));
    memset(measurements, 0, sizeof(measurements));

    printf("Benchmarking %s...\n", testing_name);

    #ifdef WIN32
    int result = setjmp(buf);
    #else
    int result = sigsetjmp(buf, 1);
    #endif
    (void)result;
    
    if (signal(SIGABRT, &handler) == SIG_ERR)
    {
        printf("ERROR: cannot set handler on SIGABRT\n");
        exit(1);
    }

    /* reset data */
    current_option = NULL;
    current_variant = NULL;
    option_id = 0;
    variant_id = 0;
    fn();

    /* draw image */
    {
        FILE *f = fopen("bench_data_generated.csv", "w");
        
        fprintf(f, "Category");
        for (int i = 1; variant_names[i]; ++i)
        {
            putc(';', f);
            fprintf(f, "%s", variant_names[i]);
        }
        putc('\n', f);
        for (int i = 1; option_names[i]; ++i)
        {
            fprintf(f, "%s", option_names[i]);
            for (int j = 1; variant_names[j]; ++j)
            {
                putc(';', f);
                fprintf(f, "%lg", measurements[j][i] * 1e3);
            }
            putc('\n', f);
        }

        fclose(f);
        f = fopen("./bench_plot_generated.gp", "w");
        fprintf(f, R"code(
set encoding utf8
set datafile separator ";"

# this works, but print image on top left connner, and rewrite all content in terminal.
# set term sixel enhanced font "Arial,14"
set terminal pngcairo size 1600,900 enhanced font "Arial,14"
set output "bench_histogram_generated.png"

set title "function '%s' results"
set xlabel ""
set ylabel "time (ms)"

set yrange [0:*]

set style data histogram
set style histogram clustered gap 1
set style fill solid 0.8 border -1
set boxwidth 0.8
)code", testing_name);
        #ifdef USE_LOG_SCALE_IN_GISTOGRAM
        fprintf(f, "set logscale y");
        #endif
        fprintf(f, R"code(
    
set style line 1 lc rgb "#666666" lt 1 lw 1
set grid ytics linestyle 1

set key outside right top vertical

set xtics rotate by -90
# set xtic rotate by 0 scale 0

plot 'bench_data_generated.csv' using 2:xtic(1) title columnheader(2), \
)code");
        for (int i = 2; variant_names[i]; ++i)
        {
            fprintf(f, "                ''         using %d        title columnheader(%d), %c\n", i + 1, i + 1, (variant_names[i + 1] ? '\\' : ' '));
        }
        fprintf(f, R"code(
    set key off
set format y "%%g"
ncols = 2
delta = 0.18
        )code");
        fclose(f);

        
        system("gnuplot ./bench_plot_generated.gp");
        #ifdef WIN32
        system("pwsh -c see bench_histogram_generated.png");
        // remove("./bench_histogram_generated.png");
        #endif
        remove("./bench_plot_generated.gp");
        remove("./bench_data_generated.csv");
    }

    
    puts("measurements:");
    for (int x = 0; x < TABLE_VARIANT_COLUMN_SIZE; ++x)
    {
        putchar('-');
    }
    for (int i = 1; option_names[i]; ++i)
    {
        for (int x = 0; x < TABLE_OPTION_COLUMN_SIZE; ++x)
        {
            putchar('-');
        }
    }
    putchar('\n');
    for (int x = 0; x < TABLE_VARIANT_COLUMN_SIZE; ++x)
    {
        putchar(' ');
    }
    for (int i = 1; option_names[i]; ++i)
    {
        printf("%*.*s", (int)TABLE_OPTION_COLUMN_SIZE, (int)TABLE_OPTION_COLUMN_SIZE, option_names[i]);
    }
    #ifdef SHOW_TOTAL_WORST_TIME
        printf("%*.*s", (int)TABLE_OPTION_COLUMN_SIZE, (int)TABLE_OPTION_COLUMN_SIZE, "Worst");
    #endif
    #ifdef SHOW_TOTAL_AVERAGE_TIME
        printf("%*.*s", (int)TABLE_OPTION_COLUMN_SIZE, (int)TABLE_OPTION_COLUMN_SIZE, "Avrg");
    #endif
    putchar('\n');

    #ifdef COLOR_OUTPUT
        #define CHECK_COL(col, tim) \
            { \
                double r, g, d = sqrt(measurements_worst[col] - measurements_best[col] > 0.0000001 ? ((tim) - measurements_best[col]) / (measurements_worst[col] - measurements_best[col]) : 0.5); \
                r = (d > 0.5 ? 1.0 : d * 2.0); \
                g = (d > 0.5 ? 2.0 - d * 2.0 : 1.0); \
                if ((tim) == measurements_best[col]) \
                { \
                    printf("\033[38;2;%d;%d;%d;48;2;%d;%d;%dm", (int)(255.9 * r), (int)(255.9 * g), 0, 0, 32 + (col == 0) * 32, 0); \
                } \
                else if ((tim) == measurements_worst[col]) \
                { \
                    printf("\033[38;2;%d;%d;%d;48;2;%d;%d;%dm", (int)(255.9 * r), (int)(255.9 * g), 0, 32 + (col == 0) * 32, 0, 0); \
                } \
                else \
                { \
                    printf("\033[38;2;%d;%d;%d;48;2;%d;%d;%dm", (int)(255.9 * r), (int)(255.9 * g), 0, 0, 0, 0); \
                } \
            }
        #define CHECK_COL_END(...) \
            printf("\033[0m \b");
    #else
        #define CHECK_COL(...)
        #define CHECK_COL_END(...)
    #endif
    
    #ifdef SHOW_LITTLE_TIME_IN_MICROSECONDS
    #define PRINT_TIME(col, tim) \
        CHECK_COL(col, tim) \
        if ((tim) * 1e3 < 10.0) \
        { \
            printf("%*.3lf us", (int)TABLE_OPTION_COLUMN_SIZE - 3, (tim) * 1e6); \
        } \
        else \
        { \
            printf("%*.3lf ms", (int)TABLE_OPTION_COLUMN_SIZE - 3, (tim) * 1e3); \
        } \
        CHECK_COL_END()
    #else
    #define PRINT_TIME(tim) \
        printf("%*.3lf ms", (int)TABLE_OPTION_COLUMN_SIZE - 3, (tim) * 1e3); \
        CHECK_COL_END()
    #endif

    memset(measurements_worst, 0, sizeof(measurements_worst));
    for (size_t j = 0; j < sizeof(measurements_best) / sizeof(*measurements_best); ++j)
    {
        measurements_best[j] = INFINITY;
    }
    
    for (int i = 1; variant_names[i]; ++i)
    {
        #ifdef SHOW_TOTAL_WORST_TIME
            double worst = 0.0;
        #endif
        #ifdef SHOW_TOTAL_AVERAGE_TIME
            double avrg = 0.0;
        #endif
        for (int j = 1; option_names[j]; ++j)
        {
            if (measurements[i][j] > measurements_worst[j])
            {
                measurements_worst[j] = measurements[i][j];
            }
            if (measurements[i][j] < measurements_best[j])
            {
                measurements_best[j] = measurements[i][j];
            }
            #ifdef SHOW_TOTAL_WORST_TIME
                if (measurements[i][j] > worst)
                {
                    worst = measurements[i][j];
                }
            #endif
            #ifdef SHOW_TOTAL_AVERAGE_TIME
                avrg += measurements[i][j];
            #endif
        }
        #ifdef SHOW_TOTAL_WORST_TIME
            if (worst > measurements_worst[0])
            {
                measurements_worst[0] = worst;
            }
            if (worst < measurements_best[0])
            {
                measurements_best[0] = worst;
            }
        #endif
        #ifdef SHOW_TOTAL_AVERAGE_TIME
            if (avrg > measurements_worst[((int)sizeof(measurements_best)/sizeof(*measurements_best)) - 1])
            {
                measurements_worst[((int)sizeof(measurements_best)/sizeof(*measurements_best)) - 1] = avrg;
            }
            if (avrg < measurements_best[((int)sizeof(measurements_best)/sizeof(*measurements_best)) - 1])
            {
                measurements_best[((int)sizeof(measurements_best)/sizeof(*measurements_best)) - 1] = avrg;
            }
        #endif
    }

    for (int i = 1; variant_names[i]; ++i)
    {
        printf("%-*.*s", (int)TABLE_VARIANT_COLUMN_SIZE, (int)TABLE_VARIANT_COLUMN_SIZE, variant_names[i]);
        double worst = 0.0;
        #ifdef SHOW_TOTAL_AVERAGE_TIME
            double avrg = 0.0;
        #endif
        for (int j = 1; option_names[j]; ++j)
        {
            #ifdef SHOW_TOTAL_WORST_TIME
                if (measurements[i][j] > worst)
                {
                    worst = measurements[i][j];
                }
            #endif
            #ifdef SHOW_TOTAL_AVERAGE_TIME
                avrg += measurements[i][j];
            #endif
            PRINT_TIME(j, measurements[i][j])
        }
        #ifdef SHOW_TOTAL_WORST_TIME
            PRINT_TIME(0, worst)
        #endif
        #ifdef SHOW_TOTAL_AVERAGE_TIME
            PRINT_TIME(((int)sizeof(measurements_best)/sizeof(*measurements_best)) - 1, avrg)
        #endif
        putchar('\n');
    }
    puts("\n\n");
}


#define CALL_MACRO2(name) wrapper_ ## name ();
#define CALL_MACRO(bench) CALL_MACRO2(bench)
#define DECLARE_MACRO2(name) void wrapper_ ## name ();
#define DECLARE_MACRO(bench) DECLARE_MACRO2(bench)

#ifdef BENCH_FUNCTION
DECLARE_MACRO(BENCH_FUNCTION)
#else
    #error Auto function collection not supported. Define BENCH_FUNCTION macro.
#endif

int cnt = 0;
int main(void)
{
    /* iterate functions */
    #ifdef BENCH_FUNCTION
    CALL_MACRO(BENCH_FUNCTION)
    #else
        #error Auto function collection not supported. Define BENCH_FUNCTION macro.
    #endif
    // bench_fn *fn = &wrapper_end_address;
    // bench_fn *start = (bench_fn *)(((size_t)&wrapper_end_address) & ~0xFFF);
    // while (fn >= start)
    // {
    //     if (*fn)
    //     {
    //         (*fn)();
    //     }
    //     fn--;
    // }

    return 0;
}
