#!/usr/bin/env python3
"""Deflate a file for stbi_zlib_decode_buffer, inflated size in front.

    zlib-compress.py in out

The 4-byte big-endian header carries the inflated size, which
stbi_zlib_decode_buffer cannot report. 7-Zip's deflate encoder emits the same
stream about 13% smaller than zlib -9, which is the fallback when it is missing.
"""
import os, shutil, struct, subprocess, sys, tempfile, zlib

def deflate_7z(d):
    exe = os.environ.get("FM_7Z") or shutil.which("7z") or shutil.which("7za")
    pf = os.environ.get("ProgramFiles") or r"C:\Program Files"
    if not exe:
        exe = shutil.which("7z.exe", path=os.path.join(pf, "7-Zip"))
    if not exe:
        return None
    with tempfile.TemporaryDirectory() as tmp:
        src, gz = os.path.join(tmp, "in"), os.path.join(tmp, "out.gz")
        open(src, "wb").write(d)
        r = subprocess.run([exe, "a", "-tgzip", "-mx=9", "-mfb=258", "-mpass=15", gz, src],
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        if r.returncode or not os.path.exists(gz):
            return None
        g = open(gz, "rb").read()

    flg, i = g[3], 10
    if flg & 4:
        i += 2 + struct.unpack_from("<H", g, i)[0]
    for bit in (8, 16):
        if flg & bit:
            i = g.index(b"\0", i) + 1
    if flg & 2:  # FHCRC trails the optional name and comment
        i += 2
    return g[i:-8]

def main(src, dst):
    d = open(src, "rb").read()
    raw = deflate_7z(d)
    if raw is None:
        z, how = zlib.compress(d, 9), "zlib -9"
    else:
        z, how = b"\x78\xda" + raw + struct.pack(">I", zlib.adler32(d, 1)), "7z"
    if zlib.decompress(z) != d:
        raise SystemExit("round trip failed")
    open(dst, "wb").write(struct.pack(">I", len(d)) + z)
    print(f"{len(d)} -> {len(z) + 4} bytes ({100 * (len(z) + 4) / len(d):.1f}%, {how})")

if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
