#!/bin/sh
#
# Instrumented PGO for the clang build: compile instrumented, run the trainers, merge what
# they wrote, rebuild with -fprofile-use. Run with --help for the verbs and the env knobs.

set -o pipefail
set -eu

case "$OS" in
    Windows_NT) export PATH="$PATH:/usr/bin" ;;
esac

self="$(basename -- "$0")"
cd -- "$(dirname -- "$0")"

# Context-sensitive rounds run by 'use'. The full ladder is generate -> cs x N -> link, so N=1
# already gives two data-generating rounds. That is the ceiling: clang has two instrumentation
# sites, pre-inline (filled by 'generate') and post-inline (filled here), and a build instruments
# one while using the other. A second cs round recompiles to a byte-identical object, because the
# IR records that drive inlining never change after 'generate'. FM_CS_ROUNDS overrides it;
# 0 skips the cs stage entirely, leaving plain one-round PGO.
cs_rounds=${FM_CS_ROUNDS:-1}

# Everything the profile is trained on, cheapest first: a trainer that fails aborts the run
# before the merge, and the editor takes minutes where the other two take seconds.
default_exes="floormat-test floormat-benchmark floormat-editor"

# All three repeat in-process. That is cheaper than restarting them and it keeps startup and
# atlas loading counted once instead of once per pass.
driver_repeat=${FM_DRIVER_REPEAT:-10}
bench_reps=${FM_BENCH_REPS:-3}
bench_min_time=${FM_BENCH_MIN_TIME:-0.05s}
test_repeat=${FM_TEST_REPEAT:-10}

gen_dir=build/clang-pgo-gen
use_dir=build/clang-pgo-use
prof=pgo

usage_text() {
    cat <<EOF
usage: ${self} <all|generate|use|link>... [exe...]
       ${self} --help

Each verb implies the ones before it, so 'link' on its own runs the whole ladder.

  generate  configure ${gen_dir} instrumented, run the trainers, merge
            what they wrote into a fresh profile
  use       generate, then the context-sensitive rounds, each folded into the
            same profile
  link      the above, then the final -fprofile-use build in ${use_dir}
  all       same as link

Trainers default to: ${default_exes}
A name without the floormat- prefix gets one. They run one at a time, in the order
given, and one that fails aborts the run before the merge.

The profile is FLOORMAT_PGO_PROFDATA out of ${gen_dir}'s cache when that
tree exists, else build/${prof}.profdata.

env:
  FM_CS_ROUNDS       context-sensitive rounds in 'use'; 0 for plain PGO  (${cs_rounds})
  FM_DRIVER_REPEAT   editor passes over the driver scene table  (${driver_repeat})
  FM_BENCH_REPS      benchmark repetitions per case  (${bench_reps})
  FM_BENCH_MIN_TIME  benchmark time per case  (${bench_min_time})
  FM_TEST_REPEAT     floormat-test passes  (${test_repeat})

FLOORMAT_PGO is implemented in the Clang userconfig, and llvm-profdata is taken from
beside the compiler named in the cache. On Windows run this under the toolchain
wrapper: clang64 ./${self} all
EOF
}

usage() {
    if test "${1:-64}" -eq 0; then
        usage_text
    else
        usage_text >&2
    fi
    exit "${1:-64}"
}

if test -z "${1:-}"; then
    usage
fi

generate=0
use=0
link=0
exes=

is_command() {
    case "$1" in
        all|generate|use|link|help|-h|--help) return 0 ;;
        *) return 1 ;;
    esac
}

add_exe() {
    case "$1" in
        floormat-*) exes="$exes $1" ;;
        *) exes="$exes floormat-$1" ;;
    esac
}

# basename minus the 'floormat-' prefix and the extension
exe_tag() {
    local _x
    _x="$(basename -- "$1")"
    _x="${_x%.exe}"
    echo "${_x#floormat-}"
}

# Each binary rejects the others' options, so these are per-exe.
# --driver=profile, not =all: without --driver-scenes the mask stays all-ones and driver_tick()
# filters by mode instead, so =all would also train on the coverage-only scenes. Only profile
# makes a driver that bailed out quit instead of idling forever.
exe_args() {
    case "$(exe_tag "$1")" in
        editor) echo "--magnum-gpu-validation=full --vsync=off --driver=profile --driver-repeat $driver_repeat" ;;
        # Instrumented, the benchmark's default 0.5s per case turns one training run into
        # minutes, and PGO reads the counts relative to each other, not their magnitude.
        # Repetitions rather than a longer min_time, because a repetition re-runs the fixture
        # as well, which is what another iteration means here.
        benchmark) echo "--benchmark_min_time=$bench_min_time --benchmark_repetitions=$bench_reps" ;;
        test) echo "--repeat $test_repeat" ;;
    esac
}

