#!/usr/bin/env python3
"""rootfs-meta.py - keep the rootfs metadata that git cannot store.

git records file content, symlinks and a single executable bit.  It does not
record empty directories, setuid/setgid/sticky bits, group/other permission
bits or mtimes - and core.gz needs all of them: a core.gz packed from a clean
checkout has no /proc, /sys, /dev/pts, /home, /mnt or /run, a busybox.suid and
sudo without setuid, and a /tmp without the sticky bit.

  capture ROOTFS > rootfs.meta     record every entry of an authoritative tree
  apply   STAGE  rootfs.meta       make a staging copy match the record
  check   ROOTFS rootfs.meta       list entries missing from the record

rootfs.meta lines:  <type> <octal mode> <mtime> <path>   (type d/f/l)
Entries not in the record (files added since the capture) keep their content,
get mode 0755 for directories / their git mode for files, and the mtime
SOURCE_DATE_EPOCH, so the result is still deterministic.
"""
import os, stat, sys

SKIP = {'./.kilo', './.git'}


def walk(root):
    for top, dirs, files in os.walk(root):
        rel_top = '.' + top[len(root):]
        dirs[:] = sorted(d for d in dirs if os.path.join(rel_top, d) not in SKIP)
        for name in dirs + sorted(files):
            yield os.path.join(rel_top, name)


def capture(root):
    root = os.path.abspath(root)
    for rel in ['.'] + list(walk(root)):
        st = os.lstat(os.path.join(root, rel))
        t = 'l' if stat.S_ISLNK(st.st_mode) else 'd' if stat.S_ISDIR(st.st_mode) else 'f'
        if t == 'f' and not stat.S_ISREG(st.st_mode):
            sys.exit('special file not supported: %s' % rel)
        print('%s %04o %d %s' % (t, stat.S_IMODE(st.st_mode), int(st.st_mtime), rel))


def load(meta):
    rec = {}
    for line in open(meta):
        t, mode, mtime, path = line.rstrip('\n').split(' ', 3)
        rec[path] = (t, int(mode, 8), int(mtime))
    return rec


def apply(stage, meta):
    stage = os.path.abspath(stage)
    rec = load(meta)
    epoch = int(os.environ.get('SOURCE_DATE_EPOCH', '0'))
    made = 0
    for path, (t, mode, mtime) in sorted(rec.items()):
        full = os.path.join(stage, path)
        if not os.path.lexists(full):
            if t != 'd':
                continue          # deleted since the capture: nothing to do
            os.makedirs(full)
            made += 1
    extra = []
    # deepest first, so setting a directory's mtime is not undone by its children
    for path in sorted(walk(stage), key=lambda p: -p.count('/')):  # '.' handled below
        full = os.path.join(stage, path)
        st = os.lstat(full)
        if path in rec:
            t, mode, mtime = rec[path]
        else:
            extra.append(path)
            mode = 0o755 if stat.S_ISDIR(st.st_mode) else stat.S_IMODE(st.st_mode)
            mtime = epoch
        if not stat.S_ISLNK(st.st_mode):
            os.chmod(full, mode)
        os.utime(full, (mtime, mtime), follow_symlinks=False)
    if '.' in rec:
        os.chmod(stage, rec['.'][1])
        os.utime(stage, (rec['.'][2], rec['.'][2]))
    else:
        os.utime(stage, (epoch, epoch))
    print('rootfs-meta: %d entries recorded, %d empty dirs restored, %d not in record'
          % (len(rec), made, len(extra)), file=sys.stderr)
    for p in extra[:20]:
        print('rootfs-meta:   not in record (SOURCE_DATE_EPOCH): %s' % p, file=sys.stderr)


def check(root, meta):
    rec = load(meta)
    missing = [p for p in walk(os.path.abspath(root)) if p not in rec]
    for p in missing:
        print(p)
    return 1 if missing else 0


if __name__ == '__main__':
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    cmd = sys.argv[1]
    if cmd == 'capture':
        capture(sys.argv[2])
    elif cmd == 'apply':
        apply(sys.argv[2], sys.argv[3])
    elif cmd == 'check':
        sys.exit(check(sys.argv[2], sys.argv[3]))
    else:
        sys.exit(__doc__)
