#!/bin/sh
#
# Collect an AutoFDO sample profile on Windows: xperf LBR trace, perf-script text, llvm-profgen,
# and the .prof that FLOORMAT_PGO=sample consumes. Run with --help for the verbs and the knobs.

set -eu

self="$(basename -- "$0")"
cd -- "$(dirname -- "$0")/.."

case "$OS" in
    Windows_NT) export PATH="$PATH:/usr/bin" ;;
esac

wpt=${FM_WPT:-/c/Program Files (x86)/Windows Kits/10/Windows Performance Toolkit}
tree=${FM_TREE:-build/clang-release}
exe=${FM_EXE:-editor}
etl=${FM_ETL:-build/sample.etl}
prof=${FM_PROFILE:-build/sample.prof}
# Available on this PMU; the name in Microsoft's SPGO doc, BranchInstructionRetired, is not.
counter=${FM_COUNTER:-BranchInstructions}
interval=
verbose=
# Kill the workload after this many seconds. A driver scene that stalls leaves the editor
# on screen (pgo-driver.cpp:1328 only quits in profile mode), and the trace with it.
timeout_s=${FM_TIMEOUT:-}
# How many times 'run' launches the staged binary inside the one session. A workload that is
# over in a second yields too few samples at any interval worth using for a longer one.
runs=${FM_RUNS:-1}
# raw kernel log; -stop -d merges it into the .etl and then it is deleted
rawlog=

# Branch retirements per sample. At 65536 the trace grows ~36 MB/s, fine for a benchmark
# that runs for seconds and 10 GB for a five-minute editor session, so the interactive
# default is 16x sparser.
interval_batch=65536
interval_interactive=1048576

usage_text() {
    cat <<EOF
usage: ${self} [options] <start|stop|run|convert>... [-- exe args...]
       ${self} --help

Verbs run in the order given.

  start    stage an unstripped binary, arm the ETW session
  stop     stop the session and write the .etl
  run      start, run the staged binary to completion, stop
  convert  .etl -> perf script -> llvm-profgen -> the .prof

The editor is the workload worth profiling and this script cannot launch it for you,
so that case uses the split verbs:

    contrib/${self} start      # stages an unstripped editor, arms the session
    <run the staged binary it names, play, quit>
    contrib/${self} stop convert

A batch workload needs one call:

    contrib/${self} -e benchmark run -- --benchmark_min_time=0.4s

'convert' prints the cmake line that builds against the profile it wrote.

options:
  -e, --exe NAME      floormat-NAME to profile; the prefix is optional  (${exe})
  -t, --tree DIR      build tree the binary and the compiler come from  (${tree})
  -i, --interval N    branch retirements per sample
                      (${interval_batch} under 'run', ${interval_interactive} under 'start')
  -T, --timeout SECS  kill the workload after this long  (${timeout_s:-no limit})
  -n, --runs N        launches of the staged binary inside the one session  (${runs})
  -o, --etl PATH      trace to write, and to read back in 'convert'  (${etl})
  -p, --profile PATH  sample profile to write  (${prof})
  -v, --verbose       set -x
  -h, --help          this

env: FM_EXE FM_TREE FM_TIMEOUT FM_RUNS FM_ETL FM_PROFILE back those options.
FM_COUNTER is the PMU counter (${counter}) and FM_WPT the toolkit directory:
${wpt}

xperf needs an elevated shell, and perfcore.ini needs a perf_lbr.dll line or the trace
decodes without any LBR rows. llvm-profgen is taken from beside the compiler named in
the cache, so on Windows run this under the toolchain wrapper:
clang64 contrib/${self} run
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

xp() {
    "${wpt}/xperf.exe" "$@"
}

native() {
    case "$OS" in
        Windows_NT) cygpath -w -- "$1" ;;
        *) echo "$1" ;;
    esac
}

# perf_lbr.dll is the decoder, not the collector: without it in perfcore.ini the trace still
# holds LBR records and 'xperf -a dumper' prints none of them, which reads as an empty trace.
check_lbr_plugin() {
    local ini
    ini="${wpt}/perfcore.ini"
    if ! test -f "$ini"; then
        echo "warning: no ${ini}, cannot tell whether LBR decoding is registered" >&2
        return 0
    fi
    if tr -d '\r' < "$ini" | grep -qxF perf_lbr.dll; then
        return 0
    fi
    echo "error: perf_lbr.dll is not registered in ${ini}" >&2
    echo "       add a line saying 'perf_lbr.dll' to it (needs elevation), else the trace" >&2
    echo "       decodes without any LBR rows and the profile comes out empty" >&2
    return 1
}

cache_get() {
    sed -n "s|^$2:[^=]*=||p" "$1/CMakeCache.txt"
}

# Beside the compiler, not whatever is on PATH: llvm-profgen reads the binary's debug info
# with the same LLVM the binary was built by.
find_profgen() {
    local _cxx _bin _i
    _cxx="$(cache_get "$tree" CMAKE_CXX_COMPILER)"
    _bin="$(dirname -- "$_cxx")/llvm-profgen"
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
    echo "error: no llvm-profgen beside '${_cxx}'" >&2
    return 1
}

# llvm-profgen needs symbols and disassemblable code in one view, which rules out install/bin
# (stripped) and its .debug companion (--only-keep-debug keeps neither). RELEASE/bin has both
# but no sibling share/floormat, and floormat resolves assets from the exe path, so it aborts
# on walls/walls.json. Copying the unstripped exe into install/bin satisfies both.
staged_path() {
    echo "${tree}/install/bin/floormat-${exe}-fdo.exe"
}

