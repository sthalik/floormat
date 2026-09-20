#!/bin/sh
# Build every kind of PGO this toolchain can do, time them against each other, print the table.
#
#   ./contrib/pgo/pgo-matrix.sh                    build everything, measure, report
#   ./contrib/pgo/pgo-matrix.sh nopgo ir autofdo   only these
#   ./contrib/pgo/pgo-matrix.sh measure report     re-time snapshots already on disk
#
# | label    | what it builds                                                              |
# |----------|-----------------------------------------------------------------------------|
# | nopgo    | plain release, the baseline                                                 |
# | ir       | instrumented IR PGO: one generate round, trainer, -fprofile-use             |
# | cs       | ir plus one context-sensitive round (-fcs-profile-generate, post-inline)    |
# | minsize  | ir plus -pgo-cold-func-opt=minsize, cold functions built as -Oz             |
# | noinl    | ir plus -inline-cold-callsite-threshold=0, cold callsites never inlined     |
# | both     | ir plus minsize and threshold 0 together                                    |
# | fdo0     | -fdebug-info-for-profiling and no profile. Control for autofdo, and the     |
# |          | binary autofdo traces                                                       |
# | autofdo  | sampled: xperf LBR trace of fdo0 -> llvm-profgen -> -fprofile-sample-use    |
# | autofdo2 | second AutoFDO iteration, traced from the autofdo binary                    |
#
# -pgo-cold-func-opt=optsize (-Os) is left out. It is the milder setting, so add it as a
# control if minsize ever costs time: it separates -Oz from size-optimizing cold code at all.
#
# Order matters and is fixed: cs folds into ir's profile, autofdo traces fdo0's binary, and
# autofdo2 traces autofdo's. A config traces the tree as the previous one left it, so nothing
# may rebuild $use_dir in between.
#
# Trained on floormat-editor alone. Adding floormat-benchmark would train on the binary that
# gets timed, and since an unweighted merge weights each input by its sample count, a
# 3s-per-case benchmark run would drown the editor out. FM_TRAINERS takes a list if you want
# the others back; nothing in the pipeline prevents it, sample profiles merge by function name.
#
# The editor opens a window and drives itself through the scene table (--driver=all). Do not
# start a run you are not going to leave alone.
#
# Snapshotting, timing and reporting are delegated to pgo-sweep.sh, which owns the pinning,
# power scheme and cmd-quoting details.

set -eu

self="$(basename -- "$0")"
cd -- "$(dirname -- "$0")/../.."

case "$OS" in
    Windows_NT) export PATH="$PATH:/usr/bin" ;;
esac

out=build/pgo-sweep
snap=$out/snap
gen_dir=${FM_GEN_DIR:-build/clang-pgo-gen}
use_dir=${FM_USE_DIR:-build/clang-pgo-use}
prof=pgo
work=$PWD/$out/matrix.profdata
# absolute like work= above: ninja runs the compiler with cwd=<build dir>, so a
# relative path satisfies cmake's EXISTS check and then fails at every compile.
sample1=$PWD/$out/sample1.prof
sample2=$PWD/$out/sample2.prof
log=$PWD/$out/matrix.log
sweep=contrib/pgo/pgo-sweep.sh

# Everything the profile is trained on. Editor last: it is the long one and the only one that
# can stall, so a problem there does not cost the other two.
trainers=${FM_TRAINERS:-editor}

# Whatever is timed, plus whatever trains. Never 'all', which drags in floormat-anim-crop-tool,
# whose prebuilt OpenCV breaks whenever the toolchain is rebuilt.
targets=${FM_TARGETS:-}
if test -z "$targets"; then
    targets=floormat-benchmark
    for t in $trainers; do
        case " $targets " in
            *" floormat-$t "*) ;;
            *) targets="$targets floormat-$t" ;;
        esac
    done
fi

