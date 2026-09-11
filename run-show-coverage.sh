#!/bin/sh

set -e

case "$OS" in
    Windows_NT) export PATH="$PATH:/usr/bin" ;;
esac

self="$(basename -- "$0")"
cd -- "$(dirname -- "$0")"

# genhtml emits hrefs verbatim; a leading "D:" parses as a URI scheme in the browser.
case "$OS" in
    Windows_NT) srcroot="$(cygpath -m -- "$PWD")" ;;
    *) srcroot="$PWD" ;;
esac

usage() {
    echo "usage: ${self} <all|compile|run|generate|open>... [exe...]" >&2
    echo "       exe defaults to 'test editor', plus anything else already run when" >&2
    echo "       generating; profiles are merged into one report" >&2
    exit 64
}

if test -z "$1"; then
    usage
fi

if ! command -v llvm-profdata >/dev/null 2>&1; then
    export PATH="/clang64/bin:/usr/bin:$PATH"
    which llvm-profdata >/dev/null
fi

compile=0
run=0
generate=0
open=0
exes=

is_command() {
    case "$1" in
        all|compile|run|generate|open) return 0 ;;
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
    _x="$(basename -- "$1")"
    _x="${_x%.exe}"
    echo "${_x#floormat-}"
}

# Each binary rejects the others' options, so these are per-exe
exe_args() {
    case "$(exe_tag "$1")" in
        editor) echo "--magnum-gpu-validation=full --vsync=off --driver=all --driver-repeat 2" ;;
    esac
}

# A profile recorded against an older binary merges into garbage that llvm-cov reports as
# "functions have mismatched data". Dropping those by mtime is what lets profiles accumulate
# across invocations, so the editor can be driven by hand between two runs of everything else.
drop_stale_profraws() {
    for raw in ./"${prof}"-*.profraw; do
        test -f "$raw" || continue
        _t="${raw##*/${prof}-}"
        _t="${_t%.profraw}"
        _found=
        for _exe in ./install/bin/floormat-"${_t}".exe ./install/bin/floormat-"${_t}"; do
            if test -e "$_exe"; then
                _found="$_exe"
                break
            fi
        done
        if test -z "$_found"; then
            echo "note: dropping orphaned ${raw##*/}, there is no floormat-${_t}" >&2
            rm -f -- "$raw"
        elif test "$_found" -nt "$raw"; then
            echo "note: dropping stale ${raw##*/}, floormat-${_t} is newer" >&2
            rm -f -- "$raw"
        fi
    done
}

# don't leave background binaries or their logs behind when we're interrupted
bg=
kill_bg() {
    for job in $bg; do
        kill "${job%%:*}" 2>/dev/null || :
        rm -f -- "./${prof}-${job#*:}.log" 2>/dev/null || :
    done
    bg=
}

while test $# -gt 0; do
    case "$1" in
        all) compile=1; run=1; generate=1; open=1 ;;
        compile) compile=1 ;;
        run) compile=1; run=1 ;;
        generate) generate=1 ;;
        open) open=1 ;;
        *) echo "error: invalid command-line argument '$1'" >&2; usage ;;
    esac
    shift
    while test $# -gt 0 && ! is_command "$1"; do
        add_exe "$1"
        shift
    done
done

exes="${exes# }"
first_exe="${exes%% *}"
if test -z "$first_exe"; then
    first_exe=floormat-editor
fi

#find build directory
build="build/coverage"
if test -f CMakeLists.txt && test -f shaders/resources.conf; then
    cd ./"${build}"
elif test -d bin; then
    cd ../../"${build}"
elif test -x "${first_exe}" || test -x "${first_exe}".exe; then
    cd ../../"${build}"
elif test -d install/bin && test -d install/share/floormat; then
    cd ../"${build}"
elif test "./CMakeCache.txt"; then
    cd ../"${build}"
else
    echo "error: can't find build directory!" >&2
    exit 65
fi

if ! test -f "./CMakeCache.txt"; then
    echo "error: no CMakeCache.txt in build directory!"
    exit 65
fi

prof=coverage

if test $compile -gt 0; then
    rm -f -- ./"${prof}"-*.log ./"${prof}".profraw ./"${prof}".profdata ./"${prof}".lcov
    cmake -DCMAKE_BUILD_TYPE:STRING=Debug -DFLOORMAT_COVERAGE:BOOL=1 .
    cmake --build . --target install
fi

drop_stale_profraws