stage_binary() {
    local src dst
    src="${tree}/RELEASE/bin/floormat-${exe}.exe"
    dst="$(staged_path)"
    if ! test -f "$src"; then
        echo "error: no ${src}; build ${tree} first" >&2
        return 65
    fi
    if ! test -d "${tree}/install/share/floormat"; then
        echo "error: no ${tree}/install/share/floormat; run the install target first" >&2
        return 65
    fi
    if ! test -f "$dst" || test "$src" -nt "$dst"; then
        cp -f -- "$src" "$dst"
        echo "staged $dst"
    fi
    echo "$dst" > "${tree}/.fdo-staged"
}

do_start() {
    local iv
    check_lbr_plugin
    stage_binary
    iv="${interval:-$1}"
    if xp -loggers 2>/dev/null | grep -q "NT Kernel Logger"; then
        echo "error: a kernel logger is already running; './${self} stop' first" >&2
        return 65
    fi
    echo "==> xperf -on, ${counter} every ${iv}"
    mkdir -p -- "$(dirname -- "$etl")"
    xp -on LOADER+PROC_THREAD+PMC_PROFILE \
       -MinBuffers 1024 -MaxBuffers 1024 -BufferSize 1024 \
       -pmcprofile "$counter" -LastBranch PmcInterrupt \
       -setProfInt "$counter" "$iv" \
       -f "$(native "$rawlog")"
}

# Two things -stop gets wrong unless told otherwise. Without -d it still writes the trace,
# to <drive>:\kernel.etl. With -d it merges into the named file but leaves the raw kernel
# log where -on put it, so -on names that too and it gets dropped here.
do_stop() {
    mkdir -p -- "$(dirname -- "$etl")"
    echo "==> xperf -stop -d ${etl}"
    xp -stop -d "$(native "$etl")"
    rm -f -- "$rawlog"
    ls -l -- "$etl"
}

# The repeats stay inside the one session: arming and stopping a kernel logger costs far more
# than the workload, and N runs in one trace is N times the samples for one .etl to decode.
do_run() {
    local bin status i
    do_start "$interval_batch"
    bin="$(cat "${tree}/.fdo-staged")"
    status=0
    # a workload that dies must still stop the session, or the next start fails and the
    # logger keeps filling the disk
    trap 'do_stop' EXIT INT TERM
    i=1
    while test "$i" -le "$runs"; do
        if test "$runs" -gt 1; then
            echo "==> ${bin} $*  (${i}/${runs})"
        else
            echo "==> ${bin} $*"
        fi
        if test -n "$timeout_s"; then
            timeout -k 10 "$timeout_s" "$bin" "$@" || status=$?
        else
            "$bin" "$@" || status=$?
        fi
        test "$status" -eq 0 || break
        i=$((i + 1))
    done
    trap - EXIT INT TERM
    do_stop
    case "$status" in
        0) : ;;
        124) echo "warning: workload killed after ${timeout_s}s; the trace covers whatever ran" >&2 ;;
        *) echo "warning: workload exited ${status}; the trace covers whatever ran" >&2 ;;
    esac
}

do_convert() {
    local bin profgen script
    bin="$(staged_path)"
    if ! test -f "$bin"; then
        echo "error: no ${bin}; the trace has to be of a staged binary" >&2
        return 65
    fi
    if ! test -f "$etl"; then
        echo "error: no ${etl}" >&2
        return 65
    fi
    profgen="$(find_profgen)"
    script="${etl%.etl}.perfscript"
    echo "==> etw-to-perfscript"
    python3 contrib/etw-to-perfscript.py --etl "$(native "$etl")" --binary "$bin" -o "$script"
    echo "==> llvm-profgen"
    mkdir -p -- "$(dirname -- "$prof")"
    "$profgen" --perfscript="$script" --binary="$bin" --output="$prof"
    ls -l -- "$prof"
    echo "==> configure with it:"
    echo "    cmake -S . -B ${tree} -DFLOORMAT_PGO=sample -DFLOORMAT_PGO_SAMPLE=$(native "$PWD/$prof" | tr '\\' /)"
}

verbs=
while test $# -gt 0; do
    case "$1" in
        -e|--exe) exe="$2"; shift 2 ;;
        -t|--tree) tree="$2"; shift 2 ;;
        -i|--interval) interval="$2"; shift 2 ;;
        -T|--timeout) timeout_s="$2"; shift 2 ;;
        -n|--runs) runs="$2"; shift 2 ;;
        -o|--etl) etl="$2"; shift 2 ;;
        -p|--profile) prof="$2"; shift 2 ;;
        -v|--verbose) verbose=1; shift ;;
        -h|--help) usage 0 ;;
        --) shift; break ;;
        start|stop|run|convert) verbs="$verbs $1"; shift ;;
        *) echo "error: unknown argument '$1'" >&2; usage ;;
    esac
done

test -n "$verbs" || usage
test -z "$verbose" || set -x

case "${exe}" in floormat-*) exe="${exe#floormat-}" ;; esac
rawlog="${etl%.etl}.raw.etl"

for verb in $verbs; do
    case "$verb" in
        start) do_start "$interval_interactive"
               echo "now run: $(cat "${tree}/.fdo-staged")"
               echo "then:    ./${self} stop convert" ;;
        stop) do_stop ;;
        run) do_run "$@" ;;
        convert) do_convert ;;
    esac
done
