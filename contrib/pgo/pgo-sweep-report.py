#!/usr/bin/env python3
"""Aggregate pgo-sweep runs across passes, or check that a profile annotated anything."""
import json, os, re, shlex, statistics, subprocess, sys

UNIT = {'ns': 1.0, 'us': 1e3, 'ms': 1e6, 's': 1e9}


def load(path):
    """name -> median real time in ns. Falls back to plain runs if aggregates are absent."""
    with open(path) as f:
        doc = json.load(f)
    out, plain = {}, {}
    for b in doc.get('benchmarks', []):
        name = b['name']
        t = b['real_time'] * UNIT[b.get('time_unit', 'ns')]
        if b.get('aggregate_name') == 'median':
            out[name.removesuffix('_median')] = t
        elif b.get('run_type') != 'aggregate':
            plain.setdefault(name, t)
    return out or plain


def fmt(ns):
    for u, s in (('s', 1e9), ('ms', 1e6), ('us', 1e3)):
        if ns >= s:
            return f'{ns / s:.3f} {u}'
    return f'{ns:.3f} ns'


def annotation(build_dir, tu):
    """Recompile one TU to LLVM IR and count the profile metadata clang attached."""
    with open(os.path.join(build_dir, 'compile_commands.json')) as f:
        cc = json.load(f)
    e = next((x for x in cc if x['file'].replace(chr(92), '/').endswith(tu)), None)
    if e is None:
        sys.exit(f'error: {tu} not in {build_dir}/compile_commands.json')
    cmd = e['command'].replace(chr(92), '/')
    cmd = re.sub(r'\s-o\s+\S+', '', cmd)
    cmd = re.sub(r'\s-c\s', ' ', cmd)
    cmd += ' -S -emit-llvm -o -'
    p = subprocess.run(shlex.split(cmd), cwd=e.get('directory', build_dir),
                       capture_output=True, text=True)
    ir = p.stdout
    if not ir:
        sys.exit(f'error: no IR from {tu}\n{p.stderr[:400]}')
    unprofiled = 'profile-instr-unprofiled' in p.stderr
    print(f'branch_weights={ir.count("branch_weights")} '
          f'entry_count={ir.count("function_entry_count")}'
          + (' UNPROFILED' if unprofiled else ''))