# A file only appears in the report if some object passed to llvm-cov links it, so the
# default list is about objects rather than profiles: main/ lives only in the editor, and
# leaving it out hides those files instead of showing them at 0%.
if test -z "$exes"; then
    exes="floormat-test floormat-editor"
    if test $run -eq 0; then
        # also report the tools and the benchmark once they have been run
        for raw in ./"${prof}"-*.profraw; do
            if test -f "$raw"; then
                tag="floormat-${raw##*/${prof}-}"
                tag="${tag%.profraw}"
                case " $exes " in
                    *" $tag "*) ;;
                    *) exes="$exes $tag" ;;
                esac
            fi
        done
    fi
fi

# after the build, so that a bare 'compile' works on a clean tree
if test $run -gt 0 || test $generate -gt 0; then
    resolved=
    for exe in $exes; do
        case "${exe}" in
        [a-zA-Z]:/*) : ;;
        [a-zA-Z]:\\*) : ;;
        /*) : ;;
        \\*) : ;;
        *)  for i in ./install/bin/"${exe}".exe ./install/bin/"${exe}"; do
                if test -x "$i"; then
                    exe="$i"
                    break
                fi
            done ;;
        esac
        if ! test -x "${exe}"; then
            echo "error: no '${exe}' executable" >&2
            exit 65
        fi
        resolved="$resolved $exe"
    done
    exes="${resolved# }"
fi

if test $run -gt 0; then
    case "$OS" in
        Windows_NT) profdir="$(cygpath -m -- "$PWD")" ;;
        *) profdir="$PWD" ;;
    esac
    # the editor wants a foreground window; overlap everything else with it and
    # replay the output afterwards so the two don't interleave
    fg_exe=
    for exe in $exes; do
        if test "$(exe_tag "$exe")" = editor; then
            fg_exe="$exe"
        fi
    done
    if test -z "$fg_exe"; then
        for exe in $exes; do
            fg_exe="$exe"
        done
    fi
    trap 'kill_bg; exit 130' INT
    trap 'kill_bg; exit 129' HUP
    trap 'kill_bg; exit 143' TERM
    trap kill_bg EXIT
    for exe in $exes; do
        if test "$exe" = "$fg_exe"; then
            continue
        fi
        tag="$(exe_tag "$exe")"
        echo "==> ${exe} &"
        LLVM_PROFILE_FILE="$profdir/${prof}-${tag}.profraw" \
            "$exe" $(exe_args "$exe") > "./${prof}-${tag}.log" 2>&1 &
        bg="$bg $!:${tag}"
    done
    status=0
    tag="$(exe_tag "$fg_exe")"
    echo "==> ${fg_exe}"
    if ! LLVM_PROFILE_FILE="$profdir/${prof}-${tag}.profraw" \
             "$fg_exe" $(exe_args "$fg_exe"); then
        echo "error: ${fg_exe} failed" >&2
        status=1
    fi
    for job in $bg; do
        tag="${job#*:}"
        if ! wait "${job%%:*}"; then
            echo "error: floormat-${tag} failed" >&2
            status=1
        fi
        echo "==> floormat-${tag}:"
        cat -- "./${prof}-${tag}.log"
        rm -f -- "./${prof}-${tag}.log"
    done
    trap - INT HUP TERM EXIT
    if test $status -ne 0; then
        exit 1
    fi
fi

if test $generate -gt 0; then
    # merge only the requested binaries' profiles; a leftover profraw from some
    # other binary would come out as 'functions have mismatched data'
    raws=
    objects=
    for exe in $exes; do
        raw="./${prof}-$(exe_tag "$exe").profraw"
        if test -f "$raw"; then
            raws="$raws $raw"
        fi
        objects="$objects --object $exe"
    done
    if test -z "$raws"; then
        echo "error: no ${prof}-*.profraw in $PWD, run '${self} run' first" >&2
        exit 65
    fi
    llvm-profdata merge -sparse $raws -o "./${prof}".profdata
    # MSYS rewrites an argument starting with '/' into a Windows path before llvm-cov sees
    # it, which silently matches nothing; $srcroot starts with a drive letter and survives.
    llvm-cov export --format=lcov --instr-profile="./${prof}".profdata \
        $objects --ignore-filename-regex="${srcroot}/(external|build)/" > "./${prof}".lcov
    if test -e "./${prof}"; then
        rm -rf -- "./${prof}/"
    fi
    genhtml --ignore-errors inconsistent,unsupported,category -p "$srcroot" -o "./${prof}" -s "./${prof}.lcov"
fi

if test $open -gt 0; then
    if command -v xdg-open >/dev/null; then
        xdg-open "./${prof}/index.html"
    else
        start "./${prof}/index.html"
    fi
fi

exit 0
