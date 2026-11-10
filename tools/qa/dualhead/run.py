#!/usr/bin/env python3
"""dualqa/run.py ISO OUTDIR [--overlay FILE=DEST ...] [--arch bios64|bios] - QA only.

Boots the ISO (SeaBIOS, GRUB editor as in tools/qa/x-test.py) with a second
initrd carrying dualqa/xxri-qa-dual.sh (AQD_HOOK overrides), which waits for
the desktop and runs a Store scenario as the desktop user. Optional overlay
files (SRC:DEST[:MODE]) put the change under test over the ISO's own files.
The hook's report comes back on the raw 2 MiB log disk; screenshots are taken
whenever the guest prints AQD_SHOT <name> on the serial line.
"""
import importlib.util, json, os, re, shutil, subprocess, sys, tempfile, time

E = os.path.expanduser('~/.xxri-testenv')   # qemu-mods/ lives here (see README)
D = E + '/dualqa'
REPO = os.path.expanduser('~/xxri-wt/ptx')
spec = importlib.util.spec_from_file_location('bt', REPO + '/tools/qemu-boot-test.py')
bt = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bt)


def make_hook(iso, out, overlay):
    w = tempfile.mkdtemp()
    subprocess.run(['xorriso', '-osirrox', 'on', '-indev', iso, '-extract', '/boot/core.gz', w + '/core.gz'],
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True)
    r = w + '/root'
    os.makedirs(r + '/opt')
    subprocess.run('zcat %s/core.gz | (cd %s && cpio -idm --quiet opt/bootlocal.sh)' % (w, r), shell=True)
    with open(r + '/opt/bootlocal.sh', 'a') as f:
        f.write('\n# QA ONLY (dualqa, never shipped)\n'
                'grep -q xxriqad /proc/cmdline && /opt/xxri-qa-dual.sh >/dev/null 2>&1 &\n')
    files = {'opt/xxri-qa-dual.sh': (os.environ.get('AQD_HOOK', D + '/xxri-qa-dual.sh'), 0o755)}
    for spec_ in overlay:
        parts = spec_.split(':')
        files[parts[1].lstrip('/')] = (parts[0], int(parts[2], 8) if len(parts) > 2 else 0o755)
    for dst, (src, mode) in files.items():
        os.makedirs(os.path.dirname(r + '/' + dst), exist_ok=True)
        shutil.copy(src, r + '/' + dst)
        os.chmod(r + '/' + dst, mode)
    subprocess.run('cd %s && { echo opt/aqa; find . -type f | sed "s|^\\./||" | sort; } | '
                   'cpio -o -H newc --quiet -R 0:0 | gzip -9 -n > %s/xxri-qa-d.gz' % (r, w), shell=True, check=True)
    subprocess.run(['mkfs.fat', '-C', '-n', 'XXRIQAD', out, '8192'], stdout=subprocess.DEVNULL, check=True)
    subprocess.run(['mcopy', '-i', out, w + '/xxri-qa-d.gz', '::/'], check=True)
    shutil.rmtree(w)


