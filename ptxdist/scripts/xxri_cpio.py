"""xxri_cpio - read and write newc ("070701") initramfs archives.

Entries are dicts: name (no leading "./"), mode, uid, gid, mtime, data
(bytes; the target for symlinks), rdevmajor, rdevminor.  Inode numbers and
link counts are not kept: the writer numbers inodes in output order, which is
what makes its output depend on nothing but the entries.
"""
import zlib

TRAILER = 'TRAILER!!!'


def gunzip_all(data):
    """Decompress concatenated gzip members, as the kernel does."""
    out, rest = [], data
    while rest:
        d = zlib.decompressobj(31)
        out.append(d.decompress(rest))
        rest = d.unused_data.lstrip(b'\0')
    return b''.join(out)


def _pad(n):
    return (4 - n % 4) % 4


def read(blob):
    """Entries of a newc archive (bytes, already decompressed), in order.
    Hard-linked files are returned as full copies of their data."""
    entries, off, seen = [], 0, {}
    while off < len(blob):
        if blob[off:off + 6] != b'070701':
            raise ValueError('bad cpio magic at %d' % off)
        f = [int(blob[off + 6 + 8 * i: off + 14 + 8 * i], 16) for i in range(13)]
        ino, mode, uid, gid, nlink, mtime, size, _, _, rmaj, rmin, namesize, _ = f
        off += 110
        name = blob[off: off + namesize - 1].decode('utf-8', 'surrogateescape')
        off += namesize + _pad(110 + namesize)
        data = blob[off: off + size]
        off += size + _pad(size)
        if name == TRAILER:
            break
        if name.startswith('./'):
            name = name[2:]
        if name in ('', '.'):
            continue
        if nlink > 1 and (mode & 0o170000) == 0o100000:
            if size == 0 and ino in seen:
                data = seen[ino]
            elif size:
                seen[ino] = data
        entries.append(dict(name=name, mode=mode, uid=uid, gid=gid, mtime=mtime,
                            data=data, rdevmajor=rmaj, rdevminor=rmin))
    return entries


def write(entries):
    """A newc archive (bytes) of the entries, in the given order."""
    out = []
    for ino, e in enumerate(entries + [dict(name=TRAILER, mode=0, uid=0, gid=0, mtime=0,
                                            data=b'', rdevmajor=0, rdevminor=0)], 1):
        name = e['name'].encode('utf-8', 'surrogateescape') + b'\0'
        data = e['data']
        is_dir = (e['mode'] & 0o170000) == 0o040000
        hdr = '070701' + ''.join('%08X' % v for v in (
            ino if e['name'] != TRAILER else 0, e['mode'], e['uid'], e['gid'],
            2 if is_dir else 1, e['mtime'], len(data), 0, 0,
            e['rdevmajor'], e['rdevminor'], len(name), 0))
        out += [hdr.encode(), name, b'\0' * _pad(110 + len(name)), data, b'\0' * _pad(len(data))]
    blob = b''.join(out)
    return blob + b'\0' * _pad(len(blob))