# Process restarts, for a binary that has no repeat flag of its own. None has.
exe_runs() {
    case "$(exe_tag "$1")" in
        *) echo 1 ;;
    esac
}

native() {
    case "$OS" in
        Windows_NT) cygpath -m -- "$1" ;;
        *) echo "$1" ;;
    esac
}

cache_get() {
    sed -n "s|^$2:[^=]*=||p" "$1/CMakeCache.txt"
}

# Next to the compiler that produced the profraws, not whatever is on PATH: the raw format is
# versioned against the toolchain, and a mismatched llvm-profdata rejects the file outright.
find_profdata_tool() {
    local _cxx _bin _i
    _cxx="$(cache_get "$1" CMAKE_CXX_COMPILER)"
    _bin="$(dirname -- "$_cxx")/llvm-profdata"
    # --version, not -x: the toolchain links against UCRT DLLs that are only on the wrapper's
    # PATH, so an executable bit says nothing about whether it will start
    for _i in "$_bin".exe "$_bin"; do
        if test -x "$_i" && "$_i" --version >/dev/null 2>&1; then
            echo "$_i"
            return 0
        fi
    done
    if test -x "${_bin}.exe" || test -x "$_bin"; then
        echo "error: ${_bin} will not start; run ${self} under the toolchain wrapper," >&2
        echo "       e.g. 'clang64 ./${self} ...'" >&2
        return 1
    fi
    if command -v llvm-profdata >/dev/null 2>&1; then
        echo "warning: no llvm-profdata beside ${_cxx}, using PATH" >&2
        command -v llvm-profdata
        return 0
    fi
    echo "error: no llvm-profdata beside '${_cxx}' or on PATH" >&2
    return 1
}

# generate and use share one tree, so every stage switch rewrites the compile flags and ninja
# rebuilds everything. That is the cost of the loop, not a bug.
#
# The profile path goes on every configure. The userconfig defaults FLOORMAT_PGO_PROFDATA only
# when unset, so a stale cache value would be used in silence, and the guard
# below would still pass.
configure_tree() {
    echo "==> cmake $1 (FLOORMAT_PGO=$2)"
    cmake -S . -B "$1" -DFLOORMAT_PGO="$2" -DCMAKE_BUILD_TYPE=Release \
          -DFLOORMAT_PGO_PROFDATA="$(native "$profdata")"
}

build_tree() {
    configure_tree "$1" "$2"
    cmake --build "$1" --target install
}

# %m pools per binary signature and adds into whatever file it finds, so a round that reuses
# a signature inherits the previous round's counters and the merge then weights that binary
# twice. Wiped before the trainer runs and again once the merge has consumed them, so nothing
# a round wrote can reach the next one.
wipe_profraws() {
    rm -f -- ./"${gen_dir}"/"${prof}"-*.profraw
}

