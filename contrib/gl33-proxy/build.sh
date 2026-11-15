#!/bin/sh
#
# Rebuilds opengl32.dll from opengl32.c, opengl32.def and gl.xml's core 3.3 functions with the C compiler of a
# Clang build dir.

set -eu
cd -- "$(dirname -- "$0")/../.."

build_dir="${1:-build/clang-asan}"
build_dir="${build_dir%/}"
cc="$(sed -n 's/^CMAKE_C_COMPILER:[A-Z]*=//p' "$build_dir/CMakeCache.txt")"
case "$cc" in
    ""|*cl|*cl.exe) echo "fatal: $build_dir has no Clang C compiler" >&2; exit 1 ;;
esac

src=contrib/gl33-proxy
. "$src/gl-xml.sh"
download_gl_xml

out=build/gl33-proxy
rm -rf -- "$out"
mkdir -p -- "$out"
gl_functions_required_by $core_versions > "$out/core.txt"
gl_functions_required_by GL_VERSION_1_0 GL_VERSION_1_1 > "$out/exported.txt"
awk -v exported_wrappers="$out/exported-wrappers.txt" -f "$src/wrappers.awk" \
    "$out/core.txt" "$out/exported.txt" "$gl_xml" > "$out/wrappers.h"

# opengl32.dll exports GL 1.1, so a wrapper of a GL 1.1 function replaces the export forwarded to Mesa.
sed -E "$(sed 's/.*/s|^& = opengl32-mesa\\.&$|& = wrap_&|/' "$out/exported-wrappers.txt")" \
    "$src/opengl32.def" > "$out/opengl32.def"
if test "$(grep -c ' = wrap_' "$out/opengl32.def")" != "$(wc -l < "$out/exported-wrappers.txt")"; then
    echo "fatal: $src/opengl32.def doesn't forward every GL 1.1 function that has a wrapper" >&2
    exit 1
fi

"$cc" -O2 -Wall -Wextra -shared -s -Wl,--no-insert-timestamp -I"$out" -o "$src/opengl32.dll" "$src/opengl32.c" \
    "$out/opengl32.def"