# Per-binary training arguments. Each binary parses its own options and rejects the others'.
# 3s per case rather than the benchmark default, for sample density -- llvm-profgen asked for
# 1.2x more samples off a 0.05s run.
bench_train=${FM_BENCH_ARGS:---benchmark_min_time=3s}
# In-process, so startup and atlas loading are counted once rather than once per pass.
test_train=${FM_TEST_ARGS:---repeat 10}
# --driver=profile, not =all: without --driver-scenes the mask stays all-ones and driver_tick()
# filters by mode instead, so =all would also train on the coverage-only scenes. It also picks
# the bail-out path:
# driver_stop quits under profile and hands the editor back under all, where an unroutable maze
# then burns the whole timeout. A killed process writes no profile at all -- measured, a
# timeout -k kill leaves no .profraw where the same binary allowed to finish writes one.
# driver-repeat 3, not 1: SDL/GL setup, shader compile and the atlas parse run once per
# process no matter what, so a single pass gives startup its maximum share of a profile that
# is now entirely the editor's. Each further pass cuts that share by ~1/N, and past 3 or 4
# there is nothing left to win.
editor_train=${FM_EDITOR_ARGS:---magnum-gpu-validation=full --vsync=off --driver=profile --driver-repeat 3}
trainer_timeout=${FM_TRAINER_TIMEOUT:-1200}
# Process restarts, for a trainer with no repeat option of its own. None has, now that
# floormat-test takes --repeat.
runs_test=${FM_RUNS_TEST:-1}
# Branch retirements per sample, per trainer: the workloads differ by an order of magnitude in
# length and the sample count scales with 1/interval. At 1048576 a ~45s benchmark run lands just
# over llvm-profgen's density threshold, while floormat-test's ~17s of --repeat 10 comes out
# several times too sparse. llvm-profgen prints the shortfall as "estimated to optimize better
# with Nx more samples" -- divide that trainer's interval by N and trace it once more. Denser
# than needed is not free: .etl size, the dumper pass and profgen time are all linear in the
# sample count while profile quality is not.
trace_interval=${FM_TRACE_INTERVAL:-1048576}
trace_interval_test=${FM_TRACE_INTERVAL_TEST:-65536}

trace_interval_for() {
    case "$1" in
        test) echo "$trace_interval_test" ;;
        *) echo "$trace_interval" ;;
    esac
}

train_args() {
    case "$1" in
        benchmark) echo "$bench_train" ;;
        test) echo "$test_train" ;;
        editor) echo "$editor_train" ;;
        *) echo "" ;;
    esac
}

train_runs() {
    case "$1" in
        test) echo "$runs_test" ;;
        *) echo 1 ;;
    esac
}

# The seven benchmarks that repeat. The SpriteBatch_Merge family pauses and resumes the timer
# around a 64k-quad refill each iteration and its per-test noise has been measured at 1.9x-2.4x,
# which is larger than any PGO effect; Critter_move sits around 1.09x. Including them makes the
# geomean unreadable. FM_FILTER overrides.
stable='Raycast$|Raycast_Dense$|Loader_json|Grid_Build|Dijkstra|Bitmask'
filter=${FM_FILTER:-$stable}
passes=${FM_PASSES:-5}

all_configs="nopgo ir cs minsize noinl both fdo0 autofdo autofdo2"

usage() {
    echo "usage: ${self} [build|measure|report] [config...]" >&2
    echo "       configs: ${all_configs} (default: all)" >&2
    echo "       verbs default to 'build measure report'" >&2
    echo "env: FM_TRAINERS='$trainers' FM_TRAINER_TIMEOUT=$trainer_timeout FM_RUNS_TEST=$runs_test" >&2
    echo "     FM_BENCH_ARGS FM_TEST_ARGS FM_EDITOR_ARGS  per-trainer arguments" >&2
    echo "     FM_TRACE_INTERVAL=$trace_interval FM_TRACE_INTERVAL_TEST=$trace_interval_test" >&2
    echo "     FM_KEEP_ETL=1 to keep traces" >&2
    echo "     FM_FILTER FM_PASSES=$passes  measurement" >&2
    exit 64
}

say() {
    echo "$@"
    echo "$@" >> "$log"
}

# Failures inside a build are otherwise silent, everything being redirected to the log.
run() {
    echo "+ $*" >> "$log"
    if ! "$@" >> "$log" 2>&1; then
        echo "error: failed: $*" >&2
        tail -30 -- "$log" >&2
        exit 65
    fi
}

