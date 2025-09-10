param(
    [string]$File
)

gcc $File -o a -march=native -O3 -g
if ($IsWindows)
{
    ./a.exe
}
else
{
    ./a
}
