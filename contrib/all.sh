#!/bin/sh

set -eu -o pipefail
beep() { printf \\a >&2; }
test -z "${MAGNUM_LOG-}" && export MAGNUM_LOG=default

level=0
ts=0
opts="$(getopt -n "$0" -o ta -l test::,all -- "$@")" || exit 64
eval set -- "$opts"
while :; do
    case "$1" in
        -t) ts=$((ts + 1)); level=$ts; shift;;
        -a|--all) ts=0; level=2; shift;;
        --test) ts=0; level=${2:-1}; shift 2;;
        --) shift; break;;
    esac
done
case "$level" in
    0) types=;;
    1) types='1';;
    2) types='1 2 3';;
    *) echo "$0: test level must be 0, 1 or 2" >&2; exit 64;;
esac

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
    (
        local dir="$(basename -- "$PWD")"
        local wrapper="$1"; shift
        local type="$1"; shift
        local short=
        case "$dir" in *-asan|*-debug) short=1;; esac
        cd install/bin &&
        case "$type" in
            1) "$wrapper" ./floormat-test;;
            2) FLOORMAT_PGO_DRIVER_SHORTEN_SCENES=$short "$wrapper" ./floormat-editor --scenes=all --driver-no-swapbuffers --minimized --vsync=true;;
            3) "$wrapper" ./floormat-benchmark;;
        esac
    )
}

cd "$(dirname -- "$0" || exit $?)"
#cd f:/build/floormat
#set -x

exec 9>lock
if type flock >/dev/null 2>&1; then
    flock -n 9
elif type lockf >/dev/null 2>&1; then
    lockf -kTt 0 9
fi

configurations='
clang64 clang-asan
vc      msvc-debug
mingw64 gcc-debug
clang64 clang-release
vc      msvc-release
mingw64 gcc-release
clang64 clang-debug
'

printf "%s\\n" "$configurations" |
while read wrapper configs; do
    case "$wrapper" in \#*) continue ;; esac
    for i in $configs; do
        (
            cd $i
            bprintf -- "***** Entering directory %s\\n" "$i"
            "$wrapper" cmake ../.. >/dev/null
            if test $# -eq 0; then
                printf -- "> Running ninja for %s\\n" "$i"
            else
                printf -- "> Running ninja %s\\n" "$*"
            fi
            #printf -- "> Running cmake . for %s\\n" "$i"
            "$wrapper" ninja "$@"
            printf -- "> Running ninja install for %s\\n" "$i"
            "$wrapper" ninja install
            printf -- "\\n***** Exiting directory %s\\n\\n" "$i"
        )
    done
done

printf "%s\\n" "$configurations" |
while read wrapper configs; do
    case "$wrapper" in \#*) continue ;; esac
    for k in $types; do
        for i in $configs; do
            (
                cd $i
                bprintf -- "***** Entering directory %s\\n" "$i"
                run_test "$wrapper" $k
                printf -- "\\n***** Exiting directory %s\\n\\n" "$i"
            )
        done
    done
done

exit 0
