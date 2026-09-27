#!/usr/bin/env python3
"""xxri-tc-closure.py [--exclude NAME ...] [--have MANIFEST] PKG...

Maintainer tool: print manifest lines ("name.tcz md5") for the .dep closure of
the given Tiny Core 16.x packages, as published on the mirror right now.  Used
to extend config/tc-sysroot.manifest; the build itself only ever reads the
manifest and verifies each file against the md5 recorded there.

--have MANIFEST  skip packages already listed in MANIFEST
--exclude NAME   do not follow NAME (runtime-only or tooling packages)
"""
import argparse, urllib.request

MIRROR = 'http://repo.tinycorelinux.net/16.x/x86/tcz/'


def fetch(name):
    try:
        with urllib.request.urlopen(MIRROR + name, timeout=60) as r:
            return r.read().decode()
    except urllib.error.HTTPError as e:
        if e.code == 404:
            return ''
        raise


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--exclude', action='append', default=[])
    ap.add_argument('--have')
    ap.add_argument('pkgs', nargs='+')
    a = ap.parse_args()
    norm = lambda n: n if n.endswith('.tcz') else n + '.tcz'
    have = set()
    if a.have:
        for line in open(a.have):
            if line.strip() and not line.startswith('#'):
                have.add(line.split()[0])
    excl = {norm(n) for n in a.exclude}
    seen, order, todo = set(), [], [norm(p) for p in a.pkgs]
    while todo:
        p = todo.pop(0)
        if p in seen or p in excl:
            continue
        seen.add(p)
        order.append(p)
        todo += [norm(d.strip()) for d in fetch(p + '.dep').split() if d.strip()]
    for p in order:
        if p in have:
            continue
        md5 = fetch(p + '.md5.txt').split()
        if not md5:
            raise SystemExit('%s: not on the mirror' % p)
        print(p, md5[0])


if __name__ == '__main__':
    main()
