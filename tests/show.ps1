param(
    [string]$File
)

gcc $File -o a -march=native -O3 -g -fopenmp
if ($IsWindows)
{
    ./a.exe
}
else
{
    ./a
}
