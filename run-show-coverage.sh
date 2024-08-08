#!/bin/sh

set -e

case "$OS" in
    Windows_NT) export PATH="$PATH:/usr/bin" ;;
esac

self="$(basename -- "$0")"
cd -- "$(dirname -- "$0")"

usage() {
    echo "usage: ${self} <all|compile|run|generate|open>..." >&2
    exit 64
}

if test -z "$1"; then
    usage
fi

if ! command -v llvm-profdata >/dev/null 2>&1; then
    export PATH="/clang64/bin:/usr/bin:$PATH"
fi

compile=0
run=0
generate=0
open=0
exe=floormat-editor

find_exe() {
    case "$1" in
        floormat-*) exe="$1" ;;
        *) exe="floormat-$1" ;;
    esac
}

while test $# -gt 0; do
    case "$1" in
        all)
        {
            compile=1; run=1; generate=1; open=1 exe=floormat-test
            if test $# -gt 1; then shift; find_exe "$1"; fi
        } ;;
        compile) compile=1 ;;
        run)
        {
            compile=1; run=1;
            if test $# -gt 1; then shift; find_exe "$1"; fi
        } ;;
        generate) generate=1 ;;
        open) open=1 ;;
        *) echo "error: invalid command-line argument '$1'" >&2; usage ;;
    esac
    shift
done

#find build directory
build="build/coverage"
if test -f CMakeLists.txt && test -f resources.conf; then
    cd ./"${build}"
elif test -d bin; then
    cd ../../"${build}"
elif test -x "${exe}" || test -x "${exe}".exe; then
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


prof=coverage

if test $compile -gt 0; then
    rm -f -- ./"${prof}".profraw ./"${prof}".profdata ../"${prof}".lcov
    cmake -DFLOORMAT_WITH-COVERAGE:BOOL=1 .
    cmake --build . --target install
fi

if test $run -gt 0; then
    if ! test -n "$exe"; then
        echo "error: no 'floormat-editor' executable" >&2
        exit 65
    fi
    case "$OS" in
        Windows_NT) profdir="$(cygpath -m -- "$PWD")" ;;
        *) profdir="$PWD" ;;
    esac
    LLVM_PROFILE_FILE="$profdir/${prof}.profraw" \
        "$exe" --magnum-gpu-validation=full --vsync=1
fi

if test $generate -gt 0; then
    llvm-profdata merge -sparse "./${prof}".profraw -o "./${prof}".profdata
    llvm-cov export --format=lcov --instr-profile="./${prof}".profdata -- "$exe" > "./${prof}".lcov
    if test -e "./${prof}"; then
        rm -rf -- "./${prof}/"
    fi
    genhtml -o "./${prof}" -s "./${prof}.lcov"
fi

if test $open -gt 0; then
    if command -v xdg-open >/dev/null; then
        xdg-open "./${prof}/index.html"
    else
        start "./${prof}/index.html"
    fi
fi

exit 0
