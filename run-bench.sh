#!/bin/sh

set -eu

self="$(basename -- "$0")"
cd -- "$(dirname -- "$0")"

# Sub-nanosecond cases (World_*) measure loop overhead more than code, and Raycast_Dense_Old is
# a retired implementation kept for reference. Neither tells you anything about codegen.
# Merge_Skipped is out for the same reason: it early-outs and reads ~0 ns. Merge_Blocked is in
# because its long streaks make the merge nearly free, so it is the most sensitive fixture to
# anything in the per-quad path.
filter='SpriteBatch_(Merge_(Blocked|Shuffled|Interleaved)|Chunk_|Emit_|Frame)|Raycast$|Raycast_Dense$|Loader_json|Grid_Build|Dijkstra|Critter_move|Bitmask'
reps=5
min_time=0.1s
outdir=build/bench

usage() {
    echo "usage: ${self} <build-dir> <label>      run and log one build" >&2
    echo "       ${self} --compare <label>...     table of previously logged runs" >&2
    echo "options: -r <reps> -t <min_time> -f <filter> -o <outdir>" >&2
    exit 64
}

while test $# -gt 0; do
    case "$1" in
        -r) reps="$2"; shift 2 ;;
        -t) min_time="$2"; shift 2 ;;
        -f) filter="$2"; shift 2 ;;
        -o) outdir="$2"; shift 2 ;;
        *) break ;;
    esac
done

test $# -ge 1 || usage
mkdir -p -- "$outdir"

if test "$1" = --compare; then
    shift
    test $# -ge 1 || usage
    exec python "$(dirname -- "$0")/contrib/bench-report.py" --compare "$outdir" "$@"
fi

test $# -eq 2 || usage
dir="$1"
label="$2"

exe=
for i in "$dir/install/bin/floormat-benchmark.exe" "$dir/install/bin/floormat-benchmark"; do
    if test -x "$i"; then
        exe="$i"
        break
    fi
done
if test -z "$exe"; then
    echo "error: no floormat-benchmark in ${dir}/install/bin" >&2
    exit 65
fi

json="$outdir/$label.json"
log="$outdir/$label.txt"

# floormat chdirs to its install prefix so it can resolve share/floormat, which lands a relative
# --benchmark_out= somewhere unintended. Hand google-benchmark an absolute native path.
case "$OS" in
    Windows_NT) json_arg="$(cygpath -m -- "$PWD")/$json" ;;
    *) json_arg="$PWD/$json" ;;
esac

# Assets resolve relative to the executable, so run it by path and never cd into its directory.
# nice -n -20 is RealTime priority class on Windows; the benchmarks are single-threaded already.
echo "==> $label  ($exe)"
start=$(date +%s)
nice -n -20 "$exe" \
    --benchmark_filter="$filter" \
    --benchmark_repetitions="$reps" \
    --benchmark_min_time="$min_time" \
    --benchmark_report_aggregates_only=true \
    --benchmark_format=json \
    --benchmark_out="$json_arg" \
    --benchmark_out_format=json > "$log" 2>&1 || {
        echo "error: benchmark failed, see $log" >&2
        tail -20 "$log" >&2
        exit 1
    }
end=$(date +%s)

echo "    wall $((end - start))s -> $json"
python "$(dirname -- "$0")/contrib/bench-report.py" --one "$json" "$label"
