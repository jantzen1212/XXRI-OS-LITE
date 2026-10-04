#!/usr/bin/env python3
"""qemu-boot-test.py - boot an XXRI image through its REAL firmware and loader.

Never uses -kernel/-initrd: the firmware (SeaBIOS, 32-bit OVMF or 64-bit OVMF)
has to find the boot loader on the medium, and GRUB has to load vmlinuz and
core.gz itself, exactly as on hardware.

  qemu-boot-test.py --fw bios|ia32|x64 --media cd|hdd|usb --mem MiB \\
                    --img IMAGE [--disk EXTRA.img] [--edit-append TEXT] \\
                    --out DIR [--shots 60,90] [--timeout 120]

Evidence collected in DIR/result.json:
  grub_menu      seconds until the GRUB menu was on screen (None = never)
  userspace      seconds until the guest's own DHCP client (busybox udhcpc,
                 identified by its 'udhcp' vendor class) sent a request - the
                 NIC option ROMs are disabled, so firmware PXE cannot fake this
  ramdisk        'RAMDISK: [mem ...]' from the kernel log (only with
                 --edit-append 'console=ttyS0,115200 loglevel=7', which edits
                 the linux line in GRUB's own editor; the loader path is the same)
  shots          screenshots (PNG) with their mean brightness
Every drive is a throwaway overlay (snapshot=on) unless passed as --disk-rw.
"""
import argparse, json, os, re, shutil, socket, subprocess, sys, tempfile, time

OVMF32 = os.environ.get('XXRI_OVMF32', '/home/jantzen/.xxri-testenv/fw/deb/usr/share/OVMF')
FW = {
    'bios': ('qemu-system-i386', None, None),
    'ia32': ('qemu-system-i386', OVMF32 + '/OVMF32_CODE_4M.fd', OVMF32 + '/OVMF32_VARS_4M.fd'),
    'x64': ('qemu-system-x86_64', '/usr/share/edk2/x64/OVMF_CODE.4m.fd', '/usr/share/edk2/x64/OVMF_VARS.4m.fd'),
}
KEYS = {' ': 'spc', '=': 'equal', ',': 'comma', '.': 'dot', '/': 'slash', '-': 'minus',
        '_': 'shift-minus', ':': 'shift-semicolon', ';': 'semicolon', '(': 'shift-9',
        ')': 'shift-0', '$': 'shift-4', '"': 'shift-apostrophe', "'": 'apostrophe'}


class Monitor:
    def __init__(self, path):
        for _ in range(100):
            try:
                self.s = socket.socket(socket.AF_UNIX)
                self.s.connect(path)
                break
            except OSError:
                time.sleep(0.1)
        self.read()

    def read(self):
        buf = b''
        self.s.settimeout(10)
        while not buf.endswith(b'(qemu) '):
            chunk = self.s.recv(65536)
            if not chunk:
                break
            buf += chunk
        return buf.decode('latin1')

    def cmd(self, c):
        self.s.send((c + '\n').encode())
        return self.read()

    def key(self, k):
        self.cmd('sendkey ' + k)
        time.sleep(0.12)

    def type(self, text):
        for ch in text:
            k = KEYS.get(ch, ch)
            if ch.isupper():
                k = 'shift-' + ch.lower()
            self.key(k)

    def vga_text(self):
        out = self.cmd('xp /4000bx 0xb8000')
        vals = [int(v, 16) for v in re.findall(r'0x([0-9a-f]{2})\b', out.split(':', 1)[-1])]
        chars = ''.join(chr(b) if 32 <= b < 127 else ' ' for b in vals[0::2])
        return '\n'.join(chars[i:i + 80] for i in range(0, len(chars), 80))


