#!/usr/bin/env python3
"""tcz-deps.py TCZ CORE.gz CDE_DIR - the .dep list a Tiny Core extension needs.

For every NEEDED soname of the ELF files in TCZ that neither TCZ itself nor
core.gz provides, names the extension in CDE_DIR/optional that does (the
first one in onboot.lst order).  Prints .dep lines; sonames nobody provides
go to stderr and make the exit status 1.
"""
import os, re, subprocess, sys, tempfile


def files(tcz_or_gz, tmp):
    d = tempfile.mkdtemp(dir=tmp)
    if tcz_or_gz.endswith('.gz'):
        subprocess.run('zcat "%s" | (cd "%s" && cpio -idm --quiet 2>/dev/null)' % (tcz_or_gz, d), shell=True)
    else:
        subprocess.run(['unsquashfs', '-q', '-n', '-f', '-d', d, tcz_or_gz],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    for top, _, names in os.walk(d):
        for n in names:
            yield os.path.join(top, n), n


def elf(p):
    try:
        with open(p, 'rb') as f:
            return f.read(4) == b'\x7fELF'
    except OSError:
        return False


def main():
    tcz, core, cde = sys.argv[1:4]
    tmp = tempfile.mkdtemp()
    own, needed = set(), set()
    for p, n in files(tcz, tmp):
        own.add(n)
        if elf(p) and not os.path.islink(p):
            out = subprocess.run(['readelf', '-dW', p], capture_output=True, text=True).stdout
            needed |= set(re.findall(r'\(NEEDED\)\s+Shared library: \[(.+?)\]', out))
    base = {n for _, n in files(core, tmp)}
    want = sorted(needed - own - base)
    order = [l.strip() for l in open(os.path.join(cde, 'onboot.lst')) if l.strip()]
    others = sorted(f for f in os.listdir(os.path.join(cde, 'optional')) if f.endswith('.tcz'))
    provides = {}
    for ext in order + [o for o in others if o not in order]:
        if ext == os.path.basename(tcz):
            continue
        for _, n in files(os.path.join(cde, 'optional', ext), tmp):
            provides.setdefault(n, ext)
    deps, bad = [], 0
    for so in want:
        if so in provides:
            if provides[so] not in deps:
                deps.append(provides[so])
        else:
            print('no provider for %s' % so, file=sys.stderr)
            bad = 1
    for d in deps:
        print(d)
    subprocess.run(['rm', '-rf', tmp])
    sys.exit(bad)


if __name__ == '__main__':
    main()
