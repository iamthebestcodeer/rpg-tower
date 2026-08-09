# build.ps1 - Build Aetherium Vanguard
# Usage:
#   .\build.ps1          # build game.exe
#   .\build.ps1 -Clean   # remove build artifacts

param(
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'
$Target = 'game.exe'

if ($Clean) {
    Remove-Item -Force $Target -ErrorAction SilentlyContinue
    Write-Host 'Cleaned.' -ForegroundColor Green
    exit 0
}

# Requires gcc + pkg-config (MSYS2 UCRT64) on PATH
$cflags = (pkg-config --cflags raylib).Split(' ')
$libs   = (pkg-config --libs raylib).Split(' ')

$gccArgs = @('-std=c99', '-Wall', '-Wextra', '-O2') +
           $cflags +
           (Get-ChildItem -Filter '*.c').FullName +
           $libs + @('-lm', '-o', $Target)

& gcc @gccArgs

if ($LASTEXITCODE -eq 0) {
    Write-Host "Built $Target" -ForegroundColor Green
}
exit $LASTEXITCODE
