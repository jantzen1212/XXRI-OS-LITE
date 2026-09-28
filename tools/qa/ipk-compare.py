#!/usr/bin/env python3
"""ipk-compare.py DIR_A DIR_B - compare the payload of every PTXdist package
(.ipk) present in both directories, file by file: content sha256, mode, owner
and link target.  Two builds of the same sources in different directories
must agree: a difference means the output depends on where or when it was
built (QA only)."""
import hashlib, io, os, sys, tarfile


def payload(path):
    data = open(path, 'rb').read()
    pos, members = 8, {}                       # ar: "!<arch>\n", 60-byte headers
    while pos < len(data):
        h = data[pos:pos + 60]
        name = h[:16].decode().strip().rstrip('/')
        size = int(h[48:58].decode().strip())
        members[name] = data[pos + 60:pos + 60 + size]
        pos += 60 + size + (size & 1)
    name = next(n for n in members if n.startswith('data.tar'))
    out = {}
    with tarfile.open(fileobj=io.BytesIO(members[name])) as t:
        for m in t.getmembers():
            if m.isfile():
                v = hashlib.sha256(t.extractfile(m).read()).hexdigest()[:16]
            elif m.issym():
                v = '-> ' + m.linkname
            else:
                v = 'dir' if m.isdir() else 'other'
            out[m.name] = (v, oct(m.mode), m.uid, m.gid)
    return out


def main():
    a, b = sys.argv[1:3]
    common = sorted(set(f for f in os.listdir(a) if f.endswith('.ipk')) &
                    set(f for f in os.listdir(b) if f.endswith('.ipk')))
    same = 0
    for f in common:
        pa, pb = payload(os.path.join(a, f)), payload(os.path.join(b, f))
        d = [k for k in sorted(set(pa) | set(pb)) if pa.get(k) != pb.get(k)]
        if d:
            print('DIFF %s: %d of %d entries differ, e.g. %s' % (f, len(d), len(pa), ', '.join(d[:4])))
        else:
            same += 1
            print('same %s (%d entries)' % (f, len(pa)))
    print('%d identical, %d different, of %d common packages' % (same, len(common) - same, len(common)))


if __name__ == '__main__':
    main()
