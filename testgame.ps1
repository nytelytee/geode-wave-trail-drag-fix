param (
   [switch]$force = $false,
   [switch]$dontrun = $false
)

$sw = [Diagnostics.Stopwatch]::StartNew()

if ($force -And (Test-Path -Path build)) {
	Remove-Item -Path .\build\ -Force -Recurse
}

if (-Not (Test-Path -Path build)) {
	cmake -B build #-A win32
}

cmake --build build --config RelWithDebInfo
if ($? -And (-Not $dontrun)) {
	taskkill /IM GeometryDash.exe
	steam steam://rungameid/322170
}

$sw.Stop()

Write-Output "`nTook $($sw.Elapsed.TotalSeconds)s."