cache_get() {
    sed -n "s|^$2:[^=]*=||p" "$1/CMakeCache.txt"
}

# Beside the compiler that wrote the profraws: the raw format is versioned against the
# toolchain and a mismatched llvm-profdata rejects the file outright.
find_tool() {
    _cxx="$(cache_get "$gen_dir" CMAKE_CXX_COMPILER)"
    _bin="$(dirname -- "$_cxx")/$1"
    for _i in "$_bin".exe "$_bin"; do
        if test -x "$_i" && "$_i" --version >/dev/null 2>&1; then
            echo "$_i"
            return 0
        fi
    done
    if command -v "$1" >/dev/null 2>&1; then
        command -v "$1"
        return 0
    fi
    echo "error: no $1 beside '${_cxx}' or on PATH; try running under the toolchain wrapper" >&2
    return 1
}

# The flag reaching build.ninja is not enough: the compiler runs with cwd=<build dir>,
# so a relative path here compiles nowhere. Read the path back and resolve it as ninja will.
check_sample_flag() {
    _f="$(grep -o -- '-fprofile-sample-use=[^ ]*' "$1/build.ninja" | head -1)"
    _f="${_f#-fprofile-sample-use=}"
    test -n "$_f" || { echo "error: -fprofile-sample-use never reached build.ninja" >&2; exit 65; }
    ( cd "$1" && test -f "$_f" ) || {
        echo "error: -fprofile-sample-use=$_f does not resolve from $1" >&2; exit 65; }
}

# autofdo2 traces whatever RELEASE/bin holds. Without this, running it after any other
# config silently traces that binary and labels the result an AutoFDO second iteration.
check_traces_autofdo() {
    _want="$(native "$sample1")"
    _mode="$(cache_get "$1" FLOORMAT_PGO)"
    _got="$(cache_get "$1" FLOORMAT_PGO_SAMPLE)"
    if test "$_mode" != sample || test "$_got" != "$_want"; then
        echo "error: autofdo2 must trace the autofdo build, but $1 last built something else" >&2
        echo "       FLOORMAT_PGO=$_mode FLOORMAT_PGO_SAMPLE=$_got" >&2
        echo "       want FLOORMAT_PGO=sample FLOORMAT_PGO_SAMPLE=$_want" >&2
        exit 65
    fi
}

native() {
    case "$OS" in
        Windows_NT) cygpath -m -- "$1" ;;
        *) echo "$1" ;;
    esac
}

# ------------------------------------------------------------------------------ building

# Both cold vars go on every configure, empty included. The trees are reused across configs, so
# a value left in the cache by an earlier one would otherwise carry into the next.
cold=
cold_inline=

configure_tree() {
    _d="$1"; shift
    say "    cmake $_d $*"
    run cmake -S . -B "$_d" -DCMAKE_BUILD_TYPE=Release \
        -DFLOORMAT_PGO_COLD="$cold" -DFLOORMAT_PGO_COLD_INLINE="$cold_inline" "$@"
}

# cmake --install rather than --target install: the latter means 'all', which is both three
# extra LTO links and a dependency on targets this never times.
build_tree() {
    run cmake --build "$1" --target $targets
    run cmake --install "$1"
}

resolve_exe() {
    for _i in "$1/install/bin/floormat-$2.exe" "$1/install/bin/floormat-$2"; do
        if test -x "$_i"; then
            echo "$_i"
            return 0
        fi
    done
    return 1
}

# xperf-trace.sh leaves an unstripped copy in install/bin and the snapshot would carry it
drop_staged() {
    for _t in $trainers; do
        rm -f -- "$use_dir/install/bin/floormat-${_t}-fdo.exe"
    done
    rm -f -- "$use_dir/.fdo-staged"
}

take_snapshot() {
    drop_staged
    FM_SNAP_FROM="$use_dir" sh "$sweep" "snapshot=$1" 2>&1 | tee -a "$log"
}


