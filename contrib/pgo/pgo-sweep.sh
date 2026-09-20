#!/bin/sh
# Sweep the number of PGO data-generating rounds and time each one as an uninstrumented
# -fprofile-use build. pgo-matrix.sh delegates its snapshotting, timing and reporting here.
#
# Round 0 is 'generate' -- it is a build plus an instrumented trainer run exactly like every
# later round, so it counts as a round. Rounds 1..R-1 are 'cs'. All rounds share round 0's
# profile, which is why the stages are driven here rather than by looping run-pgo.sh: 'link'
# implies 'use' implies 'generate', and generate's merge overwrites rather than folds, so
# each config would get a different seed and round count would be confounded with sampling.

set -eu

self="$(basename -- "$0")"
cd -- "$(dirname -- "$0")/../.."

rounds=${FM_ROUNDS:-5}
passes=${FM_PASSES:-5}
reps=${FM_REPS:-7}
min_time=${FM_MIN_TIME:-0.25s}
warmup=${FM_WARMUP:-0.05}
# Percent system-wide CPU load above which measuring is refused.
max_load=${FM_MAX_LOAD:-15}
# 0xC0 = logical 6,7 = physical core 3, both SMT siblings, on the 96 MB CCD0 die. A mask
# covering only one sibling leaves the other free for the OS to schedule against us, and a
# mask spanning 5,6 straddles two cores. Core 3 rather than core 0, which takes more DPCs.
affinity=${FM_AFFINITY:-C0}
trainer_args=${FM_TRAINER_ARGS:---benchmark_min_time=0.05s --benchmark_filter=-SpriteBatch_}

# Ultimate Performance: min=max=100% with autonomous mode off, so the OS pins the performance
# state instead of letting CPPC pick per-moment. The machine's 'Locked CPU - Bench' scheme also
# disables idle, which leaves the pinned core's SMT sibling spinning instead of halting -- worse
# for a single-threaded pinned run, not better. Empty disables the switch entirely.
power_scheme=${FM_POWER_SCHEME:-e9a42b02-d5df-448d-aa00-03f14749eb61}
# What to land on afterwards. Default is whatever was active at startup; set it to
# 381b4222-f694-41f0-9685-ff5bb260df2e to end on Balanced regardless.
power_restore=${FM_POWER_RESTORE:-}

out=build/pgo-sweep
snap=$out/snap
gen_dir=build/clang-pgo-gen
use_dir=build/clang-pgo-use
prof=pgo
work=$PWD/$out/work.profdata
log=$out/sweep.log

case "$OS" in
    Windows_NT) export PATH="$PATH:/usr/bin" ;;
esac

usage() {
    echo "usage: ${self} [build|snapshot=LABEL|measure|report]...  (default: build measure report)" >&2
    echo "env: FM_SNAP_FROM=<build dir> for snapshot= (default $use_dir)" >&2
    echo "env: FM_ROUNDS=$rounds FM_PASSES=$passes FM_REPS=$reps FM_MIN_TIME=$min_time" >&2
    echo "     FM_AFFINITY=$affinity (hex mask) FM_MAX_LOAD=$max_load FM_FORCE=1 to override" >&2
    echo "     FM_POWER_SCHEME=$power_scheme FM_POWER_RESTORE=${power_restore:-<active at startup>}" >&2
    exit 64
}

say() { echo "$@" | tee -a "$log"; }

# ---------------------------------------------------------------- from run-pgo.sh, verbatim

cache_get() {
    sed -n "s|^$2:[^=]*=||p" "$1/CMakeCache.txt"
}

# Next to the compiler that produced the profraws, not whatever is on PATH: the raw format is
# versioned against the toolchain, and a mismatched llvm-profdata rejects the file outright.
find_profdata_tool() {
    _cxx="$(cache_get "$1" CMAKE_CXX_COMPILER)"
    _bin="$(dirname -- "$_cxx")/llvm-profdata"
    for _i in "$_bin".exe "$_bin"; do
        if test -x "$_i" && "$_i" --version >/dev/null 2>&1; then
            echo "$_i"
            return 0
        fi
    done
    if command -v llvm-profdata >/dev/null 2>&1; then
        echo "warning: no llvm-profdata beside ${_cxx}, using PATH" >&2
        command -v llvm-profdata
        return 0
    fi
    echo "error: no llvm-profdata beside '${_cxx}' or on PATH" >&2
    return 1
}


