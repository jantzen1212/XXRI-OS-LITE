#!/usr/bin/env python3
"""xxri-fw-select - a reviewed subset of a linux-firmware release.

A list names firmware the way a driver requests it (one name per line, '#'
comments), e.g. "nvidia/ad104/gsp/gsp-535.113.01.bin".  The release's own
WHENCE says what each name is: a file (File:/RawFile:) or a link (Link:),
and links may name directories ("nvidia/ad104 -> ad102").  Every name is
resolved through WHENCE; a name that is neither a file nor reachable through
links is an error, so a list can never silently ship less than it says.

  xxri-fw-select.py resolve WHENCE LIST...
      print "file NAME" / "link NAME TARGET" lines (the install plan)
  xxri-fw-select.py extract TARBALL OUTDIR LIST...
      unpack exactly the plan from the release tarball into OUTDIR: files
      0644, directories 0755, links as the relative links WHENCE defines;
      also OUTDIR's parent gets the release's LICENSE/LICENCE files that
      WHENCE names for the plan's files (licenses.txt)

Only the Python standard library is used (tarfile streams .tar.gz/.tar.xz).
"""
import os
import posixpath
import sys
import tarfile


def parse_whence(text):
    files, links, licences = set(), {}, {}
    current = []
    for raw in text.splitlines():
        line = raw.strip()
        key, sep, val = line.partition(':')
        if not sep:
            continue
        val = val.strip()
        if key in ('File', 'RawFile'):
            name = val.strip('"')
            files.add(name)
            current.append(name)
        elif key == 'Link':
            name, arrow, target = val.partition(' -> ')
            if not arrow:
                raise SystemExit('WHENCE: bad Link line: %s' % raw)
            links[name.strip().strip('"')] = target.strip().strip('"')
        elif key in ('Licence', 'License'):
            # "Licence: Redistributable. See LICENSE.amdgpu for details."
            words = [w.strip('.,;()') for w in val.split()]
            lic = [w for w in words if w.startswith(('LICENSE', 'LICENCE'))]
            for name in current:
                licences.setdefault(name, set()).update(lic)
        elif line.startswith('Driver'):
            current = []
    return files, links, licences


def resolve(name, files, links):
    """(real file, [(link, target), ...]) for a requested name."""
    used, seen = [], set()
    path = posixpath.normpath(name)
    while True:
        if path in files:
            return path, used
        parts = path.split('/')
        for i in range(1, len(parts) + 1):
            prefix = '/'.join(parts[:i])
            if prefix in links:
                if prefix in seen:
                    raise SystemExit('%s: link loop at %s' % (name, prefix))
                seen.add(prefix)
                target = links[prefix]
                used.append((prefix, target))
                base = posixpath.normpath(posixpath.join(posixpath.dirname(prefix), target))
                path = posixpath.normpath('/'.join([base] + parts[i:]))
                break
        else:
            raise SystemExit('%s: not in this linux-firmware release (no File/Link in WHENCE)' % name)


def read_lists(paths):
    names = []
    for p in paths:
        with open(p) as f:
            for line in f:
                line = line.split('#', 1)[0].strip()
                if line:
                    names.append(line)
    return names


def plan(whence_text, lists):
    files, links, licences = parse_whence(whence_text)
    real, lnk = set(), {}
    for name in read_lists(lists):
        path, used = resolve(name, files, links)
        real.add(path)
        for l, t in used:
            lnk[l] = t
    lic = set()
    for path in real:
        lic.update(licences.get(path, ()))
    return sorted(real), sorted(lnk.items()), sorted(lic)


def whence_from_tar(tarball):
    with tarfile.open(tarball, 'r|*') as t:
        for m in t:
            if m.isfile() and m.name.split('/', 1)[-1] == 'WHENCE':
                return t.extractfile(m).read().decode('utf-8', 'surrogateescape')
    raise SystemExit('%s: no WHENCE' % tarball)


def extract(tarball, outdir, lists):
    real, lnk, lic = plan(whence_from_tar(tarball), lists)
    want = set(real)
    licdir = os.path.dirname(os.path.abspath(outdir))
    wantlic = set(lic)
    for l in lic:
        wantlic.add('LICENSES/' + l)
    got, gotlic = set(), []
    os.makedirs(outdir, exist_ok=True)
    with tarfile.open(tarball, 'r|*') as t:
        for m in t:
            if not m.isfile():
                continue
            rel = m.name.split('/', 1)[-1]
            if rel in want:
                dst = os.path.join(outdir, rel)
                os.makedirs(os.path.dirname(dst), mode=0o755, exist_ok=True)
                with open(dst, 'wb') as o:
                    o.write(t.extractfile(m).read())
                os.chmod(dst, 0o644)
                got.add(rel)
            elif rel in wantlic:
                dst = os.path.join(licdir, 'licenses', os.path.basename(rel))
                os.makedirs(os.path.dirname(dst), exist_ok=True)
                with open(dst, 'wb') as o:
                    o.write(t.extractfile(m).read())
                os.chmod(dst, 0o644)
                gotlic.append(os.path.basename(rel))
    missing = want - got
    if missing:
        raise SystemExit('in WHENCE but not in %s: %s' % (tarball, ' '.join(sorted(missing))))
    for name, target in lnk:
        dst = os.path.join(outdir, name)
        os.makedirs(os.path.dirname(dst), mode=0o755, exist_ok=True)
        if os.path.lexists(dst):
            os.remove(dst)
        os.symlink(target, dst)
    for root, dirs, _ in os.walk(outdir):
        for d in dirs:
            p = os.path.join(root, d)
            if not os.path.islink(p):
                os.chmod(p, 0o755)
    with open(os.path.join(licdir, 'licenses.txt'), 'w') as o:
        o.write('\n'.join(sorted(set(gotlic))) + '\n')
    print('xxri-fw-select: %d files, %d links, licences: %s'
          % (len(real), len(lnk), ' '.join(sorted(set(gotlic))) or 'none'))


def main(argv):
    if len(argv) >= 3 and argv[1] == 'resolve':
        with open(argv[2], encoding='utf-8', errors='surrogateescape') as f:
            real, lnk, lic = plan(f.read(), argv[3:])
        for p in real:
            print('file', p)
        for l, t in lnk:
            print('link', l, t)
        for l in lic:
            print('licence', l)
    elif len(argv) >= 4 and argv[1] == 'extract':
        extract(argv[2], argv[3], argv[4:])
    else:
        sys.exit(__doc__)


if __name__ == '__main__':
    main(sys.argv)
