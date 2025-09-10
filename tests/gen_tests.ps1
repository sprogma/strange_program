$size = 10MB
$bytes = New-Object byte[] $size
(New-Object Random).NextBytes($bytes)
[System.IO.File]::WriteAllBytes("10mb.txt", $bytes)
$size = 100MB
$bytes = New-Object byte[] $size
(New-Object Random).NextBytes($bytes)
[System.IO.File]::WriteAllBytes("100mb.txt", $bytes)
$size = 1000MB
$bytes = New-Object byte[] $size
(New-Object Random).NextBytes($bytes)
[System.IO.File]::WriteAllBytes("1000mb.txt", $bytes)