# resolved fresh every round: the binaries are rebuilt in place between rounds, and on a fresh
# tree they do not exist until the first build
resolve_exes() {
    local resolved exe i
    resolved=
    for exe in $exes; do
        case "${exe}" in
        [a-zA-Z]:/*|[a-zA-Z]:\\*|/*|\\*) : ;;
        *)  for i in ./"${gen_dir}"/install/bin/"${exe}".exe \
                     ./"${gen_dir}"/install/bin/"${exe}"; do
                if test -x "$i"; then
                    exe="$i"
                    break
                fi
            done ;;
        esac
        if ! test -x "${exe}"; then
            echo "error: no '${exe}' executable in ${gen_dir}" >&2
            exit 65
        fi
        resolved="$resolved $exe"
    done
    echo "${resolved# }"
}

# One trainer at a time. google-benchmark sizes its iteration counts from measured wall time, so
# a benchmark sharing the machine with the GPU-bound editor records different counts than one
# that does not, and the run counts are what balances the trainers against each other.
run_exes() {
    local _list profdir exe tag n i
    _list="$(resolve_exes)"
    case "$OS" in
        Windows_NT) profdir="$(cygpath -m -- "$PWD/$gen_dir")" ;;
        *) profdir="$PWD/$gen_dir" ;;
    esac
    for exe in $_list; do
        tag="$(exe_tag "$exe")"
        n="$(exe_runs "$exe")"
        i=1
        while test "$i" -le "$n"; do
            if test "$n" -gt 1; then
                echo "==> ${exe}  (${i}/${n})"
            else
                echo "==> ${exe}"
            fi
            # A trainer that dies takes atexit with it and writes no profile at all, so there
            # is nothing to salvage by going on to the next one.
            if ! LLVM_PROFILE_FILE="$profdir/${prof}-${tag}_%m.profraw" \
                     "$exe" $(exe_args "$exe"); then
                echo "error: ${exe} failed" >&2
                exit 1
            fi
            i=$((i + 1))
        done
    done
}

# "fn=N max=M" for one profile, on a single line
raw_stats() {
    "$1" show "$2" | awk '/^Total functions:/ { printf "fn=%s ", $3 }
                          /^Maximum function count:/ { printf "max=%s", $4 }'
}
# $1: 1 to fold the existing profile back in. A CS round has to, because its profraw holds only
# the post-inline counters -- the IR-level records it was compiled against live in the old file
# and nothing else supplies them. A cold round must not, since it is defined as using no profile.
merge_pool() {
    local raws raw profdata_bin
    raws=
    for raw in ./"${gen_dir}"/"${prof}"-*.profraw; do
        if test -f "$raw"; then
            raws="$raws $raw"
        fi
    done
    if test -z "$raws"; then
        echo "error: no ${prof}-*.profraw in ${gen_dir}" >&2
        exit 65
    fi
    if test "$1" -gt 0 && test -f "$profdata"; then
        raws="$raws $profdata"
    fi
    profdata_bin="$(find_profdata_tool "$gen_dir")" || exit 65
    # The merge is flat, so each trainer's share of the result is whatever its raw counts
    # happen to be and the run counts are the only dial. Without this there is nothing to set
    # them from.
    for raw in $raws; do
        case "$raw" in *.profdata) continue ;; esac
        echo "    ${raw##*/}: $(raw_stats "$profdata_bin" "$raw")"
    done
    # No -sparse. It drops records whose counters are all zero, and PGO reads that case as
    # "instrumented, never ran" and marks the function cold. Absent from the profile means
    # "unknown" instead and keeps static heuristics. Coverage does not care, this does.
    "$profdata_bin" merge $raws -o "${profdata}.tmp"
    mv -f -- "${profdata}.tmp" "$profdata"
    echo "==> $profdata"
    "$profdata_bin" show "$profdata" \
        | grep -E '^(Total functions|Maximum function count|Maximum internal block count)'
}

while test $# -gt 0; do
    case "$1" in
        help|-h|--help) usage 0 ;;
        all) generate=1; use=1; link=1 ;;
        generate) generate=1 ;;
        use) generate=1; use=1 ;;
        link) generate=1; use=1; link=1 ;;
        *) echo "error: invalid command-line argument '$1'" >&2; usage ;;
    esac
    shift
    while test $# -gt 0 && ! is_command "$1"; do
        add_exe "$1"
        shift
    done
done

exes="${exes# }"
if test -z "$exes"; then
    exes="$default_exes"
fi

if ! test -f ./CMakeLists.txt || ! test -f ./shaders/resources.conf; then
    echo "error: ${self} must live in the source root" >&2
    exit 65
fi

profdata=
if test -f "$gen_dir/CMakeCache.txt"; then
    profdata="$(cache_get "$gen_dir" FLOORMAT_PGO_PROFDATA)"
fi
if test -z "$profdata"; then
    profdata="$PWD/build/${prof}.profdata"
fi

if test $generate -gt 0; then
    build_tree "$gen_dir" generate
    wipe_profraws
    run_exes
    merge_pool 0
    wipe_profraws
fi

if test $use -gt 0; then
    round=1
    while test $round -le $cs_rounds; do
        echo "==> context-sensitive round ${round}/${cs_rounds}"
        build_tree "$gen_dir" cs
        wipe_profraws
        run_exes
        merge_pool 1
        wipe_profraws
        round=$((round + 1))
    done
fi

if test $link -gt 0; then
    if ! test -f "$profdata"; then
        echo "error: no profile at '${profdata}', run '${self} generate' first" >&2
        exit 65
    fi
    # assert between configure and build: the userconfig silently falls back to a plain release
    # build when the profile is missing, and finding that out after a full LTO link is expensive
    configure_tree "$use_dir" use
    if ! grep -q -- '-fprofile-use=' "$use_dir/build.ninja"; then
        echo "error: ${use_dir} configured without -fprofile-use, profile not picked up" >&2
        exit 65
    fi
    cmake --build "$use_dir" --target install
fi

exit 0
