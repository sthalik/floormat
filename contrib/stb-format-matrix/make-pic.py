#!/usr/bin/env python3
"""Wrap raw RGB bytes on stdin into a Softimage PIC file on stdout.

ImageMagick has no PIC coder, and file(1) 5.48 has no Softimage magic entry, so this
is the only way to produce the sample and the only check on it is that stb decodes it.
Layout taken from stbi__pic_test_core and stbi__pic_load, stb_image.h:6113-6302.
"""
import struct
import sys


def encode(width, height, rgb):
    if len(rgb) != width * height * 3:
        raise SystemExit(f'expected {width * height * 3} bytes of RGB, got {len(rgb)}')
    out = bytearray()
    out += b'\x53\x80\xf6\x34'
    out += struct.pack('>f', 3.14)          # version, unread
    out += b'floormat stb matrix sample'.ljust(80, b'\0')
    out += b'PICT'
    out += struct.pack('>HH', width, height)
    out += struct.pack('>fHH', 1.0, 0, 0)   # ratio, fields, pad -- all skipped
    out += bytes((0, 8, 0, 0xE0))
    out += rgb
    return bytes(out)


if __name__ == '__main__':
    if len(sys.argv) != 3:
        raise SystemExit('usage: make-pic.py WIDTH HEIGHT < rgb.raw > out.pic')
    w, h = int(sys.argv[1]), int(sys.argv[2])
    sys.stdout.buffer.write(encode(w, h, sys.stdin.buffer.read()))
