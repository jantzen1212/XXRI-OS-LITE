#!/usr/bin/env python3
"""xxri-sound-modules.py KERNELDIR TC_SHIPPED TC_BUILT OUTDIR

Pick what goes into xxri-sound.tcz from an installed module tree of the x86_64
build (KERNELDIR = lib/modules/<release>/kernel) and copy it to OUTDIR,
gzip'ed like Tiny Core's:

  - everything under sound/ (xxri-sound.tcz replaces Tiny Core's alsa-modules);
  - modules Tiny Core's config doesn't build at all (TC_BUILT), i.e. what the
    kernel fragment added outside sound/;
  - what those depend on that Tiny Core's packages don't ship (TC_SHIPPED).

TC_SHIPPED and TC_BUILT are lists of module paths below kernel/, '#' comments.
"""
import gzip, os, subprocess, sys


def plist(path):
    return {l.strip() for l in open(path) if l.strip() and not l.startswith('#')}


def name(rel):
    return os.path.basename(rel)[:-3].replace('-', '_')


def main():
    kdir, shipped, built, out = sys.argv[1:5]
    shipped, built = plist(shipped), plist(built)
    mods = sorted(os.path.relpath(os.path.join(d, f), kdir)
                  for d, _, fs in os.walk(kdir) for f in fs if f.endswith('.ko'))
    by_name = {name(m): m for m in mods}
    shipped_names = {name(m) for m in shipped}
    pick = {m for m in mods if m.startswith('sound/') or m not in built}
    todo = list(pick)
    while todo:
        m = todo.pop()
        deps = subprocess.run(['modinfo', '-F', 'depends', os.path.join(kdir, m)],
                              capture_output=True, text=True, check=True).stdout.strip()
        for d in filter(None, deps.replace('-', '_').split(',')):
            if d in shipped_names and not by_name.get(d, '').startswith('sound/'):
                continue
            if d not in by_name:
                sys.exit('%s needs %s, which this build does not have' % (m, d))
            if by_name[d] not in pick:
                pick.add(by_name[d]); todo.append(by_name[d])
    for m in sorted(pick):
        dst = os.path.join(out, m + '.gz')
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        with open(os.path.join(kdir, m), 'rb') as src, open(dst, 'wb') as f:
            with gzip.GzipFile(filename='', mode='wb', compresslevel=9, fileobj=f, mtime=0) as g:
                g.write(src.read())
    other = sorted(m for m in pick if not m.startswith('sound/'))
    print('xxri-sound-modules: %d modules (%d under sound/; also %s)'
          % (len(pick), len(pick) - len(other), ' '.join(name(m) for m in other)))


if __name__ == '__main__':
    main()
