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
$cflags = (pkg-config --cflags raylib) -split '\s+' | Where-Object { $_ }
if ($LASTEXITCODE -ne 0) { throw 'pkg-config --cflags raylib failed.' }
$libs = (pkg-config --libs raylib) -split '\s+' | Where-Object { $_ }
if ($LASTEXITCODE -ne 0) { throw 'pkg-config --libs raylib failed.' }

$sources = (Get-ChildItem -Path $PSScriptRoot -Filter '*.c' -File).FullName
if (-not $sources) { throw "No .c sources found in $PSScriptRoot." }

$gccArgs = @('-std=c99', '-Wall', '-Wextra', '-O3') +
           $cflags +
           $sources +
           $libs + @('-lm', '-o', $Target)

& gcc @gccArgs

if ($LASTEXITCODE -eq 0) {
    Write-Host "Built $Target" -ForegroundColor Green
}
exit $LASTEXITCODE