def ppm_brightness(path):
    d = open(path, 'rb').read()
    i, fields = 0, []
    while len(fields) < 4:
        while d[i:i + 1].isspace():
            i += 1
        j = i
        while not d[j:j + 1].isspace():
            j += 1
        fields.append(d[i:j]); i = j
    px = d[i + 1:]
    step = max(1, len(px) // 300000)
    sample = px[::step]
    return round(sum(sample) / max(1, len(sample)), 1)


def dhcp_seen(pcap):
    try:
        d = open(pcap, 'rb').read()
    except OSError:
        return False
    off = 24
    while off + 16 <= len(d):
        incl = int.from_bytes(d[off + 8:off + 12], 'little')
        pkt = d[off + 16:off + 16 + incl]
        off += 16 + incl
        if len(pkt) > 42 and pkt[12:14] == b'\x08\x00' and pkt[23] == 17:
            ihl = (pkt[14] & 15) * 4
            dport = int.from_bytes(pkt[14 + ihl + 2:14 + ihl + 4], 'big')
            if dport == 67 and b'udhcp' in pkt:
                return True
    return False


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--fw', choices=FW, required=True)
    ap.add_argument('--media', choices=['cd', 'hdd', 'usb'], required=True)
    ap.add_argument('--mem', type=int, default=2048)
    ap.add_argument('--img', required=True)
    ap.add_argument('--disk', action='append', default=[])
    ap.add_argument('--edit-append')
    ap.add_argument('--grub-edit', help='JSON list of editor steps, e.g. [["down"],["type","x"]]')
    ap.add_argument('--disk-rw', action='append', default=[],
                    help='extra disk that is really written (install target)')
    ap.add_argument('--until-poweroff', action='store_true',
                    help='run until the guest powers off (or --timeout)')
    ap.add_argument('--out', required=True)
    ap.add_argument('--shots', default='75')
    ap.add_argument('--timeout', type=int, default=120)
    ap.add_argument('--no-kvm', action='store_true')
    ap.add_argument('--cpu', help='emulate this CPU model with TCG (e.g. pentium, pentium2, pentium3, '
                    '"pentium3,+sse2").  TCG traps CMOV on pentium and SSE on pentium2, but NOT SSE2 on '
                    'pentium3 and no model traps NOPL: pair it with --plugin tools/qa/isa-sentinel.so')
    ap.add_argument('--plugin', help='QEMU TCG plugin spec, e.g. isa-sentinel.so,cpu=pentium3,out=hits.tsv')
    ap.add_argument('--fw-code', help='override the OVMF code image (e.g. an older EDK2)')
    ap.add_argument('--fw-vars', help='override the OVMF variable store template')
    a = ap.parse_args()
    # XXRI_QA_ACCEL=tcg: no KVM - for a host where another hypervisor holds
    # VT-x (a running VirtualBox VM makes every KVM entry fail with "hardware
    # error 0x0").  XXRI_QA_TIME scales the timeout and the screenshot times,
    # since TCG boots the ISO to its desktop ~10x slower than KVM.
    if os.environ.get('XXRI_QA_ACCEL') == 'tcg':
        a.no_kvm = True
    k = float(os.environ.get('XXRI_QA_TIME', '1'))
    if k != 1:
        a.timeout = int(a.timeout * k)
        a.shots = ','.join(str(int(float(t) * k)) for t in a.shots.split(',') if t)

    os.makedirs(a.out, exist_ok=True)
    tmp = tempfile.mkdtemp(prefix='xq', dir='/tmp')
    mon, serial, pcap = tmp + '/m', a.out + '/serial.log', a.out + '/net.pcap'
    for f in (serial, pcap):
        if os.path.exists(f):
            os.unlink(f)
    qemu, code, vars_ = FW[a.fw]
    code, vars_ = a.fw_code or code, a.fw_vars or vars_
    cmd = [qemu, '-m', str(a.mem), '-machine', 'pc', '-vga', 'std',
           '-display', 'none', '-monitor', 'unix:%s,server,nowait' % mon,
           '-serial', 'file:' + serial,
           '-netdev', 'user,id=n0', '-device', 'e1000,netdev=n0,romfile=',
           '-object', 'filter-dump,id=f0,netdev=n0,file=' + pcap,
           '-device', 'qemu-xhci,id=xhci', '-device', 'usb-tablet,bus=xhci.0']
    if a.cpu:
        cmd += ['-accel', 'tcg', '-cpu', a.cpu]
        if a.plugin:
            cmd += ['-plugin', a.plugin]
    elif not a.no_kvm:
        cmd += ['-enable-kvm', '-cpu', 'host']
    if code:
        shutil.copy(vars_, tmp + '/vars.fd')
        cmd += ['-drive', 'if=pflash,format=raw,readonly=on,file=' + code,
                '-drive', 'if=pflash,format=raw,file=%s/vars.fd' % tmp]
    img = os.path.abspath(a.img)
    if a.media == 'cd':
        cmd += ['-drive', 'if=none,id=boot,media=cdrom,readonly=on,file=' + img,
                '-device', 'ide-cd,drive=boot,bus=ide.0,unit=0,bootindex=0']
    elif a.media == 'hdd':
        cmd += ['-drive', 'if=none,id=boot,format=raw,snapshot=on,file=' + img,
                '-device', 'ide-hd,drive=boot,bus=ide.0,unit=0,bootindex=0']
    else:
        cmd += ['-drive', 'if=none,id=boot,format=raw,snapshot=on,file=' + img,
                '-device', 'usb-storage,bus=xhci.0,drive=boot,bootindex=0']
    # Every drive is a throwaway overlay (snapshot=on) except --disk-rw ones.
    # The pc machine has two IDE buses with two units each; slot 0.0 belongs to
    # the boot medium unless it is on USB.
    disks = [(d, 'on') for d in a.disk] + [(d, 'off') for d in a.disk_rw]
    slots = [(b, u) for b in (0, 1) for u in (0, 1)][0 if a.media == 'usb' else 1:]
    if len(disks) > len(slots):
        sys.exit('too many disks for the IDE slots')
    for i, ((d, snap), (bus, unit)) in enumerate(zip(disks, slots)):
        cmd += ['-drive', 'if=none,id=d%d,format=raw,snapshot=%s,file=%s' % (i, snap, os.path.abspath(d)),
                # OVMF connects only devices named in QEMU's boot order, so
                # every extra disk gets a (lower-priority) bootindex too.
                '-device', 'ide-hd,drive=d%d,bus=ide.%d,unit=%d,bootindex=%d' % (i, bus, unit, i + 1)]

    res = dict(cpu=a.cpu or 'host', fw=a.fw, fw_code=os.path.basename(code or 'seabios'), media=a.media, mem=a.mem, img=os.path.basename(img),
               edit=a.edit_append, grub_menu=None, edited=False, userspace=None,
               ramdisk=None, shots=[], qemu_error=None)
    t0 = time.time()
    p = subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=open(a.out + '/qemu.err', 'w'))
    m = Monitor(mon)
    shots = sorted(int(s) for s in a.shots.split(',') if s)
    try:
        while time.time() - t0 < a.timeout and p.poll() is None:
            now = time.time() - t0
            if res['grub_menu'] is None:
                text = m.vga_text() if a.fw == 'bios' else open(serial, 'rb').read().decode('latin1', 'replace')
                if 'XXRI OS Lite' in text and ('Reboot' in text or 'GNU GRUB' in text):
                    res['grub_menu'] = round(now, 1)
                    if a.edit_append:
                        m.key('e'); time.sleep(1)
                        m.key('down'); m.key('down'); m.key('end')
                        m.type(' ' + a.edit_append)
                        m.key('ctrl-x')
                        res['edited'] = True
                    elif a.grub_edit:
                        m.key('e'); time.sleep(1)
                        for step in json.loads(a.grub_edit):
                            m.type(step[1]) if step[0] == 'type' else m.key(step[0])
                        m.key('ctrl-x')
                        res['edited'] = True
            if res['userspace'] is None and dhcp_seen(pcap):
                res['userspace'] = round(now, 1)
            if shots and now >= shots[0]:
                n = shots.pop(0)
                ppm = '%s/shot-%03d.ppm' % (tmp, n)
                png = '%s/shot-%03d.png' % (a.out, n)
                m.cmd('screendump ' + ppm)
                time.sleep(1)
                if os.path.exists(ppm):
                    b = ppm_brightness(ppm)
                    subprocess.run(['magick', ppm, png], stderr=subprocess.DEVNULL)
                    res['shots'].append(dict(t=n, png=os.path.basename(png), brightness=b))
            if not shots and res['userspace'] is not None and not a.until_poweroff:
                break
            time.sleep(0.5)
    finally:
        res['powered_off'] = p.poll() is not None
        p.kill()
        p.wait()
    log = open(serial, 'rb').read().decode('latin1', 'replace')
    r = re.search(r'RAMDISK: \[mem [^\]]+\]', log)
    res['ramdisk'] = r.group(0) if r else None
    for pat in ('Move RAMDISK[^\n]*', 'Kernel panic[^\n]*', 'rootfs image is not initramfs[^\n]*',
                'efi: EFI v[^\n]*', 'efi:[^\n]*mismatch[^\n]*', 'LOWMEM available[^\n]*',
                'HighMem[^\n]*|HIGHMEM available[^\n]*', 'Initramfs unpacking failed[^\n]*'):
        r = re.search(pat, log)
        if r:
            res.setdefault('kernel', []).append(r.group(0).strip())
    err = open(a.out + '/qemu.err').read()
    if 'entry failed' in err or 'KVM' in err:
        res['qemu_error'] = err.strip()[:300]
    res['elapsed'] = round(time.time() - t0, 1)
    json.dump(res, open(a.out + '/result.json', 'w'), indent=1)
    shutil.rmtree(tmp, ignore_errors=True)
    print(json.dumps(res))


if __name__ == '__main__':
    main()
