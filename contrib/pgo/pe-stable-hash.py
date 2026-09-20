#!/usr/bin/env python3
"""Hash a PE image with its build stamps masked out, so two builds of the same inputs match.

Three fields carry the link time and defeat a plain hash: the COFF TimeDateStamp, the
TimeDateStamp inside every debug-directory entry, and the .gnu_debuglink CRC32 -- which
covers the separate .debug file, itself stamped the same way. Everything else in a
floormat release link is deterministic, verified by a clean rebuild from identical inputs.
"""
import hashlib, struct, sys


def stable(path):
    b = bytearray(open(path, 'rb').read())
    o = struct.unpack_from('<I', b, 0x3c)[0]
    if bytes(b[o:o+4]) != b'PE\0\0':
        sys.exit('%s: not a PE image' % path)
    nsec, optsz = struct.unpack_from('<H', b, o+6)[0], struct.unpack_from('<H', b, o+20)[0]
    symptr, nsym = struct.unpack_from('<II', b, o+12)
    struct.pack_into('<I', b, o+8, 0)                      # COFF TimeDateStamp
    magic = struct.unpack_from('<H', b, o+24)[0]
    opt = o+24
    struct.pack_into('<I', b, opt+64, 0)                   # optional header CheckSum

    secs = []
    for i in range(nsec):
        s = o+24+optsz+40*i
        vsz, va, rsz, ptr = struct.unpack_from('<IIII', b, s+8)
        secs.append((bytes(b[s:s+8]).rstrip(b'\0').decode(), va, vsz, ptr, rsz))

    def to_file(rva):
        for _, va, vsz, ptr, rsz in secs:
            if va <= rva < va + max(vsz, rsz):
                return ptr + (rva - va)
        return None

    dd = opt + (112 if magic == 0x20b else 96)
    rva, sz = struct.unpack_from('<II', b, dd + 8*6)       # IMAGE_DIRECTORY_ENTRY_DEBUG
    off = to_file(rva) if rva else None
    if off is not None:
        for i in range(sz // 28):
            struct.pack_into('<I', b, off + 28*i + 4, 0)   # entry TimeDateStamp

    strtab = symptr + 18*nsym
    for name, va, vsz, ptr, rsz in secs:
        if name.startswith('/') and strtab < len(b):
            i = strtab + int(name[1:])
            name = bytes(b[i:b.index(b'\0', i)]).decode()
        # debuglink is <name>\0 padded to 4, then a CRC32 of the .debug companion
        if name == '.gnu_debuglink':
            struct.pack_into('<I', b, ptr + vsz - 4, 0)
    return hashlib.sha256(bytes(b)).hexdigest()[:16]


if __name__ == '__main__':
    for p in sys.argv[1:]:
        print(stable(p), p)