# %m pools per binary signature and adds into whatever file it finds, so a round that reuses
# a signature inherits the previous round's counters and the merge then weights that binary
# twice. Wiped before the trainer runs and again once the merge has consumed them, so nothing
# a round wrote can reach the next one.
wipe_profraws() {
    rm -f -- ./"${gen_dir}"/"${prof}"-*.profraw
}

# One instrumented pass per trainer, repeated train_runs times. %m pools per binary signature
# so the three never collide and repeats accumulate into the same file, and merge_pool sums
# whatever landed. A trainer that dies contributes nothing rather than breaking the round:
# terminate skips atexit, so an aborting binary writes no profile at all.
run_trainer() {
    _profdir="$(native "$PWD/$gen_dir")"
    _any=0
    for _t in $trainers; do
        _exe="$(resolve_exe "$gen_dir" "$_t")" || {
            say "    skip floormat-${_t}: not in ${gen_dir}/install/bin"
            continue
        }
        _n="$(train_runs "$_t")"
        if test "$_n" -gt 1; then
            say "    train: floormat-${_t} x${_n} $(train_args "$_t")"
        else
            say "    train: floormat-${_t} $(train_args "$_t")"
        fi
        _i=1
        _ok=0
        while test "$_i" -le "$_n"; do
            _st=0
            LLVM_PROFILE_FILE="$_profdir/${prof}-${_t}_%m.profraw" \
                timeout -k 10 "$trainer_timeout" "$_exe" $(train_args "$_t") >>"$log" 2>&1 || _st=$?
            case "$_st" in
                0) _ok=$((_ok + 1)) ;;
                124) say "    !!! floormat-${_t} run ${_i}/${_n} hit the ${trainer_timeout}s timeout" ;;
                *) say "    !!! floormat-${_t} run ${_i}/${_n} exited ${_st}" ;;
            esac
            test "$_st" -eq 0 || break
            _i=$((_i + 1))
        done
        test "$_ok" -eq 0 || _any=1
        test "$_ok" -eq "$_n" || say "    !!! floormat-${_t}: only ${_ok}/${_n} runs contributed"
    done
    test "$_any" -eq 1 || { echo "error: every trainer failed" >&2; exit 65; }
}

# $1 = 1 to fold the existing profile back in, which is what a cs round needs: its profraw
# carries only post-inline records and the pre-inline ones live in the old profile. Merging
# the cs raw alone would pass every guard and then miss on every pre-inline lookup.
merge_pool() {
    _raws=
    for raw in ./"${gen_dir}"/"${prof}"-*.profraw; do
        test -f "$raw" && _raws="$_raws $raw"
    done
    test -n "$_raws" || { echo "error: no ${prof}-*.profraw in ${gen_dir}" >&2; exit 65; }
    if test "$1" -gt 0 && test -f "$work"; then
        _raws="$_raws $work"
    fi
    # No -sparse. It drops all-zero records, and PGO reads present-and-zero as cold but absent
    # as unknown, so sparse silently deletes the cold marking.
    run "$(find_tool llvm-profdata)" merge $_raws -o "${work}.tmp"
    mv -f -- "${work}.tmp" "$work"
}

base_profile_done=0
make_base_profile() {
    test "$base_profile_done" -eq 0 || return 0
    say ">>> instrumented generate round"
    configure_tree "$gen_dir" -DFLOORMAT_PGO=generate -DFLOORMAT_PGO_PROFDATA="$(native "$work")"
    build_tree "$gen_dir"
    wipe_profraws
    run_trainer
    merge_pool 0
    wipe_profraws
    base_profile_done=1
}

make_cs_profile() {
    make_base_profile
    say ">>> context-sensitive round"
    configure_tree "$gen_dir" -DFLOORMAT_PGO=cs -DFLOORMAT_PGO_PROFDATA="$(native "$work")"
    build_tree "$gen_dir"
    wipe_profraws
    run_trainer
    merge_pool 1
    wipe_profraws
}

