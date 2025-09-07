# bestmakefile !
pushd $PSScriptRoot
[void](mkdir bin -F)
gci -E bin -Di | % N* | % -Pa {
    $o = "bin/$_$($IsWindows ?'.exe':'')"
    $f = gci $_/*.c
    if (($f | % L*W*e) -gt (gi $o 2>$null | % L*W*e))
    {
        Write-Host "build $_ -> $o"
        gcc $f -o $o
    }
}
popd
