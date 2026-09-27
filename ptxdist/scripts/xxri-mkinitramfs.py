#!/usr/bin/env python3
"""xxri-mkinitramfs.py BASE.gz REMOVE-LIST ROOT OUT.gz

Compose XXRI's core.gz: Tiny Core's released initramfs (BASE.gz) with the
XXRI packages (ROOT, the tree PTXdist extracted from their packages, read
inside the same fakeroot session so owners are the declared ones) laid over
it.  Deterministic: entries are sorted by path, inode numbers follow from
the order, Tiny Core's entries keep their own timestamps and XXRI's get
SOURCE_DATE_EPOCH, and gzip carries no name or time.

Merge rules, in this order:
  1. every path in REMOVE-LIST (and everything below it) leaves the base;
  2. a directory that exists in the base keeps the base's owner and mode -
     packages only create the directories they need, with default modes, and
     must not change Tiny Core's (e.g. /opt is root:staff 2775 so the tc user
     can edit bootlocal.sh);
  3. a regular file that replaces a base regular file keeps the base file's
     owner and permission bits: XXRI changes the content of Tiny Core files
     such as /etc/passwd, not who may change them;
  4. anything else from ROOT is taken as installed;
  5. replacing a base directory with a non-directory (or the reverse) is an
     error unless rule 1 removed it first.
"""
import gzip, os, stat, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import xxri_cpio


def main():
    base_gz, remove_list, root, out = sys.argv[1:5]
    epoch = int(os.environ['SOURCE_DATE_EPOCH'])
    base = {e['name']: e for e in xxri_cpio.read(xxri_cpio.gunzip_all(open(base_gz, 'rb').read()))}

    removed = 0
    for line in open(remove_list):
        p = line.split('#')[0].strip().strip('/')
        if not p:
            continue
        if p not in base:
            sys.exit('remove list: %s is not in the base' % p)
        for n in [n for n in base if n == p or n.startswith(p + '/')]:
            del base[n]
            removed += 1

    added = replaced = 0
    for top, dirs, files in os.walk(root):
        dirs.sort()
        for name in sorted(dirs) + sorted(files):
            path = os.path.join(top, name)
            rel = os.path.relpath(path, root)
            st = os.lstat(path)
            kind = stat.S_IFMT(st.st_mode)
            if kind == stat.S_IFDIR:
                data = b''
            elif kind == stat.S_IFLNK:
                data = os.readlink(path).encode('utf-8', 'surrogateescape')
            elif kind == stat.S_IFREG:
                data = open(path, 'rb').read()
            else:
                sys.exit('%s: unsupported file type' % rel)
            e = dict(name=rel, mode=st.st_mode, uid=st.st_uid, gid=st.st_gid,
                     mtime=epoch, data=data, rdevmajor=0, rdevminor=0)
            b = base.get(rel)
            if b is not None:
                bkind = stat.S_IFMT(b['mode'])
                if (bkind == stat.S_IFDIR) != (kind == stat.S_IFDIR):
                    sys.exit('%s: would replace a %s of the base with a %s - add it to the remove list'
                             % (rel, 'directory' if bkind == stat.S_IFDIR else 'file',
                                'directory' if kind == stat.S_IFDIR else 'file'))
                if kind == stat.S_IFDIR:
                    continue                                        # rule 2
                if kind == stat.S_IFREG and bkind == stat.S_IFREG:  # rule 3
                    e.update(mode=b['mode'], uid=b['uid'], gid=b['gid'])
                replaced += 1
            else:
                added += 1
            base[rel] = e

    entries = [base[n] for n in sorted(base)]
    blob = xxri_cpio.write(entries)
    with open(out, 'wb') as f:
        with gzip.GzipFile(filename='', mode='wb', compresslevel=9, fileobj=f, mtime=0) as g:
            g.write(blob)
    print('core.gz: %d entries (%d removed from the base, %d replaced, %d added), %d bytes'
          % (len(entries), removed, replaced, added, os.path.getsize(out)))


if __name__ == '__main__':
    main()
