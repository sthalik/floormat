#!/bin/sh
#
# Rebuilds opengl32.dll from opengl32.c, minimum-values.h, opengl32.def and gl.xml, and with-dll-dir.exe, with
# the C compiler of a Clang build dir.

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
sed -En 's/^static [a-z]+ ((pre|post)_gl[A-Za-z0-9_]+)\(.*/\1/p' "$src/minimum-values.h" > "$out/hooks.txt"
awk -v exported_wrappers="$out/exported-wrappers.txt" -f "$src/wrappers.awk" \
    "$out/core.txt" "$out/exported.txt" "$out/hooks.txt" "$gl_xml" > "$out/wrappers.h"

grep -oE '\<GL_[A-Za-z0-9_]+\>' "$src/minimum-values.h" | sort -u > "$out/enum-names.txt"
awk 'FILENAME == ARGV[1] { wanted[$1] = 1; next }
     /<enum value="/ && !/api="gles/ { match($0, /name="[^"]*"/); name = substr($0, RSTART + 6, RLENGTH - 7)
                                       match($0, /value="[^"]*"/); value = substr($0, RSTART + 7, RLENGTH - 8)
                                       if (name in wanted && !(name in defined)) {
                                           printf "#define %s %s\n", name, value; defined[name] = 1 } }
     END { for (name in wanted) if (!(name in defined)) missing = missing " " name
           if (missing != "") { print "fatal: gl.xml has no enum" missing > "/dev/stderr"; exit 1 } }' \
    "$out/enum-names.txt" "$gl_xml" > "$out/enums.h"

# opengl32.dll exports GL 1.1, so a wrapper of a GL 1.1 function replaces the export forwarded to Mesa.
sed -E "$(sed 's/.*/s|^& = opengl32-mesa\\.&$|& = wrap_&|/' "$out/exported-wrappers.txt")" \
    "$src/opengl32.def" > "$out/opengl32.def"
if test "$(grep -c ' = wrap_' "$out/opengl32.def")" != "$(wc -l < "$out/exported-wrappers.txt")"; then
    echo "fatal: $src/opengl32.def doesn't forward every GL 1.1 function that has a wrapper" >&2
    exit 1
fi

"$cc" -O2 -Wall -Wextra -shared -s -Wl,--no-insert-timestamp -I"$out" -o "$src/opengl32.dll" "$src/opengl32.c" \
    "$out/opengl32.def"
"$cc" -O2 -Wall -Wextra -s -Wl,--no-insert-timestamp -o "$src/with-dll-dir.exe" "$src/with-dll-dir.c"
