pushd $PSSciptRoot
write-host -- gcc ./unicode/main.c libgrapheme.dll -o ./bin/unicode.exe -Ofast
gcc ./unicode/main.c libgrapheme.dll -o ./bin/unicode.exe -Ofast -Iunicode
popd
