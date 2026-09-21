#!/bin/bash
# The matrix writes configure.h and the .conf itself, so it never exercises the
# CMake option path. This does, and diffs the result against the matrix variant
# for the same mask.
set -e

FM="$(cd "$(dirname "$0")/../.." && pwd)"
MP="$FM/external/magnum-plugins"
ROOT="$MP/build-spot"
PREFIX="$ROOT/install"
LLVM=D:/dev/llvm-23.1.0-rc3/bin
MATRIX="$MP/build"

TOOLCHAIN=(
    -GNinja
    -DCMAKE_BUILD_TYPE=Release
    -DCMAKE_CXX_STANDARD=17
    -DCMAKE_CXX_FLAGS=-g0
    -DCMAKE_C_COMPILER=$LLVM/cc.exe
    -DCMAKE_CXX_COMPILER=$LLVM/c++.exe
    -DCMAKE_LINKER=$LLVM/ld.lld.exe
    -DCMAKE_RC_COMPILER=$LLVM/windres.exe
    -DCMAKE_INSTALL_PREFIX="$PREFIX"
    -DCMAKE_PREFIX_PATH="$PREFIX"
    -DBUILD_SHARED_LIBS=ON
)

IMP=(JPEG PNG BMP GIF PSD PIC PNM HDR TGA)
CONV=(BMP JPEG HDR PNG TGA)

if [ ! -e "$PREFIX/bin/libCorradeUtility.dll" ]; then
    echo "=== corrade ==="
    cmake -S "$FM/external/corrade" -B "$ROOT/corrade" "${TOOLCHAIN[@]}" \
        -DCORRADE_BUILD_TESTS=OFF -DCORRADE_BUILD_DEPRECATED=OFF \
        -DCORRADE_WITH_INTERCONNECT=OFF -DCORRADE_WITH_PLUGINMANAGER=ON \
        -DCORRADE_WITH_TESTSUITE=ON >/dev/null
    ninja -C "$ROOT/corrade" install >/dev/null
fi

if [ ! -e "$PREFIX/bin/libMagnumTrade.dll" ]; then
    echo "=== magnum ==="
    cmake -S "$FM/external/magnum" -B "$ROOT/magnum" "${TOOLCHAIN[@]}" \
        -DMAGNUM_BUILD_TESTS=OFF -DMAGNUM_BUILD_PLUGINS_STATIC=OFF \
        -DMAGNUM_WITH_GL=OFF -DMAGNUM_WITH_AUDIO=OFF -DMAGNUM_WITH_DEBUGTOOLS=ON \
        -DMAGNUM_WITH_MATERIALTOOLS=OFF -DMAGNUM_WITH_MESHTOOLS=OFF \
        -DMAGNUM_WITH_PRIMITIVES=OFF -DMAGNUM_WITH_SCENEGRAPH=OFF \
        -DMAGNUM_WITH_SCENETOOLS=OFF -DMAGNUM_WITH_SHADERS=OFF \
        -DMAGNUM_WITH_SHADERTOOLS=OFF -DMAGNUM_WITH_TEXT=OFF \
        -DMAGNUM_WITH_TEXTURETOOLS=OFF -DMAGNUM_WITH_ANYIMAGEIMPORTER=OFF \
        -DMAGNUM_WITH_ANYIMAGECONVERTER=OFF -DMAGNUM_WITH_TGAIMPORTER=OFF \
        -DMAGNUM_WITH_TGAIMAGECONVERTER=OFF -DMAGNUM_WITH_TRADE=ON >/dev/null
    ninja -C "$ROOT/magnum" install >/dev/null
fi

# mask bit i means format i is stripped, matching matrix.cmake
mask_of() {
    local -n names=$1; shift
    local m=0 i f g
    for g in "$@"; do
        for i in "${!names[@]}"; do
            if [ "${names[$i]}" = "$g" ]; then
                m=$((m | (1 << i)))
            fi
        done
    done
    echo $m
}

fails=0
check() {
    local plugin=$1 id=$2 build=$3
    local got_conf="$build/src/MagnumPlugins/$plugin/$plugin.conf"
    local got_h="$build/src/MagnumPlugins/$plugin/configure.h"
    local want_conf="$MATRIX/variants/$plugin/$id/$plugin.conf"
    local want_h="$MATRIX/gen/$plugin/$id/MagnumPlugins/$plugin/configure.h"
    local f
    for f in conf h; do
        local -n got=got_$f; local -n want=want_$f
        if ! diff -q "$want" "$got" >/dev/null; then
            echo "  FAIL $plugin $id $f differs from the matrix variant"
            diff "$want" "$got" | head -6
            fails=$((fails + 1))
        fi
    done
}

run() {
    local name=$1; shift
    local build="$ROOT/case-$name"
    echo "=== $name ==="
    cmake -S "$MP" -B "$build" "${TOOLCHAIN[@]}" \
        -DMAGNUM_WITH_STBIMAGEIMPORTER=ON -DMAGNUM_WITH_STBIMAGECONVERTER=ON \
        "$@" >/dev/null
    ninja -C "$build" StbImageImporter StbImageConverter >/dev/null
}

cases=(
    "all-on::"
    "importer-all-off:JPEG PNG BMP GIF PSD PIC PNM HDR TGA:"
)
for i in "${!IMP[@]}"; do
    conv=""
    if [ "$i" -lt "${#CONV[@]}" ]; then
        conv="${CONV[$i]}"
    fi
    cases+=("no-${IMP[$i]}:${IMP[$i]}:$conv")
done

for c in "${cases[@]}"; do
    name=${c%%:*}; rest=${c#*:}
    impoff=${rest%%:*}; convoff=${rest#*:}
    flags=()
    for f in $impoff; do flags+=("-DMAGNUM_STBIMAGEIMPORTER_NO_$f=ON"); done
    for f in $convoff; do flags+=("-DMAGNUM_STBIMAGECONVERTER_NO_$f=ON"); done
    run "$name" "${flags[@]}"
    printf -v impid '%03d' "$(mask_of IMP $impoff)"
    printf -v convid '%02d' "$(mask_of CONV $convoff)"
    check StbImageImporter "$impid" "$ROOT/case-$name"
    check StbImageConverter "$convid" "$ROOT/case-$name"
done

echo
if [ "$fails" -ne 0 ]; then
    echo "$fails mismatches"
    exit 1
fi
echo "all ${#cases[@]} option-path cases match the matrix"
