#!/usr/bin/env python3
"""Build xdiag.gz: the initrd-handoff checker that loads ahead of core.gz.

usage: mkxdiag.py CORE_GZ XD_INIT_SH OUT_GZ LABEL

Takes busybox + its libraries out of core.gz itself (so they are the same
i686 binaries XXRI ships), repoints their ELF interpreter to /xd, embeds
per-64KB and per-2KB md5 lists of CORE_GZ, and pads the result with zeros
to a 64 KB multiple so core.gz starts on a known, sector-aligned offset.
"""
import gzip, hashlib, os, stat, sys, zlib

core_path, init_path, out_path, label = sys.argv[1:5]
extra = sys.argv[5:]          # optional: xcollect.sh, xsession.fixed
core = open(core_path, 'rb').read()
raw = zlib.decompressobj(31).decompress(core)

# --- parse core.gz (newc cpio) ---
files, order, off = {}, [], 0
while off < len(raw):
    h = raw[off:off + 110]
    assert h[:6] == b'070701', 'not newc cpio'
    f = lambda a, b: int(h[a:b], 16)
    mode, fsize, nsize = f(14, 22), f(54, 62), f(94, 102)
    name = raw[off + 110:off + 110 + nsize - 1].decode()
    doff = (off + 110 + nsize + 3) & ~3
    files[name] = (mode, raw[doff:doff + fsize])
    order.append((name, mode, off))
    off = (doff + fsize + 3) & ~3
    if name == 'TRAILER!!!':
        break

def resolve(name):
    for _ in range(10):
        mode, data = files[name]
        if stat.S_ISLNK(mode):
            t = data.decode()
            name = os.path.normpath(os.path.join(os.path.dirname(name), t)).lstrip('/') if not t.startswith('/') else t.lstrip('/')
        else:
            return data
    raise SystemExit('symlink loop: ' + name)

OLD, NEW = b'/lib/ld-linux.so.2\x00', b'/xd/ld-linux.so.2\x00\x00'
def repoint(elf):
    assert elf.count(OLD) == 1, 'interpreter string not unique'
    return elf.replace(OLD, NEW)

payload = {
    'xd/ld-linux.so.2': resolve('lib/ld-linux.so.2'),
    'xd/libc.so.6': resolve('lib/libc.so.6'),
    'xd/libm.so.6': resolve('lib/libm.so.6'),
    'xd/libcrypt.so.1': resolve('usr/lib/libcrypt.so.1'),
    'xd/busybox': repoint(resolve('bin/busybox')),
    'xd/busybox.suid': repoint(resolve('bin/busybox.suid')),
    'xd/init': open(init_path, 'rb').read(),
}
for p in extra:
    payload['xd/' + ('xcollect.sh' if 'xcollect' in p else 'xsession.fixed')] = open(p, 'rb').read()
applets = ('sh awk cat clear cut dd dmesg echo grep head md5sum more od printf '
           'reboot sed sleep uname wc ls').split()
suid_applets = ('mount', 'umount')

# --- expected hashes of core.gz ---
md5 = lambda b: hashlib.md5(b).hexdigest()
c64 = [md5(core[i:i + 65536]) for i in range(0, len(core), 65536)]
s2k = [md5(core[i:i + 2048]) for i in range(0, len(core), 2048)]
regs = [(n, o) for n, m, o in order if stat.S_ISREG(m) and not n.startswith('xd/')]
markers = []
for pct in (5, 10, 25, 50, 75, 90):
    markers.append((pct, min(regs, key=lambda x: abs(x[1] - pct / 100 * len(raw)))[0]))
markers += [(98, 'init'), (100, 'bin/busybox')]

payload['xd/c64.md5'] = ('\n'.join(c64) + '\n').encode()
payload['xd/s2k.md5'] = ('\n'.join(s2k) + '\n').encode()
payload['xd/markers.txt'] = ''.join('%d %s\n' % m for m in markers).encode()

# --- newc cpio writer ---
ino = [1000]
def entry(name, mode, data=b'', rdev=(0, 0)):
    ino[0] += 1
    nb = name.encode() + b'\0'
    hdr = '070701' + ''.join('%08X' % v for v in (
        ino[0], mode, 0, 0, 1, 0, len(data), 0, 0, rdev[0], rdev[1], len(nb), 0))
    out = hdr.encode() + nb
    out += b'\0' * ((-len(out)) % 4) + data
    return out + b'\0' * ((-len(out)) % 4)

def build(expected_sh):
    b = b''
    for d in ('dev', 'proc', 'sys', 'xd'):
        b += entry(d, stat.S_IFDIR | 0o755)
    b += entry('dev/console', stat.S_IFCHR | 0o600, rdev=(5, 1))
    b += entry('dev/null', stat.S_IFCHR | 0o666, rdev=(1, 3))
    for n, data in payload.items():
        b += entry(n, stat.S_IFREG | 0o755, data)
    for a in applets:
        b += entry('xd/' + a, stat.S_IFLNK | 0o777, b'busybox')
    for a in suid_applets:
        b += entry('xd/' + a, stat.S_IFLNK | 0o777, b'busybox.suid')
    b += entry('xd/expected.sh', stat.S_IFREG | 0o644, expected_sh.encode())
    b += entry('TRAILER!!!', 0)
    return gzip.compress(b, 9, mtime=0)

def expected(core_off):
    return '\n'.join([
        'LABEL="%s"' % label,
        'CORE_OFF=%d' % core_off, 'CORE_LEN=%d' % len(core),
        'CHUNKS=%d' % len(c64), 'LASTLEN=%d' % (len(core) - 65536 * (len(c64) - 1)),
        'SECTORS=%d' % len(s2k), 'LASTSEC=%d' % (len(core) - 2048 * (len(s2k) - 1)),
        'HEAD16=%s' % md5(core[:16]), 'Z2K=%s' % md5(b'\0' * 2048),
        'F2K=%s' % md5(b'\xff' * 2048), 'E0=%s' % md5(b''), ''])

# CORE_OFF depends on the compressed size, which depends on CORE_OFF: iterate.
core_off = 65536
for _ in range(5):
    gz = build(expected(core_off))
    need = -(-len(gz) // 65536) * 65536
    if need == core_off:
        break
    core_off = need
gz = build(expected(core_off))
assert len(gz) <= core_off
open(out_path, 'wb').write(gz + b'\0' * (core_off - len(gz)))
print('%s: %d bytes compressed, padded to CORE_OFF=%d; %d blocks, %d sectors, markers %s'
      % (out_path, len(gz), core_off, len(c64), len(s2k), markers))
