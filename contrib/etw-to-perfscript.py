#!/usr/bin/env python3
"""Convert an xperf LBR trace into the perf-script text llvm-profgen reads.

    xperf -on LOADER+PROC_THREAD+PMC_PROFILE -pmcprofile BranchInstructions \
          -LastBranch PmcInterrupt -setProfInt BranchInstructions 65536
    <run the workload>
    xperf -stop -d trace.etl

    contrib/etw-to-perfscript.py --etl trace.etl \
        --binary build/clang-release/RELEASE/bin/floormat-editor.exe -o out.perfscript
    llvm-profgen --perfscript=out.perfscript --binary=<same> --output=out.prof
    <rebuild with -fprofile-sample-use=out.prof>

The binary must be unstripped, so point at RELEASE/bin, never install/bin.
"""
import argparse
import collections
import os
import re
import subprocess
import sys

WPT = r"C:\Program Files (x86)\Windows Kits\10\Windows Performance Toolkit"
# Not code in any module, so llvm-profgen maps it to ExternalAddr like it does
# for out-of-module addresses it resolves itself.
EXTERNAL = 1


def log(msg):
    print(msg, file=sys.stderr)


def preferred_base(binary):
    """PE ImageBase, which is what llvm-profgen uses as the preferred base.

    setPreferredTextSegmentAddresses() for COFF pushes getImageBase() itself,
    not ImageBase + the .text RVA.
    """
    with open(binary, "rb") as fh:
        head = fh.read(0x40)
        if head[:2] != b"MZ":
            raise SystemExit("%s is not a PE image" % binary)
        fh.seek(int.from_bytes(head[0x3C:0x40], "little"))
        if fh.read(4) != b"PE\0\0":
            raise SystemExit("%s has no PE signature" % binary)
        fh.seek(20, os.SEEK_CUR)
        opt = fh.read(32)
    magic = int.from_bytes(opt[:2], "little")
    if magic == 0x20B:
        return int.from_bytes(opt[24:32], "little")
    if magic == 0x10B:
        return int.from_bytes(opt[28:32], "little")
    raise SystemExit("unknown optional header magic 0x%x in %s" % (magic, binary))


def dump_etl(etl, xperf):
    cmd = [xperf, "-i", etl, "-a", "dumper"]
    log("running: %s" % " ".join(cmd))
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                            text=True, errors="replace", bufsize=1 << 20)
    yield from proc.stdout
    proc.wait()


def module_of(field):
    return field.split("!", 1)[0].strip().strip('"').lower()


def convert(lines, module, pref, out):
    bases = {}                                  # (pid, module) -> load address
    tid_pid = {}
    leading = {}                                # (ts, tid) -> sample IP
    samples = collections.defaultdict(dict)     # (ts, tid) -> {no: (src, dst)}
    seen_lbr = kept_lbr = 0

    for line in lines:
        if "," not in line:
            continue
        kind, sep, rest = line.partition(",")
        kind = kind.strip()
        if not sep:
            continue
        f = [x.strip() for x in rest.split(",")]

        if kind == "ImageId" and len(f) >= 6:
            name = f[5].strip('"').lower()
            if name != module:
                continue
            m = re.search(r"\((\s*\d+)\)$", f[1])
            if not m or not f[2].startswith("0x"):
                continue
            bases[(int(m.group(1)), name)] = int(f[2], 16)

        elif kind == "PmcInterrupt" and len(f) >= 6:
            m = re.search(r"\((\s*\d+)\)$", f[1])
            if not m:
                continue
            tid_pid[f[2]] = int(m.group(1))
            # The counter overflows inside the interrupt, so this PC is usually
            # in ntoskrnl rather than the module being profiled.
            if f[3].startswith("0x") and module_of(f[5]) == module:
                leading[(f[0], f[2])] = int(f[3], 16)

        elif kind == "LBR" and len(f) >= 7:
            seen_lbr += 1
            if not f[3].startswith("0x") or not f[5].startswith("0x"):
                continue
            src_mod, dst_mod = module_of(f[4]), module_of(f[6])
            if src_mod != module and dst_mod != module:
                continue
            try:
                no = int(f[2])
            except ValueError:
                continue
            samples[(f[0], f[1])][no] = (int(f[3], 16), src_mod,
                                         int(f[5], 16), dst_mod)
            kept_lbr += 1

    if not bases:
        raise SystemExit("no ImageId record for module %r in the trace" % module)
    log("load addresses: %s" % {("pid %d" % p): hex(b) for (p, _), b in bases.items()})
    log("LBR rows: %d in trace, %d touching %s" % (seen_lbr, kept_lbr, module))

    single = next(iter(bases.values())) if len(bases) == 1 else None
    written = dropped = 0

    for (ts, tid), entries in samples.items():
        pid = tid_pid.get(tid)
        base = bases.get((pid, module), single)
        if base is None:
            dropped += 1
            continue

        def conv(addr, mod):
            return pref + addr - base if mod == module else EXTERNAL

        toks = []
        # No. 1 is the most recent branch, which is the FIFO order llvm-profgen
        # wants; measured 100% valid fall-through ranges vs 49% reversed.
        for no in sorted(entries):
            src, src_mod, dst, dst_mod = entries[no]
            s, d = conv(src, src_mod), conv(dst, dst_mod)
            if s == EXTERNAL and d == EXTERNAL:
                continue
            toks.append("0x%x/0x%x/P/-/-/0" % (s, d))
        if not toks:
            dropped += 1
            continue

        ip = leading.get((ts, tid))
        ip = pref + ip - base if ip else int(toks[0].split("/")[1], 16)
        # Leading IP must have no 0x prefix while LBR addresses must have one;
        # parseAddress() rejects the line otherwise.
        out.write("%x %s\n" % (ip, " ".join(toks)))
        written += 1

    log("samples written: %d, dropped: %d" % (written, dropped))
    return written


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    src = ap.add_mutually_exclusive_group(required=True)
    src.add_argument("--etl", help="ETL captured with -LastBranch")
    src.add_argument("--dump", help="text already produced by xperf -a dumper")
    ap.add_argument("--binary", help="unstripped PE the profile is for")
    ap.add_argument("--module", help="module name in the trace (default: basename of --binary)")
    ap.add_argument("--preferred-base", type=lambda s: int(s, 0),
                    help="override the PE ImageBase")
    ap.add_argument("-o", "--output", required=True)
    ap.add_argument("--xperf", default=os.path.join(WPT, "xperf.exe"))
    args = ap.parse_args()

    module = (args.module or (args.binary and os.path.basename(args.binary)) or "").lower()
    if not module:
        ap.error("need --module or --binary")
    pref = args.preferred_base
    if pref is None:
        if not args.binary:
            ap.error("need --binary or --preferred-base")
        pref = preferred_base(args.binary)
    log("module %s, preferred base 0x%x" % (module, pref))

    if args.dump:
        fh = open(args.dump, "r", errors="replace")
        lines = fh
    else:
        fh = None
        lines = dump_etl(args.etl, args.xperf)
    try:
        with open(args.output, "w", newline="\n") as out:
            if not convert(lines, module, pref, out):
                raise SystemExit("no samples written")
    finally:
        if fh:
            fh.close()
    log("wrote %s" % args.output)


if __name__ == "__main__":
    main()
