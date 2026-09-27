#!/usr/bin/env python3
"""cpio-diff.py A.gz B.gz - compare two newc initramfs archives by content.

Ignores archive order, inode and device numbers (which differ between a
checkout and the original tree and carry no meaning after unpacking); reports
entries whose name set, mode, owner, mtime or data differ.  Concatenated
gzip members are followed, as the kernel does.
Exit 0 when the archives unpack to the same tree.
"""
import hashlib, sys, zlib


def gunzip_all(data):
    out, rest = [], data
    while rest:
        d = zlib.decompressobj(31)
        out.append(d.decompress(rest))
        rest = d.unused_data.lstrip(b'\0')
    return b''.join(out)


def entries(path):
    raw = gunzip_all(open(path, 'rb').read())
    off, ents = 0, {}
    while off < len(raw):
        h = raw[off:off + 110]
        if h[:6] != b'070701':
            raise SystemExit('%s: cpio stream broken at %d' % (path, off))
        f = lambda a, b: int(h[a:b], 16)
        mode, uid, gid, mtime, fsize, nsize = f(14, 22), f(22, 30), f(30, 38), f(46, 54), f(54, 62), f(94, 102)
        name = raw[off + 110:off + 110 + nsize - 1].decode('latin1')
        doff = (off + 110 + nsize + 3) & ~3
        if name == 'TRAILER!!!':
            off = (doff + 511) & ~511
            while off < len(raw) and raw[off:off + 6] != b'070701':
                off += 4
            continue
        data = raw[doff:doff + fsize]
        ents[name.lstrip('./') or '.'] = (mode, uid, gid, mtime, hashlib.sha256(data).hexdigest()[:16])
        off = (doff + fsize + 3) & ~3
    return ents


def main():
    a, b = entries(sys.argv[1]), entries(sys.argv[2])
    only_a = sorted(set(a) - set(b))
    only_b = sorted(set(b) - set(a))
    diff = [(k, a[k], b[k]) for k in sorted(set(a) & set(b)) if a[k] != b[k]]
    for k in only_a:
        print('only in A: %s' % k)
    for k in only_b:
        print('only in B: %s' % k)
    for k, x, y in diff:
        what = [n for n, i in (('mode', 0), ('uid', 1), ('gid', 2), ('mtime', 3), ('data', 4)) if x[i] != y[i]]
        print('differs:   %s (%s) A=%o/%d B=%o/%d' % (k, ','.join(what), x[0], x[3], y[0], y[3]))
    print('%d entries in A, %d in B: %d only-A, %d only-B, %d differ'
          % (len(a), len(b), len(only_a), len(only_b), len(diff)))
    return 1 if (only_a or only_b or diff) else 0


if __name__ == '__main__':
    sys.exit(main())
