#!/bin/sh
# Build floormat-benchmark under four PGO configurations, then time them all back to back.
#
# Build and measure are separate phases on purpose. Benchmarking a config right after its own
# LTO build measures a busy machine: three runs of one binary spread 0.6pp in geomean when idle,
# but the same binary read 4pp slower immediately after a build. That systematic error is larger
# than the differences between configurations, so each config is snapshotted and everything is
# measured afterwards, twice, with the first pass discarded as warm-up.
#
# Trainer is floormat-benchmark alone, so this trains on what it measures -- nopgo->ir1 is an
# upper bound on what PGO is worth. The ir1/cs1/cs3 comparison stays valid, since all three
# train on the identical workload. Dropping the trainer argument to run-pgo.sh below trains on
# its full set instead, at the cost of an editor driver run per round.
# Every SpriteBatch_* fixture is excluded from training (bounds violation at Arg(4)). The ones
# run-bench.sh's filter keeps, 32 of 35, are an untrained control group.

set -eu
cd -- "$(dirname -- "$0")/../.."

out=build/bench
snap=$out/snap
log="$out/sweep.log"
configs="nopgo ir1 cs1 cs3"
mkdir -p -- "$out" "$snap"
: > "$log"

say() { echo "$@" | tee -a "$log"; }

# PE keeps a 4-byte TimeDateStamp in the COFF header at e_lfanew+8, so two identical builds
# never hash the same. Zero it before comparing.
bin_hash() {
    python -c "
import hashlib,struct,sys
b=bytearray(open(sys.argv[1],'rb').read())
o=struct.unpack_from('<I',b,0x3c)[0]+8
b[o:o+4]=b'\0\0\0\0'
print(hashlib.sha256(bytes(b)).hexdigest()[:16], len(b))
" "$1"
}

# .debug companions are half the tree and nothing runs them
take_snapshot() {
    rm -rf -- "$snap/$1"
    mkdir -p -- "$snap/$1/install"
    cp -r -- build/clang-pgo-use/install/share "$snap/$1/install/share"
    mkdir -p -- "$snap/$1/install/bin"
    for f in build/clang-pgo-use/install/bin/*; do
        case "$f" in *.debug) continue ;; esac
        cp -- "$f" "$snap/$1/install/bin/"
    done
    say "--- $1: binary $(bin_hash "$snap/$1/install/bin/floormat-benchmark.exe")"
}

# Beside the compiler that wrote the profraws, not whatever is on PATH: the raw format is
# versioned against the toolchain. --version rather than -x, because these link against UCRT
# DLLs only the wrapper's PATH has, so an executable bit says nothing about whether it starts.
find_profdata() {
    _fp_cxx="$(sed -n 's|^CMAKE_CXX_COMPILER:[^=]*=||p' build/clang-pgo-use/CMakeCache.txt)"
    for _fp in "$(dirname -- "$_fp_cxx")/llvm-profdata.exe" "$(dirname -- "$_fp_cxx")/llvm-profdata"; do
        if test -x "$_fp" && "$_fp" --version >/dev/null 2>&1; then
            echo "$_fp"
            return 0
        fi
    done
    echo "error: no llvm-profdata that starts beside ${_fp_cxx}" >&2
    return 1
}

# --showcs on a profile without CS records prints 'Total functions: 0' and exits 0, so an empty
# count here means the tool itself failed.
profile_stats() {
    say "--- $1: profile ir=$("$profdata" show build/pgo.profdata \
        | sed -n 's/^Total functions: //p') cs=$("$profdata" show --showcs build/pgo.profdata \
        | sed -n 's/^Total functions: //p')"
}

say "=== build phase  $(date '+%F %T')"

say ">>> nopgo"
cmake -S . -B build/clang-pgo-use -DFLOORMAT_PGO= -DCMAKE_BUILD_TYPE=Release >>"$log" 2>&1
profdata="$(find_profdata)" || exit 65
cmake --build build/clang-pgo-use --target install >>"$log" 2>&1
take_snapshot nopgo

for n in 0 1 3; do
    case $n in 0) label=ir1 ;; *) label=cs$n ;; esac
    say ">>> $label  (FM_CS_ROUNDS=$n)"
    FM_CS_ROUNDS=$n ./run-pgo.sh link benchmark >>"$log" 2>&1
    profile_stats "$label"
    take_snapshot "$label"
done

say "=== measure phase  $(date '+%F %T')  (pass 1 discarded as warm-up)"
for pass in 1 2; do
    for c in $configs; do
        if test $pass -eq 1; then
            ./run-bench.sh -o "$out/warmup" "$snap/$c" "$c" >/dev/null 2>&1
        else
            ./run-bench.sh "$snap/$c" "$c" 2>&1 | tee -a "$log"
        fi
    done
done

say "=== comparison"
./run-bench.sh --compare $configs 2>&1 | tee -a "$log"
say "=== done $(date '+%F %T')"
