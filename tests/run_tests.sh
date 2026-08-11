#!/usr/bin/env bash
# Build the game logic + test harness with gcov coverage, run the tests, and
# print a per-module plus total line-coverage report.
#
# Requires the same toolchain as the game: MSYS2 UCRT64 gcc + pkg-config +
# raylib headers (see src/README.md). The tests link against a raylib stub
# (tests/raylib_stub.c) so no window or GPU is needed.
#
# Usage: bash tests/run_tests.sh
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

BUILD="$ROOT/tests/build"
rm -rf "$BUILD"
mkdir -p "$BUILD"

CFLAGS="$(pkg-config --cflags raylib)"
WARN="-Wall -Wextra"
BASE="-std=c99 -O0 -g --coverage -DRAYMATH_STATIC_INLINE"

MODULES=(utils waves enemies hero towers projectiles update vfx draw ui main)

echo "Compiling game modules..."
for m in "${MODULES[@]}"; do
    extra=()
    if [ "$m" = "main" ]; then extra=(-Dmain=game_renamed_main); fi
    gcc $BASE $WARN "${extra[@]}" $CFLAGS -I"$ROOT/src" -I"$ROOT/tests" \
        -c "$ROOT/src/$m.c" -o "$BUILD/$m.o"
done

echo "Compiling test harness..."
gcc $BASE $WARN $CFLAGS -I"$ROOT/src" -I"$ROOT/tests" \
    -c "$ROOT/tests/raylib_stub.c" -o "$BUILD/raylib_stub.o"
for t in test_main test_utils_main test_hero test_towers test_enemies \
         test_projectiles test_waves_update test_vfx test_draw_ui; do
    gcc $BASE $WARN $CFLAGS -I"$ROOT/src" -I"$ROOT/tests" \
        -c "$ROOT/tests/$t.c" -o "$BUILD/$t.o"
done

gcc --coverage "$BUILD"/*.o -lm -o "$BUILD/tests.exe"
echo "Running tests..."
"$BUILD/tests.exe"

echo
echo "---- Line coverage (gcov) ----"
cd "$BUILD"
total=0
covered=0
for m in "${MODULES[@]}"; do
    # gcov also prints lines for raymath.h and a per-invocation summary; the
    # source module itself is always reported first.
    out=$(gcov -o . "$m.gcda" 2>/dev/null | grep 'Lines executed' | head -1 || true)
    pct=$(echo "$out" | sed -n 's/.*Lines executed:\([0-9.]*\)% of \([0-9]*\).*/\1/p')
    tot=$(echo "$out" | sed -n 's/.*Lines executed:\([0-9.]*\)% of \([0-9]*\).*/\2/p')
    if [ -z "$tot" ]; then
        printf "%-12s no coverage data\n" "$m.c"
        continue
    fi
    c=$(awk -v p="$pct" -v t="$tot" 'BEGIN { printf "%.0f", t*p/100 }')
    total=$((total + tot))
    covered=$((covered + c))
    printf "%-12s %6.2f%%  (%s/%s lines)\n" "$m.c" "$pct" "$c" "$tot"
done
if [ "$total" -gt 0 ]; then
    pct=$(awk -v c="$covered" -v t="$total" 'BEGIN { printf "%.2f", 100*c/t }')
    echo "---------------------------------------------"
    printf "TOTAL        %6.2f%%  (%s/%s lines)\n" "$pct" "$covered" "$total"
    echo
    awk -v c="$covered" -v t="$total" 'BEGIN {
        if (100*c/t >= 80) { print "Target met: >= 80% coverage" }
        else { print "WARNING: coverage below 80% target" }
    }'
fi
