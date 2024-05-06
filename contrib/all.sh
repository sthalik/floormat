#!/bin/sh

set -eu -o pipefail
beep() { printf \\a >&2; }
test -z "$MAGNUM_LOG" && export MAGNUM_LOG=default

trap 'cleanup $?' exit
cleanup() {
    local i ret=$1
    set +x
    for i in {1..5}; do beep; sleep 0.2; done
    exit $ret
}

bold="\033[1m"
unbold="\033[0m"

bprintf() {
    printf "$bold" &&
    printf "$@" &&
    printf "$unbold" ||
    return 0
}

run_test() {
    local dir="$(basename -- "$PWD")"
    cd install/bin &&
    if test -e floormat-test-asan; then
        ./floormat-test-asan "$@"
    else
        ./floormat-test "$@"
    fi
}

#cd "$(dirname -- "$0" || exit $?)"
cd f:/build/floormat
#set -x

configurations='
clang64 clang
mingw64 gcc
msvc64  msvc-debug
clang64 clang-asan

clang64 clang-release
msvc64  msvc
mingw64 gcc-release
'

printf "%s\\n" "$configurations" |
while read wrapper configs; do
    for i in $configs; do
        (
            cd $i
            bprintf -- "***** Entering directory %s\\n" "$i"
            "$wrapper" cmake c:/repos/floormat >/dev/null
            if test $# -eq 0; then
                printf -- "> Running ninja for %s\\n" "$i"
            else
                printf -- "> Running ninja %s\\n" "$*"
            fi
            #printf -- "> Running cmake . for %s\\n" "$i"
            "$wrapper" ninja "$@"
            printf -- "> Running ninja install for %s\\n" "$i"
            "$wrapper" ninja install
            printf -- "> Running tests for %s\\n" "$i"
            run_test
            printf -- "\\n***** Exiting directory %s\\n\\n" "$i"
        )
    done
done

exit 0
