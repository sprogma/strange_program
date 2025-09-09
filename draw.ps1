param(
  [Parameter(Mandatory=$true)] [PSObject[]] $Table
)

pushd $PSScriptRoot

$Table | % {$id = 0}{
  "$(($id++)) $($_.Time.Average) $($_.Time.Minimum) $($_.Time.Maximum)"
} >results.dat
$Table | % {$id = 0}{
  "$(($id++)) ""$($_.Program)"""
} >resultsNames.dat

$Table | % {[pscustomobject]@{Program=$_.Program; Average=$_.Time.Average; Minimum=$_.Time.Minimum; Maximum=$_.Time.Maximum}}

& {gnuplot plot.gp; Write-Output "Generated result.png"} || Write-Error "Failed to run gnuplot."

popd
