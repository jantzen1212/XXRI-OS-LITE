#!/usr/bin/env python3
"""xxri-mkucode - early microcode images for GRUB's initrd line.

  xxri-mkucode.py intel OUT.img FILE...   Intel's intel-ucode/FF-MM-SS files
  xxri-mkucode.py amd   OUT.img FILE...   linux-firmware's amd-ucode/*.bin

The kernel applies microcode on the boot CPU before anything else runs,
from an UNCOMPRESSED newc cpio at the start of the initrd holding
kernel/x86/microcode/GenuineIntel.bin or AuthenticAMD.bin (6.12:
arch/x86/kernel/cpu/microcode/core.c, find_microcode_in_initrd(); 32-bit
kernels map the initrd early for this, CONFIG_MICROCODE_INITRD32).  GRUB
concatenates the initrd files it is given, so an image goes in front of
core.gz on the same initrd line.

Intel: the update blobs of all files, each identical blob once (one blob
often covers several steppings and sits in several files), in file-name
order.  AMD: the per-family containers concatenated, as AMD ships them.
Timestamps come from SOURCE_DATE_EPOCH, owners are 0: the output depends on
the input files only.
"""
import hashlib
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import xxri_cpio  # noqa: E402


def intel_blobs(path):
    data = open(path, 'rb').read()
    off = 0
    while off < len(data):
        if len(data) - off < 48:
            raise SystemExit('%s: truncated header at %d' % (path, off))
        hdrver, _rev, _date, _sig, _ck, ldrver, _pf, datasize, totalsize = \
            struct.unpack_from('<9I', data, off)
        if hdrver != 1 or ldrver != 1:
            raise SystemExit('%s: not an Intel microcode update at %d' % (path, off))
        if datasize == 0:
            totalsize = 2048
        if totalsize < 48 or off + totalsize > len(data):
            raise SystemExit('%s: bad total size at %d' % (path, off))
        yield data[off:off + totalsize]
        off += totalsize


def intel(files):
    out, seen = [], set()
    for f in sorted(files, key=os.path.basename):
        for blob in intel_blobs(f):
            h = hashlib.sha256(blob).digest()
            if h not in seen:
                seen.add(h)
                out.append(blob)
    return 'GenuineIntel.bin', b''.join(out), len(out)


def amd(files):
    out = []
    for f in sorted(files, key=os.path.basename):
        data = open(f, 'rb').read()
        if data[:4] != b'DMA\0':
            raise SystemExit('%s: not an AMD microcode container' % f)
        out.append(data)
    return 'AuthenticAMD.bin', b''.join(out), len(out)


def main(argv):
    if len(argv) < 4 or argv[1] not in ('intel', 'amd'):
        sys.exit(__doc__)
    name, payload, n = (intel if argv[1] == 'intel' else amd)(argv[3:])
    mtime = int(os.environ.get('SOURCE_DATE_EPOCH', '0'))
    ent = lambda p, mode, data=b'': dict(name=p, mode=mode, uid=0, gid=0, mtime=mtime,
                                          data=data, rdevmajor=0, rdevminor=0)
    entries = [ent('kernel', 0o40755), ent('kernel/x86', 0o40755),
               ent('kernel/x86/microcode', 0o40755),
               ent('kernel/x86/microcode/' + name, 0o100644, payload)]
    with open(argv[2], 'wb') as o:
        o.write(xxri_cpio.write(entries))
    print('xxri-mkucode: %s, %d %s, %d bytes' % (
        os.path.basename(argv[2]), n, 'updates' if argv[1] == 'intel' else 'containers', len(payload)))


if __name__ == '__main__':
    main(sys.argv)
