#!/bin/bash
set -u
FM=D:/dev/floormat
CXX=D:/dev/llvm-23.1.0-rc3/bin/clang++.exe
NM=D:/dev/llvm-23.1.0-rc3/bin/llvm-nm.exe
BUILD=${BUILD:-clang-asan}
PLUGIN=${1:-importer}
OBJDIR=${OBJDIR:-D:/Temp/stb-matrix-sweep}
mkdir -p "$OBJDIR"
trap 'rm -rf "$OBJDIR"' EXIT

if [ "$PLUGIN" = importer ]; then
    FORMATS=(JPEG PNG BMP GIF PSD PIC PNM HDR TGA)
    PREFIX=MAGNUM_STBIMAGEIMPORTER_NO_
    SRC=$FM/external/magnum-plugins/src/MagnumPlugins/StbImageImporter/StbImageImporter.cpp
    EXPORTS=-DStbImageImporter_EXPORTS
else
    FORMATS=(BMP JPEG HDR PNG TGA)
    PREFIX=MAGNUM_STBIMAGECONVERTER_NO_
    SRC=$FM/external/magnum-plugins/src/MagnumPlugins/StbImageConverter/StbImageConverter.cpp
    EXPORTS=-DStbImageConverter_EXPORTS
fi

N=${#FORMATS[@]}
LAST=$(( (1 << N) - 1 ))

INCS="-I$FM/external/magnum-plugins/src -I$FM/build/$BUILD/external/magnum-plugins/src
 -isystem $FM/external/corrade/src -isystem $FM/build/$BUILD/external/corrade/src
 -isystem $FM/external/magnum-plugins/src/external/stb
 -isystem $FM/external/magnum/src -isystem $FM/build/$BUILD/external/magnum/src"

compile_one() {
    local mask=$1 defs="" i
    for ((i = 0; i < N; i++)); do
        (( (mask >> i) & 1 )) && defs="$defs -D$PREFIX${FORMATS[i]}"
    done
    local obj=$OBJDIR/$mask.obj log
    log=$("$CXX" -c -o "$obj" -std=c++23 -fno-rtti -O0 -w \
          -DCORRADE_DYNAMIC_PLUGIN $EXPORTS -DNOMINMAX -D_HAS_EXCEPTIONS=0 \
          -D_WIN32_WINNT=0x0A00 -DUNICODE -D_UNICODE \
          $INCS $defs "$SRC" 2>&1)
    if [ -n "$log" ]; then
        printf 'FAIL(compile) mask=%d%s\n%s\n' "$mask" "$defs" "$log"
        return
    fi
    local undef
    undef=$("$NM" --undefined-only "$obj" 2>/dev/null | grep ' _ZL' || true)
    [ -n "$undef" ] && printf 'FAIL(link) mask=%d%s\n%s\n' "$mask" "$defs" "$undef"
    rm -f "$obj"
}
export -f compile_one
export CXX NM INCS SRC N PREFIX EXPORTS OBJDIR
export FORMATS_STR="${FORMATS[*]}"

fails=0
for ((m = 0; m <= LAST; m++)); do echo "$m"; done |
    xargs -P "${JOBS:-24}" -I{} bash -c '
        IFS=" " read -r -a FORMATS <<< "$FORMATS_STR"; compile_one {}' > /tmp/sweep-$PLUGIN.log 2>&1

if [ -s /tmp/sweep-$PLUGIN.log ]; then
    fails=$(grep -c '^FAIL' /tmp/sweep-$PLUGIN.log)
    echo "$PLUGIN: $fails of $((LAST + 1)) configs FAILED"
    grep '^FAIL' /tmp/sweep-$PLUGIN.log | head -20
    echo "(full log: /tmp/sweep-$PLUGIN.log)"
    exit 1
fi
echo "$PLUGIN: all $((LAST + 1)) configs compile clean"
