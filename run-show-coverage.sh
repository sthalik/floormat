#!/bin/sh

set -e

case "$OS" in
    Windows_NT) export PATH="$PATH:/usr/bin" ;;
esac

self="$(basename -- "$1")"

usage() {
    echo "usage: ${self} <run|run-only|show|generate>" >&2
    exit 64
}

if test -z "$1"; then
    usage
fi

run=0
generate=0
show=0

case "$1" in
    run) run=1; generate=1; show=1 ;;
    run-only) run=1 ;;
    show) generate=1; show=1 ;;
    generate) generate=1 ;;
    *) echo "error: invalid command-line argument '$1'" >&2; usage ;;
esac

#find build directory
if test -f CMakeLists.txt && test -f resources.conf; then
    cd build
elif test -d bin; then
    cd ..
elif test -x floormat || test -x floormat.exe; then
    cd ../..
elif test -d install/bin && test -d install/share/floormat; then
    :
else
    echo "error: can't find build directory!" >&2
    exit 65
fi

exe=
for i in ./install/bin/floormat.exe ./install/bin/floormat; do
    if test -x "$i"; then
        exe="$i"
        break
    fi
done

if ! test -n "$exe"; then
    echo "error: no 'floormat' executable" >&2
    exit 65
fi

prof=coverage

if test $run -gt 0; then
    rm -f -- ./"${prof}".profraw ./"${prof}".profdata ../"${prof}".lcov
    (
        export FLOORMAT_WITH_COVERAGE=1
        cmake .
        cmake --build . --target install
    )
    LLVM_PROFILE_FILE="../${prof}.profraw" FLOORMAT_WITH_COVERAGE=1 \
        "$exe" --magnum-gpu-validation on
fi

if test $generate -gt 0; then
    llvm-profdata merge -sparse "./${prof}".profraw -o "./${prof}".profdata
    llvm-cov export --format=lcov --instr-profile="./${prof}".profdata -- "$exe" > "./${prof}".lcov
    if test -e "./${prof}"; then
        rm -rf -- "./${prof}/"
    fi
    genhtml -o "./${prof}" -s "./${prof}.lcov"
fi

if test $show -gt 0; then
    if command -v xdg-open >/dev/null; then
        xdg-open "./${prof}/index.html"
    else
        start "./${prof}/index.html"

    fi
fi
exit 0
