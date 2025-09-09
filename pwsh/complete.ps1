param(
    [string]$Path,
    [string]$Output
)
$i = gc $Path
($i | sort) >$Output
($i | sort {$a = $_.ToCharArray(); [array]::reverse($a); [string]::new($a)}) >>$Output
(gc $Path -Raw) >>$Output
