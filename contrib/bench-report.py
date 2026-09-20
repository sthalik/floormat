#!/usr/bin/env python3
"""Tabulate google-benchmark JSON output, one run or several side by side."""
import json, os, sys

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


def one(path, label):
    r = load(path)
    w = max((len(k) for k in r), default=10)
    for k, v in r.items():
        print(f'  {k:<{w}}  {fmt(v):>12}')
    print(f'  {"-" * (w + 14)}')
    print(f'  {"total (" + str(len(r)) + " tests)":<{w}}  {fmt(sum(r.values())):>12}')


def compare(outdir, labels):
    runs = {}
    for l in labels:
        p = os.path.join(outdir, l + '.json')
        if not os.path.exists(p):
            sys.exit(f'error: no {p}')
        runs[l] = load(p)
    names = [n for n in runs[labels[0]] if all(n in r for r in runs.values())]
    if not names:
        sys.exit('error: runs share no benchmark names')
    base = labels[0]
    w = max(len(n) for n in names)
    head = f'  {"benchmark":<{w}}  {base:>12}'
    for l in labels[1:]:
        head += f'  {l:>12} {"ratio":>8}'
    print(head)
    print(f'  {"-" * (len(head) - 2)}')
    for n in names:
        row = f'  {n:<{w}}  {fmt(runs[base][n]):>12}'
        for l in labels[1:]:
            r = runs[l][n] / runs[base][n]
            row += f'  {fmt(runs[l][n]):>12} {r:>7.3f}x'
        print(row)
    print(f'  {"-" * (len(head) - 2)}')
    tot = f'  {"sum":<{w}}  {fmt(sum(runs[base][n] for n in names)):>12}'
    for l in labels[1:]:
        s, sb = sum(runs[l][n] for n in names), sum(runs[base][n] for n in names)
        tot += f'  {fmt(s):>12} {s / sb:>7.3f}x'
    print(tot)
    # geometric mean of per-benchmark ratios: the aggregate that is not dominated by whichever
    # benchmark happens to take milliseconds while the rest take microseconds
    geo = f'  {"geomean vs " + base:<{w}}  {"1.000x":>12}'
    for l in labels[1:]:
        g = 1.0
        for n in names:
            g *= runs[l][n] / runs[base][n]
        g **= 1.0 / len(names)
        geo += f'  {"":>12} {g:>7.3f}x'
    print(geo)


if __name__ == '__main__':
    if len(sys.argv) >= 4 and sys.argv[1] == '--one':
        one(sys.argv[2], sys.argv[3])
    elif len(sys.argv) >= 4 and sys.argv[1] == '--compare':
        compare(sys.argv[2], sys.argv[3:])
    else:
        sys.exit('usage: bench-report.py --one <json> <label> | --compare <outdir> <label>...')