def main():
    a = sys.argv[1:]
    iso, outd = a[0], a[1]
    overlay = [a[i + 1] for i, x in enumerate(a) if x == '--overlay']
    fw = a[a.index('--arch') + 1] if '--arch' in a else 'bios64'
    qemu = bt.FW[fw][0]
    os.makedirs(outd, exist_ok=True)
    hook = outd + '/hook.img'
    make_hook(iso, hook, overlay)
    logd = outd + '/log.img'
    with open(logd, 'wb') as f:
        f.truncate(2 * 1024 ** 2)
    shots_img = outd + '/shots.img'
    with open(shots_img, 'wb') as f:
        f.truncate(64 * 1024 ** 2)
    tmp = tempfile.mkdtemp()
    mon, serial = tmp + '/mon', outd + '/serial.log'
    tcg = os.environ.get('XXRI_QA_ACCEL') == 'tcg'
    accel = ['-accel', 'tcg'] if tcg else ['-enable-kvm', '-cpu', 'host']
    os.environ['QEMU_MODULE_DIR'] = E + '/qemu-mods/usr/lib/qemu'
    bus = subprocess.run(['dbus-daemon', '--session', '--fork', '--print-address=1', '--print-pid=2'],
                         capture_output=True, text=True)
    addr = bus.stdout.strip(); buspid = int(bus.stderr.strip().split()[-1])
    os.environ['DBUS_SESSION_BUS_ADDRESS'] = addr
    cmd = ['choom', '-n', '1000', '--', qemu, '-m', os.environ.get('AQD_MEM', '2048'), '-machine', 'pc'] + accel + [
        '-vga', 'none', '-device', os.environ.get('AQD_GPU', 'virtio-vga,id=vga0,max_outputs=2,xres=1366,yres=768'), '-display', 'dbus', '-monitor', 'unix:%s,server,nowait' % mon, '-serial', 'file:' + serial,
        '-netdev', 'user,id=n0', '-device', 'e1000,netdev=n0,romfile=',
        '-drive', 'if=none,id=boot,media=cdrom,readonly=on,file=' + iso,
        '-device', 'ide-cd,drive=boot,bus=ide.0,unit=0,bootindex=0',
        '-drive', 'if=none,id=h,format=raw,snapshot=on,file=' + hook,
        '-device', 'ide-hd,drive=h,bus=ide.0,unit=1,bootindex=1',
        '-drive', 'if=none,id=l,format=raw,file=' + logd,
        '-device', 'ide-hd,drive=l,bus=ide.1,unit=0,bootindex=2',
        '-drive', 'if=none,id=x,format=raw,file=' + shots_img, '-device', 'ide-hd,drive=x,bus=ide.1,unit=1']
    timeout = int(os.environ.get('AQD_TIMEOUT', '1500'))
    t0 = time.time()
    p = subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=open(outd + '/qemu.err', 'w'))
    m = bt.Monitor(mon)
    if os.environ.get('AQD_EARLY'):   # the second display is plugged in before the machine boots
        for _ in range(50):
            r = subprocess.run(['busctl', '--address', addr, 'call', 'org.qemu', '/org/qemu/Display1/Console_1',
                                'org.qemu.Display1.Console', 'SetUIInfo', 'qqiiuu', '0', '0', '0', '0',
                                os.environ.get('AQD_W1', '1920'), os.environ.get('AQD_H1', '1080')], capture_output=True, text=True)
            if r.returncode == 0: break
            time.sleep(0.2)
        print('early SetUIInfo rc=%d' % r.returncode, flush=True)
    grub = None
    seen = set()
    try:
        while time.time() - t0 < timeout and p.poll() is None:
            now = time.time() - t0
            if grub is None:
                text = m.vga_text()
                if 'XXRI OS Lite' in text and ('Reboot' in text or 'GNU GRUB' in text):
                    grub = now
                    print('grub menu at %.0fs' % now, flush=True)
                    edit = [['down'], ['type', 'search --no-floppy --set=h --file /xxri-qa-d.gz'],
                            ['down'], ['end'], ['type', ' xxriqad'],
                            ['down'], ['end'], ['type', ' ($h)/xxri-qa-d.gz']]
                    m.key('e'); time.sleep(1)
                    for step in edit:
                        m.type(step[1]) if step[0] == 'type' else m.key(step[0])
                    m.key('ctrl-x')
            ser = open(serial, 'rb').read().decode('latin1', 'replace')
            for line in ser.splitlines():
                if line in seen or not line.startswith('AQD_'):
                    continue
                seen.add(line)
                print('%s at %.0fs' % (line, now), flush=True)
                if line.startswith('AQD_UI '):
                    _, con, w, h = line.split()
                    w = os.environ.get('AQD_W%s' % con, w); h = os.environ.get('AQD_H%s' % con, h)
                    r = subprocess.run(['busctl', '--address', addr, 'call', 'org.qemu', '/org/qemu/Display1/Console_' + con,
                                        'org.qemu.Display1.Console', 'SetUIInfo', 'qqiiuu', '0', '0', '0', '0', w, h],
                                       capture_output=True, text=True)
                    print('SetUIInfo console %s %sx%s: rc=%d %s' % (con, w, h, r.returncode, (r.stdout + r.stderr).strip()[:150]), flush=True)
                if line.startswith('AQD_SHOT '):
                    for head in (0, 1):
                        ppm = '%s/s%d.ppm' % (tmp, head)
                        r = m.cmd('screendump %s vga0 %d' % (ppm, head))
                        time.sleep(1)
                        try:
                            m.s.settimeout(1); r += m.s.recv(65536).decode('latin1')
                        except Exception:
                            pass
                        r = re.sub(r'\x1b\[[0-9;]*[A-Za-z]', '', r)
                        if os.path.exists(ppm):
                            subprocess.run(['magick', ppm, '%s/shot-%s-head%d.png' % (outd, line.split()[1], head)])
                            os.unlink(ppm)
                        else:
                            print('no screendump for head %d: %s' % (head, r.strip()[-200:]), flush=True)
            if 'AQD_DONE' in ser:
                time.sleep(6)
                break
            time.sleep(0.25)
    finally:
        p.kill(); p.wait()
    # guest-side screenshots: slot N (12 MiB each) of shots.img, named by AQD_XSHOT lines
    for line in sorted(seen):
        if line.startswith('AQD_XSHOT '):
            _, n, name = line.split()
            raw = open(shots_img, 'rb').read()[int(n) * 12 * 1024 ** 2:(int(n) + 1) * 12 * 1024 ** 2]
            if raw[:2] == b'P6':
                hdr = raw.split(b'\n', 3); w, h = map(int, hdr[1].split())
                body = hdr[3][:w * h * 3]
                open('%s/x-%s.ppm' % (outd, name), 'wb').write(b'P6\n%d %d\n255\n' % (w, h) + body)
                subprocess.run(['magick', '%s/x-%s.ppm' % (outd, name), '%s/x-%s.png' % (outd, name)])
                os.unlink('%s/x-%s.ppm' % (outd, name))
    rep = open(logd, 'rb').read().split(b'\0')[0].decode('latin1', 'replace')
    open(outd + '/aqd.log', 'w').write(rep)
    print('done after %.0fs; report %s/aqd.log' % (time.time() - t0, outd), flush=True)
    shutil.rmtree(tmp, ignore_errors=True)
    os.kill(buspid, 15)


if __name__ == '__main__':
    main()