# One ETW session per trainer, because llvm-profgen resolves addresses against a single
# --binary. The three partial profiles then merge: a sample profile is keyed by mangled
# function name, not by address, so the src/ functions common to all three accumulate.
#
# Traces whatever RELEASE/bin currently holds -- the caller must not have rebuilt $use_dir
# since the binary it wants was linked.
collect_sample() {
    _p="$1"
    _stem="$(basename -- "${_p%.prof}")"
    _parts=
    _n=0
    rm -f -- "$_p"
    for _t in $trainers; do
        resolve_exe "$use_dir" "$_t" >/dev/null || {
            say "    skip floormat-${_t}: not in ${use_dir}/install/bin"
            continue
        }
        _etl="$out/${_stem}-${_t}.etl"
        _part="$PWD/$out/${_stem}-${_t}.prof"
        _runs="$(train_runs "$_t")"
        say ">>> trace floormat-${_t}"
        rm -f -- "$_etl" "$_part"
        FM_TREE="$use_dir" FM_ETL="$_etl" FM_PROFILE="$_part" FM_TIMEOUT="$trainer_timeout" \
            sh contrib/xperf-trace.sh -e "$_t" -i "$(trace_interval_for "$_t")" -n "$_runs" run -- $(train_args "$_t") \
            >>"$log" 2>&1 || { say "    !!! trace of floormat-${_t} failed"; continue; }
        FM_TREE="$use_dir" FM_ETL="$_etl" FM_PROFILE="$_part" \
            sh contrib/xperf-trace.sh -e "$_t" convert >>"$log" 2>&1 \
            || { say "    !!! convert of floormat-${_t} failed"; continue; }
        say "    $(basename -- "$_part") $(stat -c%s "$_part") bytes"
        grep -E "samples written|Sample PGO is estimated" "$log" | tail -2 | sed 's/^/      /'
        _parts="$_parts $_part"
        _n=$((_n + 1))
        # The perf-script text is the same order of magnitude as the .etl it decodes -- 1.5 GB
        # against 2.5 GB for a long editor trace -- and a whole matrix makes six of each. Kept
        # on the failure paths above, where there is a reason to look at them.
        test -n "${FM_KEEP_ETL:-}" || rm -f -- "$_etl" "${_etl%.etl}.perfscript"
    done
    test -n "$_parts" || { echo "error: no trainer produced a sample profile" >&2; exit 65; }
    say "    merging ${_n} sample profiles"
    run "$(find_tool llvm-profdata)" merge --sample $_parts -o "$_p"
    say "    profile $(stat -c%s "$_p") bytes"
}

build_config() {
    say "=== $1  $(date '+%T')"
    cold=
    cold_inline=
    case "$1" in
        nopgo)
            configure_tree "$use_dir" -DFLOORMAT_PGO=
            build_tree "$use_dir" ;;
        ir|minsize|noinl|both)
            # ahead of the cold vars: the instrumented tree has to stay neutral or the
            # profile is collected from a build these flags already changed
            make_base_profile
            case "$1" in
                minsize) cold=minsize ;;
                noinl)   cold_inline=0 ;;
                both)    cold=minsize; cold_inline=0 ;;
            esac
            configure_tree "$use_dir" -DFLOORMAT_PGO=use -DFLOORMAT_PGO_PROFDATA="$(native "$work")"
            build_tree "$use_dir" ;;
        cs)
            make_cs_profile
            configure_tree "$use_dir" -DFLOORMAT_PGO=use -DFLOORMAT_PGO_PROFDATA="$(native "$work")"
            build_tree "$use_dir" ;;
        fdo0)
            rm -f -- "$sample1"
            configure_tree "$use_dir" -DFLOORMAT_PGO=sample \
                           -DFLOORMAT_PGO_SAMPLE="$(native "$sample1")"
            build_tree "$use_dir" ;;
        autofdo)
            for t in $trainers; do
                test -x "$use_dir/RELEASE/bin/floormat-${t}.exe" || {
                    echo "error: autofdo needs fdo0 built first, no floormat-${t}" >&2
                    exit 65; }
            done
            collect_sample "$sample1"
            configure_tree "$use_dir" -DFLOORMAT_PGO=sample \
                           -DFLOORMAT_PGO_SAMPLE="$(native "$sample1")"
            check_sample_flag "$use_dir"
            build_tree "$use_dir" ;;
        autofdo2)
            check_traces_autofdo "$use_dir"
            collect_sample "$sample2"
            configure_tree "$use_dir" -DFLOORMAT_PGO=sample \
                           -DFLOORMAT_PGO_SAMPLE="$(native "$sample2")"
            check_sample_flag "$use_dir"
            build_tree "$use_dir" ;;
        *) echo "error: unknown config '$1'" >&2; exit 64 ;;
    esac
    take_snapshot "$1"
}