# %m pools per binary signature and adds into whatever file it finds, so a round that reuses
# a signature inherits the previous round's counters and the merge then weights that binary
# twice. Wiped before the trainer runs and again once the merge has consumed them, so nothing
# a round wrote can reach the next one.
wipe_profraws() {
    rm -f -- ./"${gen_dir}"/"${prof}"-*.profraw
}

# $1: 1 to fold the existing profile back in. A CS round has to, because its profraw holds only
# the post-inline counters -- the IR-level records it was compiled against live in the old file
# and nothing else supplies them. A cold round must not, since it is defined as using no profile.
merge_pool() {
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
    if test "$1" -gt 0 && test -f "$work"; then
        raws="$raws $work"
    fi
    profdata_bin="$(find_profdata_tool "$gen_dir")" || exit 65
    # No -sparse. It drops records whose counters are all zero, and PGO reads that case as
    # "instrumented, never ran" and marks the function cold. Absent from the profile means
    # "unknown" instead and keeps static heuristics. Coverage does not care, this does.
    "$profdata_bin" merge $raws -o "${work}.tmp"
    mv -f -- "${work}.tmp" "$work"
}

# ---------------------------------------------------------------------------- power plan

# powercfg is a native exe, so MSYS rewrites a /-leading argument into a Windows path. The
# doubled slash is what survives that as a switch -- see the opposite case in run_pinned, where
# MSYS2_ARG_CONV_EXCL turns the collapse off and single slashes are required instead.
power_push() {
    test -n "$power_scheme" || return 0
    if ! command -v powercfg >/dev/null 2>&1; then
        say "note: no powercfg, leaving the power plan alone"
        power_restore=
        return 0
    fi
    _cur=$(powercfg //getactivescheme 2>/dev/null | grep -oE '[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}' | head -1)
    test -n "$power_restore" || power_restore="$_cur"
    trap power_pop EXIT INT TERM
    if test "$_cur" = "$power_scheme"; then
        say "--- power scheme already $power_scheme"
    elif powercfg //setactive "$power_scheme" 2>/dev/null; then
        say "--- power scheme $_cur -> $power_scheme"
    else
        say "!!! powercfg //setactive $power_scheme failed, continuing on $_cur"
    fi
}

power_pop() {
    test -n "${power_restore:-}" || return 0
    _want=$power_restore
    power_restore=
    if powercfg //setactive "$_want" 2>/dev/null; then
        say "--- power scheme restored to $_want"
    else
        say "!!! powercfg //setactive $_want failed; the machine is still on $power_scheme"
    fi
}

# ---------------------------------------------------------------------------- build phase

configure_tree() {
    echo "==> cmake $1 (FLOORMAT_PGO=$2 PROFDATA=${3:-})" >>"$log"
    if test -n "${3:-}"; then
        cmake -S . -B "$1" -DFLOORMAT_PGO="$2" -DFLOORMAT_PGO_PROFDATA="$3" \
              -DCMAKE_BUILD_TYPE=Release >>"$log" 2>&1
    else
        cmake -S . -B "$1" -DFLOORMAT_PGO="$2" -DCMAKE_BUILD_TYPE=Release >>"$log" 2>&1
    fi
}

# Only floormat-benchmark is ever timed, so building 'install' (which means 'all') spends
# three extra LTO links per config and makes the sweep depend on targets it does not use --
# floormat-anim-crop-tool links a prebuilt OpenCV that goes stale whenever the toolchain is
# rebuilt, and its failure would abort a sweep for reasons unrelated to PGO. Build the one
# target, then run the install step on its own so install/share and install/bin get populated.
targets=${FM_TARGETS:-floormat-benchmark}
build_tree() {
    configure_tree "$@"
    cmake --build "$1" --target $targets >>"$log" 2>&1
    cmake --install "$1" >>"$log" 2>&1
}

resolve_bench() {
    for _i in "$1/install/bin/floormat-benchmark.exe" "$1/install/bin/floormat-benchmark"; do
        if test -x "$_i"; then
            echo "$_i"
            return 0
        fi
    done
    echo "error: no floormat-benchmark in $1/install/bin" >&2
    return 1
}

# Unpinned on purpose: PGO reads the counts relative to each other, not their magnitude,
# so pinning the trainer would only make it slower.
run_trainer() {
    _exe="$(resolve_bench "$gen_dir")" || exit 65
    case "$OS" in
        Windows_NT) _profdir="$(cygpath -m -- "$PWD/$gen_dir")" ;;
        *) _profdir="$PWD/$gen_dir" ;;
    esac
    LLVM_PROFILE_FILE="$_profdir/${prof}-benchmark_%m.profraw" \
        "$_exe" $trainer_args >>"$log" 2>&1
}

