#!/bin/sh
# Runs one test suite from build/install. Shared by the Linux, valgrind and FreeBSD jobs.
#   sh .github/ci-test.sh gl|bench|no-dsa|no-gl|pgo|editor [extra floormat-editor args]
# FM_WRAP is the command prefix for the binary. Unset: gdb with gdbscript.py. Empty: none.
set -e
suite=${1:?usage: ci-test.sh gl|bench|no-dsa|no-gl|pgo|editor [args]}
shift
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root/build/install"

export LD_LIBRARY_PATH="$PWD/lib"
export ASAN_OPTIONS="${ASAN_OPTIONS-detect_leaks=0:abort_on_error=1}"
export MAGNUM_LOG=verbose
# llvmpipe hands back 4.5 core, which hides every use of GL above the 3.3
# floor. Pin it so the runner is the conformance oracle it looks like.
export MESA_GL_VERSION_OVERRIDE=3.3FC MESA_GLSL_VERSION_OVERRIDE=330
export MESA_EXTENSION_OVERRIDE=-GL_ARB_explicit_uniform_location
: "${FM_WRAP=gdb -q -batch -x $root/.github/gdbscript.py --args}"

# FreeBSD has no xvfb-run. Its job starts Xvfb on :99 before the build.
if command -v xvfb-run >/dev/null 2>&1; then
    x=xvfb-run
else
    x=
    export DISPLAY="${DISPLAY:-:99}"
fi
nogl="env -u DISPLAY -u WAYLAND_DISPLAY"

# $x, $nogl and $FM_WRAP are word-split on purpose.
# shellcheck disable=SC2086
case $suite in
gl)     $x $FM_WRAP bin/floormat-test --gl-only ;;
bench)  MAGNUM_LOG=default $x $FM_WRAP bin/floormat-benchmark ;;
no-dsa) MAGNUM_DISABLE_EXTENSIONS=GL_ARB_direct_state_access $x $FM_WRAP bin/floormat-test --gl-only ;;
no-gl)  $nogl $FM_WRAP bin/floormat-test --no-gl ;;
pgo)    LP_NUM_THREADS=$(nproc 2>/dev/null || sysctl -n hw.ncpu) FLOORMAT_PGO_DRIVER_SHORTEN_SCENES=1 \
        MAGNUM_LOG=default $x $FM_WRAP bin/floormat-editor --scenes=all --driver-no-swapbuffers "$@" ;;
editor) MAGNUM_LOG=default $x $FM_WRAP bin/floormat-editor "$@" --driver-no-swapbuffers ;;
*)      echo "ci-test.sh: unknown suite '$suite'" >&2; exit 2 ;;
esac
