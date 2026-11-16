#!/bin/sh
set -eu
build_dir=$1
shift
source_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
sysroot=/opt/aarch64_be-floormat-sysroot
vector_bits=${FLOORMAT_SVE_BITS:-512}
case $vector_bits in
    128|512) ;;
    *) echo 'FLOORMAT_SVE_BITS must be 128 or 512' >&2; exit 64 ;;
esac
vector_bytes=$((vector_bits / 8))
cd "$build_dir"
unset DISPLAY WAYLAND_DISPLAY
export MAGNUM_LOG=verbose
timeout -k 30 -s QUIT 3540 taskset -c 0,3 qemu-aarch64_be \
    -cpu "max,sve$vector_bits=on,sve-default-vector-length=$vector_bytes" -L "$sysroot" \
    -E LD_LIBRARY_PATH="$sysroot/usr/lib:$sysroot/usr/lib64:$sysroot/lib64" \
    -g 1234 "$PWD/install/bin/floormat-test" --no-gl "$@" &
qemu_pid=$!
trap 'kill "$qemu_pid" 2>/dev/null || true; wait "$qemu_pid" 2>/dev/null || true' EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
gdb -q -batch -ex 'set sysroot /' \
    -ex 'set auto-load python-scripts off' \
    -ex "set solib-search-path $sysroot/usr/lib:$sysroot/usr/lib64:$sysroot/lib:$sysroot/lib64:$PWD/install/lib" \
    -ex 'target remote :1234' \
    -x "$source_dir/.github/gdbscript.py" "$PWD/install/bin/floormat-test"
