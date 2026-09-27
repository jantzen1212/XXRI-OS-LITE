#!/usr/bin/env python3
"""elf-closure.py CORE.gz CDE_DIR [--check PREFIX...] - can every program run?

Builds the file system a booted XXRI system sees - core.gz plus every
extension in cde/onboot.lst (and their .dep closure) laid over it the way
tce-load does (existing files win) - and checks, for each ELF file whose
path starts with one of the --check prefixes (default: every ELF that is
not identical to a Tiny Core base file):

  * every DT_NEEDED library resolves through the file's RPATH/RUNPATH
    ($ORIGIN expanded), /etc/ld.so.conf and the default /lib, /usr/lib;
  * every versioned symbol reference (GLIBC_2.x, GLIBCXX_3.4.x, ...) is
    defined by the library it resolved to.

Exit status 1 if anything is unresolved.  Needs binutils' readelf.
"""
import os, re, subprocess, sys, tempfile

LDCONF_DEFAULT = ['/lib', '/usr/lib']


def run(*a):
    return subprocess.run(a, capture_output=True, text=True).stdout


def unpack(core, cde, tmp):
    root = os.path.join(tmp, 'root')
    os.makedirs(root)
    subprocess.run('zcat "%s" | (cd "%s" && cpio -idm --quiet 2>/dev/null)' % (core, root), shell=True)
    order, seen = [], set()

    def add(name):
        if name in seen:
            return
        seen.add(name)
        dep = os.path.join(cde, 'optional', name + '.dep')
        if os.path.exists(dep):
            for d in open(dep).read().split():
                add(d.replace('KERNEL', '6.12.11-tinycore'))
        order.append(name)

    for line in open(os.path.join(cde, 'onboot.lst')):
        if line.strip():
            add(line.strip())
    missing_ext = []
    for name in order:
        tcz = os.path.join(cde, 'optional', name)
        if not os.path.exists(tcz):
            missing_ext.append(name)
            continue
        x = os.path.join(tmp, 'ext', name)
        os.makedirs(os.path.dirname(x), exist_ok=True)
        subprocess.run(['unsquashfs', '-q', '-n', '-d', x, tcz], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        # tce-load: cp -ais - files already present are kept
        for top, dirs, files in os.walk(x):
            rel = os.path.relpath(top, x)
            os.makedirs(os.path.join(root, rel), exist_ok=True)
            for f in files + [d for d in dirs if os.path.islink(os.path.join(top, d))]:
                dst = os.path.join(root, rel, f)
                if not os.path.lexists(dst):
                    os.symlink(os.path.join(top, f), dst)
    return root, order, missing_ext


def resolve(root, path):
    """follow symlinks inside the image root (links into an unpacked
    extension, made by unpack(), are followed on the host)"""
    ext = os.path.join(os.path.dirname(root), 'ext')
    p = path
    for _ in range(40):
        full = root + p
        if os.path.islink(full):
            t = os.readlink(full)
            if t.startswith(ext):
                return os.path.realpath(t) if os.path.exists(t) else None
            p = os.path.normpath(os.path.join(os.path.dirname(p), t)) if not t.startswith('/') else t
            continue
        return full if os.path.exists(full) else None
    return None


def dyn(path):
    out = run('readelf', '-dW', path)
    needed = re.findall(r'\(NEEDED\)\s+Shared library: \[(.+?)\]', out)
    rpath = re.findall(r'\((?:RPATH|RUNPATH)\)\s+Library (?:rpath|runpath): \[(.+?)\]', out)
    return needed, [d for r in rpath for d in r.split(':')]


def verneed(path):
    out = run('readelf', '-VW', path)
    req, cur = {}, None
    sec = out.split("Version needs section")
    if len(sec) < 2:
        return req
    for line in sec[1].splitlines():
        m = re.search(r'File: (\S+)', line)
        if m:
            cur = m.group(1)
            req.setdefault(cur, set())
            continue
        m = re.search(r'Name: (\S+)\s+Flags', line)
        if m and cur:
            req[cur].add(m.group(1))
    return req


_verdef = {}


def verdef(path):
    if path not in _verdef:
        out = run('readelf', '-VW', path)
        s = set()
        if 'Version definition section' in out:
            part = out.split('Version definition section')[1].split('Version needs section')[0]
            s = set(re.findall(r'Name: (\S+)', part))
        _verdef[path] = s
    return _verdef[path]


def main():
    core, cde = sys.argv[1], sys.argv[2]
    prefixes = sys.argv[sys.argv.index('--check') + 1:] if '--check' in sys.argv else None
    tmp = tempfile.mkdtemp()
    root, order, missing_ext = unpack(core, cde, tmp)
    ldconf = LDCONF_DEFAULT[:]
    lc = root + '/etc/ld.so.conf'
    if os.path.exists(lc):
        ldconf = [l.strip() for l in open(lc) if l.strip() and not l.startswith('#')] + ldconf
    problems = 0
    for e in missing_ext:
        print('MISSING EXTENSION %s (onboot.lst / .dep)' % e)
        problems += 1
    checked = 0
    for top, _, files in os.walk(root):
        for f in files:
            full = os.path.join(top, f)
            path = '/' + os.path.relpath(full, root)
            if prefixes and not any(path.startswith(p) for p in prefixes):
                continue
            real = resolve(root, path)
            if not real or os.path.isdir(real):
                continue
            with open(real, 'rb') as fh:
                if fh.read(4) != b'\x7fELF':
                    continue
            checked += 1
            needed, rpath = dyn(real)
            dirs = [d.replace('$ORIGIN', os.path.dirname(path)).replace('${ORIGIN}', os.path.dirname(path))
                    for d in rpath] + ldconf
            vn = verneed(real)
            for lib in needed:
                hit = next((resolve(root, d.rstrip('/') + '/' + lib) for d in dirs
                            if resolve(root, d.rstrip('/') + '/' + lib)), None)
                if not hit:
                    print('%s: NEEDED %s not found' % (path, lib))
                    problems += 1
                    continue
                missing = vn.get(lib, set()) - verdef(hit)
                if missing:
                    print('%s: %s lacks %s' % (path, lib, ' '.join(sorted(missing))))
                    problems += 1
    print('elf-closure: %d ELF files checked, %d extensions loaded, %d problems' % (checked, len(order), problems))
    subprocess.run(['rm', '-rf', tmp])
    sys.exit(1 if problems else 0)


if __name__ == '__main__':
    main()
