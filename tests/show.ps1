param(
    [string]$File
)

gcc $File -o a.exe -march=native -O3 -g
./a.exe
