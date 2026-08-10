# build.ps1 - Build Aetherium Vanguard
# Usage:
#   .\build.ps1          # build game.exe (dev)
#   .\build.ps1 -Release # build game.exe stripped + assembly-optimized
#   .\build.ps1 -Clean   # remove build artifacts

param(
    [switch]$Clean,
    [switch]$Release
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

# Release build: strip all debug info and symbol tables and enable
# assembly-level optimizations. -march=x86-64-v3 (AVX2/FMA/BMI2) is a safe
# baseline for any CPU since ~2015; -march=native is intentionally NOT used
# because the GitHub runner can emit AVX-512 code that would crash on older
# machines. -flto optimizes across the split .c modules at link time.
$releaseFlags = @()
if ($Release) {
    $releaseFlags = @(
        '-s',                              # strip symbol table + debug info
        '-fno-asynchronous-unwind-tables', # drop .eh_frame unwind tables
        '-march=x86-64-v3',                # AVX2/FMA/BMI2 instruction sets
        '-funroll-loops',                  # unroll hot loops
        '-fomit-frame-pointer',            # free a register, smaller prologues
        '-flto'                            # link-time optimization across modules
    )
    Write-Host 'Release build: stripping debug info, enabling assembly optimizations.' -ForegroundColor Yellow
}

$gccArgs = @('-std=c99', '-Wall', '-Wextra', '-O3') +
           $releaseFlags +
           $cflags +
           $sources +
           $libs + @('-lm', '-o', $Target)

& gcc @gccArgs

if ($LASTEXITCODE -eq 0) {
    Write-Host "Built $Target" -ForegroundColor Green
}
exit $LASTEXITCODE