# A plain hash of the image is useless: the COFF TimeDateStamp, every debug-directory entry's
# TimeDateStamp, and the .gnu_debuglink CRC32 (which covers the equally-stamped .debug file) all
# move between two builds of identical inputs. Mask those three and the link is reproducible --
# verified by a clean rebuild from the same profile, which differed in exactly 8 bytes, all of
# them in those fields.
bin_hash() {
    python contrib/pgo/pe-stable-hash.py "$1" | cut -d' ' -f1
}

# .debug companions are half the tree and nothing runs them
take_snapshot() {
    _from=${snap_from:-$use_dir}
    rm -rf -- "$snap/$1"
    mkdir -p -- "$snap/$1/install/bin"
    cp -r -- "$_from/install/share" "$snap/$1/install/share"
    for f in "$_from"/install/bin/*; do
        case "$f" in *.debug) continue ;; esac
        cp -- "$f" "$snap/$1/install/bin/"
    done
    _h=$(bin_hash "$snap/$1/install/bin/floormat-benchmark.exe")
    _dup=$(sed -n "s/^\([^ ]*\) $_h\$/\1/p" "$out/hashes.txt" 2>/dev/null | head -1)
    echo "$1 $_h" >> "$out/hashes.txt"
    if test -n "$_dup"; then
        say "--- $1: binary $_h  (identical to $_dup)"
    else
        say "--- $1: binary $_h"
    fi
    # Defect 4 of the hand-driven attempt was timing binaries that were still instrumented.
    # -fprofile-use alone pulls in nothing from libclang_rt.profile, so any __llvm_profile string
    # means -fprofile-generate or -fcs-profile-generate leaked into this link.
    if command -v strings >/dev/null 2>&1; then
        _hits=$(strings -a "$snap/$1/install/bin/floormat-benchmark.exe" | grep -c __llvm_profile || true)
        if test "${_hits:-0}" -gt 0; then
            say "!!! $1: instrumented binary ($_hits __llvm_profile strings), refusing to time it"
            exit 65
        fi
    fi
}

profile_stats() {
    _pd="$(find_profdata_tool "$gen_dir")" || return 0
    say "--- $1: profile level=$("$_pd" show "$2" | sed -n 's/^Instrumentation level: //p') \
ir=$("$_pd" show "$2" | sed -n 's/^Total functions: //p') \
cs=$("$_pd" show --showcs "$2" 2>/dev/null | sed -n 's/^Total functions: //p')"
}

# The check that catches every variant of "the profile was silently ignored": recompile one
# hot TU against the profile and count the metadata it produced. An unprofiled build of
# search-astar.cpp emits ~5 branch_weights and 0 function_entry_count.
check_annotation() {
    _n=$(python contrib/pgo/pgo-sweep-report.py --annotation "$use_dir" src/search-astar.cpp) || {
        say "!!! $1: annotation check could not run"
        return 0
    }
    say "--- $1: annotation $_n"
    case "$_n" in
        *UNPROFILED*|branch_weights=[0-5]\ *)
            say "!!! $1: profile annotated nothing -- see plan, defects 1-3"
            exit 65 ;;
    esac
}

link_and_snapshot() {
    build_tree "$use_dir" use "$2"
    if ! grep -q -- '-fprofile-use=' "$use_dir/build.ninja"; then
        echo "error: ${use_dir} configured without -fprofile-use" >&2
        exit 65
    fi
    check_annotation "$1"
    take_snapshot "$1"
}

build_phase() {
    mkdir -p -- "$out" "$snap"
    : > "$out/hashes.txt"
    say "=== build phase  $(date '+%F %T')   R=$rounds"

    say ">>> nopgo"
    build_tree "$use_dir" "" ""
    take_snapshot nopgo

    rm -f -- "$work"
    wipe_profraws

    _r=0
    while test $_r -lt "$rounds"; do
        if test $_r -eq 0; then
            say ">>> r0  (generate)"
            build_tree "$gen_dir" generate "$work"
            wipe_profraws
            run_trainer
            merge_pool 0
            wipe_profraws
        else
            say ">>> r$_r  (cs round $_r)"
            build_tree "$gen_dir" cs "$work"
            wipe_profraws
            run_trainer
            merge_pool 1
            wipe_profraws
        fi
        cp -- "$work" "$out/P$_r.profdata"
        profile_stats "r$_r" "$out/P$_r.profdata"
        link_and_snapshot "r$_r" "$PWD/$out/P$_r.profdata"
        _r=$((_r + 1))
    done

    # FLOORMAT_PGO_PROFDATA is a cache variable and sticks. Left pointing into build/pgo-sweep,
    # a later run-pgo.sh would read it back through cache_get and train the wrong profile.
    configure_tree "$gen_dir" generate "$PWD/build/pgo.profdata"
    configure_tree "$use_dir" "" "$PWD/build/pgo.profdata"
}

# -------------------------------------------------------------------------- measure phase

# Same rows sweep.log used, so nopgo-vs-r0 is comparable against its 0.955x. Sub-nanosecond
# cases (World_*) measure loop overhead more than code, and Raycast_Dense_Old is retired.
filter=${FM_FILTER:-'SpriteBatch_Merge_(Shuffled|Interleaved)|Raycast$|Raycast_Dense$|Loader_json|Grid_Build|Dijkstra|Critter_move|Bitmask'}

config_list() {
    # explicit label list, for timing configs the round sweep does not produce
    if test -n "${FM_CONFIGS:-}"; then
        echo "$FM_CONFIGS"
        return
    fi
    _c=nopgo
    _i=0
    while test $_i -lt "$rounds"; do
        _c="$_c r$_i"
        _i=$((_i + 1))
    done
    echo "$_c"
}

# Assets resolve relative to the executable, so run it by path and never cd into its
# directory; floormat then chdirs to its install prefix, which is why --benchmark_out must be
# an absolute native path.
run_pinned() {
    _exe="$1"; _json="$2"; _rlog="$3"
    # start reads the first quoted token as a window title, so the exe path goes in unquoted
    # and must not contain a space. -s asks for the 8.3 name, which removes spaces on volumes
    # that still generate one. Quoting the path instead makes start take it as the title and
    # then fail on the next token with "The system cannot execute the specified program"; an
    # empty "" title token is worse, because inside a single /c string cmd turns it into a UNC
    # "\\" and Explorer raises a modal error.
    _exe_w="$(cygpath -w -s -- "$_exe")"
    case "$_exe_w" in
        *\ *)
            echo "error: space in '$_exe_w' and no 8.3 name; start cannot take a quoted exe" >&2
            exit 65 ;;
    esac
    _json_w="$(cygpath -m -- "$PWD/$_json")"

    # Flags go through the environment, not argv. cmd re-parses everything after start, so a
    # filter containing '|' becomes a pipe. Every google-benchmark flag has a BENCHMARK_<FLAG>
    # equivalent (BM_DEFINE_string -> StringFromEnv, commandlineflags.h:33), which cmd never
    # touches. MSYS2_ARG_CONV_EXCL='*' also switches off the '//' -> '/' collapse, hence the
    # single slashes. /v:on plus !ERRORLEVEL! is what carries start /wait's exit status back
    # out: %ERRORLEVEL% would expand when cmd parses the line, before the child ran.
    _st=0
    BENCHMARK_FILTER="$filter" \
    BENCHMARK_REPETITIONS="$reps" \
    BENCHMARK_MIN_TIME="$min_time" \
    BENCHMARK_MIN_WARMUP_TIME="$warmup" \
    BENCHMARK_REPORT_AGGREGATES_ONLY=true \
    BENCHMARK_FORMAT=json \
    BENCHMARK_OUT="$_json_w" \
    BENCHMARK_OUT_FORMAT=json \
    MSYS2_ARG_CONV_EXCL='*' cmd /v:on /c \
        "start /affinity $affinity /high /b /wait $_exe_w & exit /b !ERRORLEVEL!" \
        >"$_rlog" 2>&1 || _st=$?
    if test $_st -ne 0; then
        echo "error: benchmark exited $_st" >&2
        tail -20 -- "$_rlog" >&2
        exit 1
    fi
    # Also parse the JSON: it catches a run that exited 0 having written nothing.
    if ! python -c "import json,sys; d=json.load(open(sys.argv[1])); raise SystemExit(0 if d.get('benchmarks') else 1)" \
             "$_json" 2>/dev/null; then
        echo "error: no usable benchmark JSON at $_json" >&2
        tail -20 -- "$_rlog" >&2
        exit 1
    fi
}

# Counting compiler processes misses an idle-looking desktop entirely. A browser, an IDE and an
# antivirus scan read 85% CPU between them and inflated a pinned single-core run by 45%, which
# voided the 2026-08-25 re-measure. Ask the OS for the number instead of inferring it.
cpu_load() {
    case "$OS" in
        Windows_NT)
            powershell.exe -NoProfile -NonInteractive -Command                 "(Get-CimInstance Win32_Processor | Measure-Object -Property LoadPercentage -Average).Average"                 2>/dev/null | tr -d '' | head -1
            ;;
        *) echo "" ;;
    esac
}

# Returns nothing when the load is unreadable, and an unreadable load must not block a run.
check_idle() {
    _l="$(cpu_load)"
    case "${_l:-x}" in
        *[!0-9]*) say "--- cpu load unavailable"; return 0 ;;
    esac
    say "--- cpu load ${_l}%"
    test "$_l" -le "$max_load" && return 0
    say "!!! ${_l}% cpu load, over FM_MAX_LOAD=${max_load}% -- timings will be garbage"
    test -n "${FM_FORCE:-}" || {
        say "    wait for the machine to go quiet, or set FM_FORCE=1"
        exit 65
    }
}

# Configs are interleaved within a pass rather than run back to back, so the spread across
# passes estimates between-invocation noise instead of drift. Pass 1 is warm-up and the
# reporter drops it.
measure_phase() {
    _cfgs="$(config_list)"
    check_idle
    say "=== measure phase  $(date '+%F %T')  passes=$passes reps=$reps min_time=$min_time affinity=0x$affinity"
    _p=1
    while test $_p -le "$passes"; do
        mkdir -p -- "$out/pass$_p"
        for c in $_cfgs; do
            _exe="$(resolve_bench "$snap/$c")" || exit 65
            run_pinned "$_exe" "$out/pass$_p/$c.json" "$out/pass$_p/$c.txt"
        done
        say "    pass $_p/$passes  $(date '+%T')  load $(cpu_load)%"
        _p=$((_p + 1))
    done
}

report_phase() {
    say "=== comparison  (pass 1 discarded)"
    python contrib/pgo/pgo-sweep-report.py --compare "$out" "$passes" $(config_list) 2>&1 | tee -a "$log"
}

# ------------------------------------------------------------------------------- dispatch

if ! test -f ./CMakeLists.txt || ! test -f ./shaders/resources.conf; then
    echo "error: ${self} must run from the source root" >&2
    exit 65
fi

do_build=0
do_measure=0
do_report=0
do_snapshot=
snap_from=${FM_SNAP_FROM:-}
if test $# -eq 0; then
    do_build=1; do_measure=1; do_report=1
fi
while test $# -gt 0; do
    case "$1" in
        build) do_build=1 ;;
        snapshot=*) do_snapshot="${1#snapshot=}" ;;
        measure) do_measure=1 ;;
        report) do_report=1 ;;
        *) usage ;;
    esac
    shift
done

mkdir -p -- "$out" "$snap"
test -f "$log" || : > "$log"

if test $do_build -gt 0 || test $do_measure -gt 0; then
    power_push
fi

if test $do_build -gt 0; then
    : > "$log"
    build_phase
fi
if test -n "$do_snapshot"; then
    take_snapshot "$do_snapshot"
fi
if test $do_measure -gt 0; then
    measure_phase
fi
if test $do_report -gt 0; then
    report_phase
fi
say "=== done $(date '+%F %T')"