def compare(outdir, passes, labels, only=None):
    # pass 1 is warm-up; contrib/pgo/pgo-compare.sh measures 4pp of systematic error on a binary
    # timed straight after its own LTO link, and the first pass absorbs the same effect
    per_pass = {l: [] for l in labels}
    for p in range(2, passes + 1):
        for l in labels:
            path = os.path.join(outdir, f'pass{p}', l + '.json')
            if os.path.exists(path):
                per_pass[l].append(load(path))
    missing = [l for l in labels if not per_pass[l]]
    if missing:
        sys.exit(f'error: no pass data for {", ".join(missing)}')

    names = [n for n in per_pass[labels[0]][0]
             if all(n in r for runs in per_pass.values() for r in runs)]
    if not names:
        sys.exit('error: runs share no benchmark names')
    if only:
        kept = [n for n in names if re.search(only, n)]
        if not kept:
            sys.exit(f'error: --only={only} matched none of {len(names)} benchmarks')
        # say what was dropped rather than printing a quietly narrower table
        if len(kept) != len(names):
            print(f'  --only={only}: {len(kept)} of {len(names)} benchmarks '
                  f'({", ".join(n for n in names if n not in kept)} excluded)')
        names = kept

    med = {l: {n: statistics.median(r[n] for r in per_pass[l]) for n in names} for l in labels}
    base = labels[0]
    # Noise wants as many timings of one unchanged binary as possible. When the build phase
    # recorded hashes, configs often collapse into a single image -- the cs profile stops
    # changing codegen after the first round -- and then every pass of every label in that group
    # is another A/A sample. The baseline's own pass spread gives far fewer.
    group = [base]
    hpath = os.path.join(outdir, 'hashes.txt')
    if os.path.exists(hpath):
        hashes = dict(l.split() for l in open(hpath) if len(l.split()) == 2)
        by_hash = {}
        for l in labels:
            if l in hashes:
                by_hash.setdefault(hashes[l], []).append(l)
        best = max(by_hash.values(), key=len, default=[base])
        if len(best) > 1:
            group = best
    pool = {n: [r[n] for l in group for r in per_pass[l]] for n in names}
    noise = {n: max(pool[n]) / min(pool[n]) for n in names}

    w = max(max(len(n) for n in names), len('geomean vs ' + base))
    head = f'  {"benchmark":<{w}}  {base:>12}'
    for l in labels[1:]:
        head += f'  {l:>12} {"ratio":>8}'
    head += f'  {"noise":>8}'
    print(head)
    print(f'  {"-" * (len(head) - 2)}')
    for n in names:
        row = f'  {n:<{w}}  {fmt(med[base][n]):>12}'
        for l in labels[1:]:
            row += f'  {fmt(med[l][n]):>12} {med[l][n] / med[base][n]:>7.3f}x'
        row += f'  {noise[n]:>7.3f}x'
        print(row)
    print(f'  {"-" * (len(head) - 2)}')

    # geometric mean of per-benchmark ratios: the aggregate that is not dominated by whichever
    # benchmark happens to take milliseconds while the rest take microseconds
    geo = f'  {"geomean vs " + base:<{w}}  {"1.000x":>12}'
    gmean = {}
    for l in labels[1:]:
        g = 1.0
        for n in names:
            g *= med[l][n] / med[base][n]
        gmean[l] = g ** (1.0 / len(names))
        geo += f'  {"":>12} {gmean[l]:>7.3f}x'
    gn = 1.0
    for n in names:
        gn *= noise[n]
    geo += f'  {gn ** (1.0 / len(names)):>7.3f}x'
    print(geo)
    used = len(per_pass[base])
    print(f'\n  passes used: {used} of {passes} (pass 1 discarded), {len(names)} benchmarks')
    if len(group) > 1:
        print('  noise from ' + str(len(group) * used) + ' timings of one binary: '
              + ' == '.join(group))
    else:
        print('  noise from ' + str(used) + ' timings of ' + base)
    if used < 3:
        print('  note: noise needs 3+ surviving passes to mean anything')

    # An effect smaller than the noise on an unchanged binary is not an effect. Said out loud
    # because a table with plausible-looking ratios reads as a result whatever the noise column
    # says next to it.
    if gmean and used >= 3:
        spread = (gn ** (1.0 / len(names))) - 1.0
        biggest = max(gmean, key=lambda l: abs(1.0 - gmean[l]))
        effect = abs(1.0 - gmean[biggest])
        if spread >= effect:
            print()
            print(f'  !!! noise {spread * 100:.1f}% swamps the largest effect '
                  f'({biggest} {gmean[biggest]:.3f}x, {effect * 100:.1f}%)'
                  ' -- this table says nothing.')
            print('      re-measure on a quiet machine.')
        else:
            mute = [l for l in gmean if abs(1.0 - gmean[l]) < spread]
            if spread >= effect / 2:
                print(f'  note: noise {spread * 100:.1f}% against a largest effect of '
                      f'{effect * 100:.1f}% -- read differences under that as absent.')
            if mute:
                print('  indistinguishable from ' + base + ': ' + ', '.join(sorted(mute)))


if __name__ == '__main__':
    if len(sys.argv) == 4 and sys.argv[1] == '--annotation':
        annotation(sys.argv[2], sys.argv[3])
    elif len(sys.argv) >= 5 and sys.argv[1] == '--compare':
        argv = sys.argv[2:]
        only = next((a.split('=', 1)[1] for a in argv if a.startswith('--only=')), None)
        argv = [a for a in argv if not a.startswith('--only=')]
        compare(argv[0], int(argv[1]), argv[2:], only)
    else:
        sys.exit('usage: pgo-sweep-report.py --annotation <build-dir> <tu>\n'
                 '       pgo-sweep-report.py --compare <outdir> <passes> [--only=<regex>] <label>...')
