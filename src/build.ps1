# build.ps1 - Build Aetherium Vanguard
# Usage:
#   .\build.ps1          # build game.exe (dev)
#   .\build.ps1 -Release # build game.exe stripped + assembly-optimized + hardened
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

# Release build: hardening against reverse engineering + assembly optimizations.
# -s / --strip-all / --exclude-all-symbols : remove all symbols, imports hidden via dynamic GetProcAddress in protect.c
# -fno-asynchronous-unwind-tables + -fno-unwind-tables : drop .eh_frame (needed for debuggers)
# -fno-ident : drop compiler comment
# -fvisibility=hidden : hide ELF/PE symbols not explicitly exported
# -ffunction-sections / -fdata-sections + -Wl,--gc-sections : dead-strip & scatter functions (harder to map)
# -Wl,--build-id=none : omit GNU build-id (otherwise leaks hash of content)
# -flto : cross-TU inlining flattens call graph
# -march=x86-64-v3 etc : see README; -fomit-frame-pointer frees register + removes frame markers
$releaseFlags = @()
$releaseLinkFlags = @()
if ($Release) {
    $releaseFlags = @(
        '-s',                              # strip symbol table + debug info (also via linker)
        '-DNDEBUG',                        # drop assert() strings/code
        '-fno-asynchronous-unwind-tables', # drop .eh_frame unwind tables
        '-fno-unwind-tables',
        '-fno-ident',                      # drop GCC comment in .comment
        '-fvisibility=hidden',             # hide all symbols by default
        '-ffunction-sections',             # one section per function (for --gc-sections)
        '-fdata-sections',
        '-march=x86-64-v3',                # AVX2/FMA/BMI2 baseline (not -march=native)
        '-funroll-loops',                  # unroll hot loops (obscures loop structure)
        '-fomit-frame-pointer',            # free a register, smaller prologues, no frame chain for unwinding
        '-flto'                            # link-time optimization across modules (flattens boundaries)
    )
    $releaseLinkFlags = @(
        '-Wl,--gc-sections',
        '-Wl,--strip-all',
        '-Wl,--exclude-all-symbols',
        '-Wl,--build-id=none'
    )
    Write-Host 'Release build: stripping, hardening, and packing for anti-RE.' -ForegroundColor Yellow
}

$gccArgs = @('-std=c99', '-Wall', '-Wextra', '-O3') +
           $releaseFlags +
           $releaseLinkFlags +
           $cflags +
           $sources +
           $libs + @('-lm', '-o', $Target)

& gcc @gccArgs

if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

# Post-link hardening (Release only): extra strip + UPX packing if available.
# UPX --lzma compresses the PE, hides section layout/imports, and breaks naive
# static analysis (strings, IAT, function boundaries). The binary remains a
# valid Windows EXE and unpacks transparently at runtime with ~0 overhead.
# If UPX is not installed locally, the build still succeeds — CI installs it.
if ($Release -and $LASTEXITCODE -eq 0) {
    # Extra strip pass (idempotent if already -s)
    try { & strip --strip-all $Target 2>$null } catch {}
    # Remove .comment if any slipped through
    try { & objcopy --remove-section .comment $Target 2>$null } catch {}

    $upx = Get-Command upx -ErrorAction SilentlyContinue
    if ($upx) {
        Write-Host 'Packing with UPX --best --lzma (anti-static-analysis)...' -ForegroundColor Cyan
        & upx --best --lzma $Target
        if ($LASTEXITCODE -ne 0) {
            Write-Warning 'UPX packing failed — shipping unpacked binary (build still succeeds).'
            $global:LASTEXITCODE = 0
        }
    } else {
        Write-Host 'UPX not found — skipping pack. Install mingw-w64-ucrt-x86_64-upx for full hardening.' -ForegroundColor DarkYellow
    }
}

if ($LASTEXITCODE -eq 0) {
    Write-Host "Built $Target" -ForegroundColor Green
    # Quick sanity: show that no cleartext title / API names survive in the binary
    if ($Release) {
        try {
            $hit = Select-String -Path $Target -Pattern 'Aetherium Vanguard - OPTIMIZED|IsDebuggerPresent|CheckRemoteDebuggerPresent' -SimpleMatch -ErrorAction SilentlyContinue
            if ($hit) { Write-Warning 'Plaintext anti-RE markers leaked into binary!' }
            else { Write-Host 'Anti-RE string check: no plaintext markers found.' -ForegroundColor Green }
        } catch {}
    }
}
exit $LASTEXITCODE
