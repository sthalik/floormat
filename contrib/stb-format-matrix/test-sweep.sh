#!/bin/bash
# Runs magnum-plugins' own StbImageImporterTest and StbImageConverterTest against
# every variant matrix.cmake builds -- 512 importer masks, 31 converter masks.
# The driver checks decoded bytes; this checks that the upstream test suite still
# passes once formats are taken out, which is what the CORRADE_SKIP guards claim.
#
# Run it through the clang64 wrapper.
set -e

FM="$(cd "$(dirname "$0")/../.." && pwd)"
B="$FM/external/magnum-plugins/build"
LLVM=D:/dev/llvm-23.1.0-rc3/bin
JOBS=${JOBS:-24}
LOG=${LOG:-/tmp/stb-test-sweep.log}

if [ ! -e "$B/build.ninja" ]; then
    cmake -S "$FM/contrib/stb-format-matrix" -B "$B" -GNinja \
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=17 -DCMAKE_CXX_FLAGS=-g0 \
        -DCMAKE_C_COMPILER=$LLVM/cc.exe -DCMAKE_CXX_COMPILER=$LLVM/c++.exe \
        -DCMAKE_LINKER=$LLVM/ld.lld.exe -DCMAKE_RC_COMPILER=$LLVM/windres.exe
fi
ninja -C "$B"

# multithreaded() opens 200k images in two threads and reads no format flag, so
# it is the same work in all 512 importer variants. Run it here, skip it below.
base="$B/bin/StbImageImporter_000_test.exe"
"$base" > "$LOG.base"
skip=$(sed -n 's/^ *OK \[0*\([0-9]*\)\] multithreaded.*/\1/p' "$LOG.base")
if [ -z "$skip" ]; then
    echo "cannot find the multithreaded case number in the baseline run"
    exit 1
fi
echo "baseline passes; skipping case $skip (multithreaded) in the sweep"

run_one() {
    local exe=$1 args=() out
    case "$(basename "$exe")" in
        StbImageImporter_*) args=(--skip "$SKIP");;
    esac
    if ! out=$("$exe" "${args[@]}" 2>&1); then
        printf 'FAIL %s\n%s\n' "$(basename "$exe" .exe)" "$out"
    fi
}
export -f run_one
export SKIP=$skip

total=$(ls "$B"/bin/StbImage*_test.exe | wc -l)
ls "$B"/bin/StbImage*_test.exe |
    xargs -P "$JOBS" -I{} bash -c 'run_one {}' > "$LOG" 2>&1

if [ -s "$LOG" ]; then
    echo "$(grep -c '^FAIL' "$LOG") of $total variants FAILED"
    grep -A6 '^FAIL' "$LOG" | head -60
    echo "(full log: $LOG)"
    exit 1
fi
echo "all $total variants pass the upstream test suite"
