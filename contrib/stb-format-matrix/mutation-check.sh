#!/bin/bash
# Each mutation plants one defect the driver is supposed to catch. A mutation the
# driver still passes means that check proves nothing.
set -e

FM="$(cd "$(dirname "$0")/../.." && pwd)"
B="$FM/external/magnum-plugins/build"
IMG="$FM/contrib/stb-format-matrix/images"
DRIVER="$B/bin/driver.exe"
BK="$B/mutation-backup"

IMP="$B/variants/StbImageImporter"
CONV="$B/variants/StbImageConverter"

rm -rf "$BK"; mkdir -p "$BK"
n=0
save() {
    n=$((n + 1))
    cp "$1" "$BK/$n"
    echo "$n $1" >> "$BK/list"
}
restore() {
    if [ -e "$BK/list" ]; then
        while read -r i f; do
            cp "$BK/$i" "$f"
        done < "$BK/list"
        rm -f "$BK/list"
    fi
}
trap restore EXIT

caught=0
missed=0
probe() {
    local name=$1
    if "$DRIVER" "$B/variants.tsv" "$IMG" "$B/tmp" >"$BK/out" 2>&1; then
        echo "NOT CAUGHT  $name"
        missed=$((missed + 1))
    else
        caught=$((caught + 1))
        echo "caught      $name"
        grep -m2 -E '^(FAIL|phase 0 failed)' "$BK/out" | sed 's/^/              /' || true
    fi
    restore
}

echo "=== baseline, unmutated ==="
if "$DRIVER" "$B/variants.tsv" "$IMG" "$B/tmp" >"$BK/out" 2>&1; then
    echo "passes, as it must for the mutations below to mean anything"
else
    echo "BASELINE ALREADY FAILS -- nothing below is interpretable"
    tail -5 "$BK/out"
    exit 1
fi
echo

save "$IMP/511/StbImageImporter.dll"
cp "$IMP/000/StbImageImporter.dll" "$IMP/511/StbImageImporter.dll"
probe "importer mask 511 built as if all formats were on"

save "$IMP/001/StbImageImporter.dll"
cp "$IMP/000/StbImageImporter.dll" "$IMP/001/StbImageImporter.dll"
probe "importer mask 001 still has the JPEG decoder"

save "$IMP/000/StbImageImporter.dll"
cp "$IMP/511/StbImageImporter.dll" "$IMP/000/StbImageImporter.dll"
probe "reference variant decodes nothing"

save "$IMG/rgb.png"
head -c 20 "$BK/$n" > "$IMG/rgb.png"
probe "truncated sample image"

save "$IMP/511/StbImageImporter.conf"
sed -i 's/^#provides=PngImporter/provides=PngImporter/' "$IMP/511/StbImageImporter.conf"
probe "conf advertises an alias the build dropped"

save "$CONV/08/StbImageConverter.dll"
cp "$CONV/00/StbImageConverter.dll" "$CONV/08/StbImageConverter.dll"
probe "converter mask 08 still writes PNG"

save "$CONV/08/StbImageConverter.conf"
sed -i 's/^#provides=StbPngImageConverter/provides=StbPngImageConverter/' "$CONV/08/StbImageConverter.conf"
probe "converter conf advertises an alias the build dropped"

# Padding leaves the code untouched, so only the size check can notice this one
save "$IMP/001/StbImageImporter.dll"
head -c 200000 /dev/zero >> "$IMP/001/StbImageImporter.dll"
probe "stripping JPEG no longer shrinks the binary"

echo
echo "$caught caught, $missed not caught"
[ "$missed" -eq 0 ]
