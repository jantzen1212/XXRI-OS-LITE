#!/usr/bin/env python3
"""isa-sentinel-map.py HITS.tsv SOURCE... - which file did each hit come from?

HITS.tsv is isa-sentinel's output.  A user-space instruction at guest address
A was mapped from an ELF file whose bytes at some file offset F with
F % 4096 == A % 4096 equal the instruction and the ones after it (the
"window" column).  SOURCEs are the files the guest could have run: a core.gz,
.tcz extensions, directories.  Prints one line per hit: class, exec count,
disassembly, and the file(s) holding those bytes (or "?" - JIT code such as
V8's, which exists in no file, or a window too short to be unique).
"""
import csv, os, subprocess, sys, tempfile


def elf_files(src, tmp):
    if os.path.isdir(src):
        for top, _, names in os.walk(src):
            for n in names:
                yield os.path.join(top, n), os.path.relpath(os.path.join(top, n), src)
    elif src.endswith('.tcz'):
        d = tempfile.mkdtemp(dir=tmp)
        subprocess.run(['unsquashfs', '-q', '-n', '-d', d + '/x', src], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        for p, rel in elf_files(d + '/x', tmp):
            yield p, os.path.basename(src) + ':/' + rel
    elif src.endswith('.gz'):
        d = tempfile.mkdtemp(dir=tmp)
        subprocess.run('zcat "%s" | (cd "%s" && cpio -idm --quiet 2>/dev/null)' % (src, d), shell=True)
        for p, rel in elf_files(d, tmp):
            yield p, os.path.basename(src) + ':/' + rel


def main():
    hits = [r for r in csv.DictReader(open(sys.argv[1]), delimiter='\t')]
    tmp = tempfile.mkdtemp()
    blobs = []
    for src in sys.argv[2:]:
        for p, rel in elf_files(src, tmp):
            if os.path.islink(p) or not os.path.isfile(p):
                continue
            with open(p, 'rb') as f:
                b = f.read()
            if b[:4] == b'\x7fELF':
                blobs.append((rel, b))
    for h in sorted(hits, key=lambda r: (r['class'], r['space'], int(r['order']))):
        if h['space'] != 'user':
            print('%s\t%s\tkernel %s\t%s' % (h['class'], h['count'], h['vaddr'], h['disas']))
            continue
        win = bytes.fromhex(h['window'])
        off = int(h['vaddr'], 16) % 4096
        where = []
        for rel, b in blobs:
            i = b.find(win)
            while i >= 0:
                if i % 4096 == off:
                    where.append(rel)
                    break
                i = b.find(win, i + 1)
        print('%s\t%s\t%s\t%s' % (h['class'], h['count'], h['disas'], ' '.join(sorted(set(where))) or '?'))
    subprocess.run(['rm', '-rf', tmp])


if __name__ == '__main__':
    main()
