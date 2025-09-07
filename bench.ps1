pushd $PSScriptRoot
$warmup = 32
$testing = 16
$times = gci bin | % {
    $exe = $_
    $variant = $_.Name-replace".exe$"
    Write-Host "Testing $variant"
    $sw = [System.Diagnostics.Stopwatch]::New()
    1..$warmup | % {
        & $exe ./test.txt ./output.txt | oh
    }
    $stats = 1..$testing | % {
        $sw.Restart()
        & $exe ./test.txt ./output.txt | oh
        $sw.Stop()
        $sw.Elapsed.TotalMilliseconds
    } | measure -AllStats | select A*,M*
    [pscustomobject]@{
        Time = $stats
        Program = $variant
    }
}
.\draw.ps1 -Table $times
popd
