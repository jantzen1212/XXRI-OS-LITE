#!/usr/bin/env python3
"""compare-builds.py A.iso B.iso OUTDIR - objective diff of two XXRI ISOs (QA only).

Reports: ISO sizes; every file of both ISO trees by sha256 (same / differs /
only in one); core.gz entry by entry (tools/cpio-diff.py); and for each XXRI
program in core.gz its size, linked libraries and highest GLIBC symbol
version.  Writes OUTDIR/compare.txt.
"""
import hashlib, os, re, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))


def tree(iso, dest):
    subprocess.run(['xorriso', '-osirrox', 'on', '-indev', iso, '-extract', '/', dest],
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True)
    subprocess.run(['chmod', '-R', 'u+w', dest], check=True)
    files = {}
    for top, _, names in os.walk(dest):
        for n in names:
            p = os.path.join(top, n)
            if os.path.isfile(p) and not os.path.islink(p):
                files[os.path.relpath(p, dest)] = hashlib.sha256(open(p, 'rb').read()).hexdigest()
    return files


def elf_facts(path):
    out = subprocess.run(['readelf', '-dW', path], capture_output=True, text=True).stdout
    needed = re.findall(r'\(NEEDED\).*\[(.+?)\]', out)
    sym = subprocess.run(['objdump', '-T', path], capture_output=True, text=True).stdout
    vers = sorted(set(re.findall(r'GLIBC_(\d+\.\d+)', sym)), key=lambda v: tuple(map(int, v.split('.'))))
    return os.path.getsize(path), needed, (vers[-1] if vers else 'static')


def main():
    a, b, out = sys.argv[1], sys.argv[2], sys.argv[3]
    os.makedirs(out, exist_ok=True)
    rep = []
    w = tempfile.mkdtemp(dir=out)
    ta, tb = tree(a, w + '/a'), tree(b, w + '/b')
    rep.append('ISO A %s  %d bytes' % (os.path.basename(a), os.path.getsize(a)))
    rep.append('ISO B %s  %d bytes' % (os.path.basename(b), os.path.getsize(b)))
    same = [f for f in ta if tb.get(f) == ta[f]]
    diff = sorted(f for f in ta if f in tb and tb[f] != ta[f])
    only_a, only_b = sorted(set(ta) - set(tb)), sorted(set(tb) - set(ta))
    rep.append('files: %d identical, %d differ, %d only in A, %d only in B' % (len(same), len(diff), len(only_a), len(only_b)))
    rep += ['  differs: ' + f for f in diff] + ['  only A:  ' + f for f in only_a] + ['  only B:  ' + f for f in only_b]
    rep.append('')
    rep.append('core.gz, entry by entry (A vs B):')
    cd = subprocess.run([sys.executable, os.path.join(HERE, '..', 'cpio-diff.py'),
                         w + '/a/boot/core.gz', w + '/b/boot/core.gz'], capture_output=True, text=True).stdout
    lines = cd.strip().splitlines()
    kinds = {}
    for l in lines[:-1]:
        m = re.search(r'\((.*?)\)', l)
        k = l.split(':')[0] + (' (' + m.group(1) + ')' if m else '')
        kinds[k] = kinds.get(k, 0) + 1
    rep += ['  %5d  %s' % (n, k) for k, n in sorted(kinds.items(), key=lambda x: -x[1])]
    rep += ['  ' + l for l in lines if 'data' in l][:40]
    rep.append('  ' + lines[-1])
    rep.append('')
    rep.append('XXRI programs (size / NEEDED / highest GLIBC symbol):')
    for side, t in (('A', w + '/a'), ('B', w + '/b')):
        r = t + '/root'
        os.makedirs(r, exist_ok=True)
        subprocess.run('zcat %s/boot/core.gz | (cd %s && cpio -idm --quiet "usr/local/bin/xxri-*" "usr/local/sbin/xxri-bootsplash")'
                       % (t, r), shell=True, stderr=subprocess.DEVNULL)
    for prog in sorted(os.listdir(w + '/a/root/usr/local/bin')) + ['../sbin/xxri-bootsplash']:
        pa = os.path.normpath(w + '/a/root/usr/local/bin/' + prog)
        pb = os.path.normpath(w + '/b/root/usr/local/bin/' + prog)
        if not os.path.isfile(pa) or open(pa, 'rb').read(4) != b'\x7fELF':
            continue
        fa = elf_facts(pa)
        fb = elf_facts(pb) if os.path.isfile(pb) else (0, [], '-')
        rep.append('  %-22s A %7d B %7d  glibc A %-6s B %-6s  libs %s' % (
            os.path.basename(prog), fa[0], fb[0], fa[2], fb[2],
            'same' if fa[1] == fb[1] else 'A=%s B=%s' % (','.join(fa[1]), ','.join(fb[1]))))
    subprocess.run(['rm', '-rf', w])
    open(os.path.join(out, 'compare.txt'), 'w').write('\n'.join(rep) + '\n')
    print('\n'.join(rep))


if __name__ == '__main__':
    main()
