#!/bin/bash
set -e
cd "$(dirname "$0")/images"

SIZE=9x7
SRC=rgb.png

magick -size $SIZE gradient:'#ff0000-#0000ff' -alpha off -depth 8 "PNG24:$SRC"

magick "$SRC" -quality 92                                       rgb.jpg
magick "$SRC" -depth 8                                     "BMP3:rgb.bmp"
magick "$SRC" -flatten -alpha off -colorspace sRGB -depth 8 "PSD:rgb.psd"
magick "$SRC" -compress RLE                                 "TGA:rgb.tga"
magick "$SRC" -colors 64                                    "GIF:rgb.gif"
magick "$SRC" -colors 64 \( "$SRC" -rotate 90 -resize $SIZE! -colors 64 \) \
              \( "$SRC" -negate -colors 64 \) -delay 10 -loop 0 "GIF:anim.gif"
magick "$SRC" -colorspace RGB -define quantum:format=floating-point -depth 32 "HDR:rgb.hdr"
magick "$SRC" -colorspace Gray -depth 8                     "PGM:gray.pgm"
magick "$SRC" -depth 8                                      "PPM:rgb.ppm"

magick "$SRC" -depth 8 RGB:- | python3 ../make-pic.py ${SIZE%x*} ${SIZE#*x} > rgb.pic

echo "--- file(1) ---"
file rgb.png rgb.jpg rgb.bmp rgb.psd rgb.tga rgb.gif rgb.hdr gray.pgm rgb.ppm

echo "--- rgb.pic magic (expect 5380 f634) ---"
xxd -l 4 rgb.pic

echo "--- PNM headers (expect P6 / P5, maxval 255) ---"
head -c 16 rgb.ppm | tr '\n' ' '; echo
head -c 16 gray.pgm | tr '\n' ' '; echo