# ------------------------------------------------------------------------------ measuring

# A 32-way build starting mid-measurement is what voided the first autofdo2 run: nopgo on an
# unchanged snapshot read 48% slow and the noise column hit 1.75x. Cheap to check for.
busy_count() {
    ps -W 2>/dev/null | grep -icE 'clang\.exe|ninja\.exe|cl\.exe|link\.exe|cc1plus' || true
}

check_idle() {
    _n="$(busy_count)"
    if test "${_n:-0}" -gt 2; then
        say "!!! ${_n} compiler processes running -- timings will be garbage"
        if test -z "${FM_FORCE:-}"; then
            say "    wait for the machine to go idle, or set FM_FORCE=1"
            exit 65
        fi
    fi
}

measure() {
    check_idle
    _before="$(busy_count)"
    FM_CONFIGS="$configs" FM_FILTER="$filter" sh "$sweep" measure 2>&1 | tee -a "$log"
    _after="$(busy_count)"
    if test "${_after:-0}" -gt 2 && test "${_before:-0}" -le 2; then
        say "!!! the machine went busy during measurement (${_before} -> ${_after} processes)."
        say "    re-run './${self} measure report' when idle before believing the table"
    fi
}

# FM_FILTER restricts what the benchmark binary runs, so it only takes effect at measure
# time. --only does the same subsetting at report time, which is what re-reads an older,
# wider measurement down to the benchmarks that repeat.
report() {
    python contrib/pgo/pgo-sweep-report.py --compare "$out" "$passes"         --only="$filter" $configs 2>&1 | tee -a "$log"
}

# ------------------------------------------------------------------------------- dispatch

if ! test -f ./CMakeLists.txt || ! test -f ./shaders/resources.conf; then
    echo "error: ${self} must run from the source root" >&2
    exit 65
fi

do_build=0; do_measure=0; do_report=0; configs=; verbs=0
while test $# -gt 0; do
    case "$1" in
        build) do_build=1; verbs=1 ;;
        measure) do_measure=1; verbs=1 ;;
        report) do_report=1; verbs=1 ;;
        -h|--help) usage ;;
        -*) usage ;;
        *)
            case " $all_configs " in
                *" $1 "*) configs="$configs $1" ;;
                *) echo "error: unknown config '$1'" >&2; usage ;;
            esac ;;
    esac
    shift
done
test "$verbs" -eq 1 || { do_build=1; do_measure=1; do_report=1; }

# canonical order regardless of how they were typed, because the recipes chain
if test -z "$configs"; then
    configs="$all_configs"
else
    _sel="$configs"; configs=
    for c in $all_configs; do
        case " $_sel " in *" $c "*) configs="$configs $c" ;; esac
    done
fi
configs="${configs# }"

mkdir -p -- "$out" "$snap"
: > "$log"
say "=== pgo-matrix $(date '+%F %T')"
say "    configs:  $configs"
say "    trainers: $trainers"
say "    filter:   $filter"
say "    HEAD $(git rev-parse --short HEAD 2>/dev/null || echo '?')"

if test $do_build -gt 0; then
    : > "$out/hashes.txt"
    for c in $configs; do
        build_config "$c"
    done
    # leave the trees where run-pgo.sh expects them, and off any profile this run wrote
    configure_tree "$use_dir" -DFLOORMAT_PGO=
    configure_tree "$gen_dir" -DFLOORMAT_PGO=generate \
                   -DFLOORMAT_PGO_PROFDATA="$(native "$PWD/build/pgo.profdata")"
    wipe_profraws
fi
test $do_measure -eq 0 || measure
test $do_report -eq 0 || report
say "=== done $(date '+%F %T')"
