#!/bin/sh

set -eu
set_pipefail() { test $# -lt 5 && set -o pipefail || true; }
set_pipefail {1,2,3,4,5} # detect dash
beep() { printf \\a >&2; }
test -z "$MAGNUM_LOG" && export MAGNUM_LOG=default

trap 'cleanup $?' exit
cleanup() {
    local i ret=$1
    set +x
    for i in 1 2 3 4 5; do beep; sleep 0.2; done
    exit $ret
}

run_test() {
    local dir="$(basename -- "$PWD")"
    cd install/bin &&
    if test -e floormat-test-asan.exe; then
        ./floormat-test-asan "$@"
    else
        ./floormat-test "$@"
    fi
}

#cd "$(dirname -- "$0" || exit $?)"
cd f:/build/floormat
#set -x

configurations='
clang64 clang clang-asan clang-release
mingw64 gcc gcc-release
msvc64  msvc-debug msvc
'

printf "%s\\n" "$configurations" |
while read wrapper configs; do
    for i in $configs; do
        (
            cd $i
            printf -- "> Entering directory %s\\n" "$i"
            "$wrapper" cmake c:/repos/floormat >/dev/null
            "$wrapper" ninja "$@" install
            printf -- "* Running tests for %s\\n" "$i"
            run_test
            printf -- "< Exiting directory %s\\n" "$i"
        )
    done
done

exit 0
