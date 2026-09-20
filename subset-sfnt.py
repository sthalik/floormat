#!/usr/bin/env python3
"""Drop codepoint ranges from a font's cmap and delete the glyphs left unreachable.

    subset-sfnt.py in.ttf out.ttf [LO-HI | CP ...]

Ranges are hex and inclusive. With none, drops the Private Use Area: E000-F8FF.
Only the tables stb_truetype reads are carried over; the rest are dropped.
"""
import struct, sys

DEFAULT_DROP = ["E000-F8FF", "2800-28FF"]

def checksum(b):
    b = b + b"\0" * (-len(b) % 4)
    return sum(struct.unpack(f">{len(b)//4}I", b)) & 0xFFFFFFFF

def read_tables(d):
    num = struct.unpack_from(">H", d, 4)[0]
    t = {}
    for i in range(num):
        tag, _, off, ln = struct.unpack_from(">4sIII", d, 12 + 16 * i)
        t[tag.decode("latin1")] = d[off:off + ln]
    return t

def read_cmap(cmap):
    for i in range(struct.unpack_from(">H", cmap, 2)[0]):
        pid, eid, off = struct.unpack_from(">HHI", cmap, 4 + 8 * i)
        if (pid, eid) == (3, 1):
            break
    else:
        raise SystemExit("no (3, 1) cmap subtable")
    fmt, _, _, seg2 = struct.unpack_from(">HHHH", cmap, off)
    if fmt != 4:
        raise SystemExit(f"(3, 1) cmap subtable is format {fmt}, want 4")
    seg = seg2 // 2
    end = struct.unpack_from(f">{seg}H", cmap, off + 14)
    start = struct.unpack_from(f">{seg}H", cmap, off + 16 + seg2)
    delta = struct.unpack_from(f">{seg}h", cmap, off + 16 + 2 * seg2)
    ro_at = off + 16 + 3 * seg2
    ro = struct.unpack_from(f">{seg}H", cmap, ro_at)

    out = {}
    for i in range(seg):
        for c in range(start[i], min(end[i], 0xFFFE) + 1):
            if ro[i]:
                g = struct.unpack_from(">H", cmap, ro_at + 2 * i + ro[i] + 2 * (c - start[i]))[0]
                g = g and (g + delta[i]) & 0xFFFF
            else:
                g = (c + delta[i]) & 0xFFFF
            if g:
                out[c] = g
    return out

def write_cmap(m):
    """idRangeOffset throughout, so idDelta never has to wrap."""
    segs = []
    for c in sorted(m):
        if segs and c == segs[-1][1] + 1:
            segs[-1][1] = c
        else:
            segs.append([c, c])
    segs.append([0xFFFF, 0xFFFF])
    n = len(segs)

    ro, gids = [0] * n, []
    for i, (lo, hi) in enumerate(segs[:-1]):
        ro[i] = 2 * (n - i) + 2 * len(gids)
        gids += [m[c] for c in range(lo, hi + 1)]

    sel = n.bit_length() - 1
    search = 2 << sel
    body = (struct.pack(f">{n}H", *(s[1] for s in segs)) + b"\0\0"
            + struct.pack(f">{n}H", *(s[0] for s in segs))
            + struct.pack(f">{n}h", *([0] * (n - 1) + [1]))
            + struct.pack(f">{n}H", *ro) + struct.pack(f">{len(gids)}H", *gids))
    sub = struct.pack(">HHHHHHH", 4, 14 + len(body), 0,
                      2 * n, search, sel, 2 * n - search) + body
    return struct.pack(">HHHHI", 0, 1, 3, 1, 12) + sub

def write_sfnt(tables):
    n = len(tables)
    sel = max(n.bit_length() - 1, 0)
    search = 16 << sel
    off = 12 + 16 * n
    recs, body = [], []
    for tag, b in sorted(tables.items()):
        # head.checkSumAdjustment is over the finished file, so it cannot be known yet.
        cs = checksum(b[:8] + b"\0\0\0\0" + b[12:]) if tag == "head" else checksum(b)
        recs.append(struct.pack(">4sIII", tag.encode("latin1"), cs, off, len(b)))
        body.append(b + b"\0" * (-len(b) % 4))
        off += len(b) + (-len(b) % 4)

    out = bytearray(struct.pack(">IHHHH", 0x00010000, n, search, sel, n * 16 - search)
                    + b"".join(recs) + b"".join(body))
    for i, (tag, _) in enumerate(sorted(tables.items())):
        if tag == "head":
            at = struct.unpack_from(">I", out, 12 + 16 * i + 8)[0]
            struct.pack_into(">I", out, at + 8, 0)
            struct.pack_into(">I", out, at + 8, (0xB1B0AFBA - checksum(bytes(out))) & 0xFFFFFFFF)
            break
    return bytes(out)

def main(src, dst, drop):
    d = open(src, "rb").read()
    t = read_tables(d)
    gone = set()
    for r in drop:
        lo, _, hi = r.partition("-")
        gone.update(range(int(lo, 16), int(hi or lo, 16) + 1))

    cmap = read_cmap(t["cmap"])
    keep = {c: g for c, g in cmap.items() if c not in gone}

    long_loca = struct.unpack_from(">h", t["head"], 50)[0]
    fmt = f">{len(t['loca']) // 4}I" if long_loca else f">{len(t['loca']) // 2}H"
    loca = struct.unpack(fmt, t["loca"])
    if not long_loca:
        loca = [x * 2 for x in loca]
    n_hm = struct.unpack_from(">H", t["hhea"], 34)[0]

    old = [0] + sorted(set(keep.values()))
    new = {g: i for i, g in enumerate(old)}
    glyf, offs = bytearray(), [0]
    for g in old:
        b = t["glyf"][loca[g]:loca[g + 1]]
        if b and struct.unpack_from(">h", b, 0)[0] < 0:
            raise SystemExit(f"glyph {g} is composite; component ids would need renumbering")
        glyf += b + b"\0" * (-len(b) % 4)
        offs.append(len(glyf))

    short = offs[-1] < 0x20000 and not any(o % 2 for o in offs)
    head = bytearray(t["head"])
    struct.pack_into(">h", head, 50, 0 if short else 1)

    maxp = bytearray(t["maxp"])
    struct.pack_into(">H", maxp, 4, len(old))
    hhea = bytearray(t["hhea"])
    struct.pack_into(">H", hhea, 34, len(old))
    out = {
        "cmap": write_cmap({c: new[g] for c, g in keep.items()}),
        "glyf": bytes(glyf),
        "head": bytes(head),
        "hhea": bytes(hhea),
        "hmtx": b"".join(t["hmtx"][4 * min(g, n_hm - 1):][:4] for g in old),
        "loca": (struct.pack(f">{len(offs)}H", *(o // 2 for o in offs)) if short
                 else struct.pack(f">{len(offs)}I", *offs)),
        "maxp": bytes(maxp),
    }
    if "OS/2" in t:
        os2 = bytearray(t["OS/2"])
        struct.pack_into(">HH", os2, 64, min(keep), min(max(keep), 0xFFFF))
        out["OS/2"] = bytes(os2)

    b = write_sfnt(out)
    open(dst, "wb").write(b)
    print(f"{len(d)} -> {len(b)} bytes, {len(loca) - 1} -> {len(old)} glyphs, "
          f"{len(cmap)} -> {len(keep)} codepoints")

if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2], sys.argv[3:] or DEFAULT_DROP)
