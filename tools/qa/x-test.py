#!/usr/bin/env python3
"""x-test.py ISO OUTDIR [SCENARIO...] - display, input and acceleration paths (QA only).

Boots the ISO through its own firmware and GRUB with tools/qa/x-hook appended
as a second initrd (GRUB editor, like install-test.py), waits for the hook's
XQA_READY on the serial line, injects mouse movement, clicks and key presses
through the QEMU monitor, takes screenshots, and collects the hook's report
from the raw log disk:

  kms-bios, kms-ia32        -vga std (bochs-drm): XLibre Xorg + modesetting,
                            glamor, GLX, libinput - the preferred path
  fbdev-bios, fbdev-ia32    the same machine booted with `nomodeset`: no KMS
                            device, XLibre Xfbdev on vesafb / efifb
  vesa-bios                 no KMS device and no framebuffer at all (nomodeset,
                            GRUB in text mode, so no vesafb): Xorg + the vesa
                            driver through the video BIOS - the last fallback
  gpu-cirrus, gpu-vmware    other emulated adapters: cirrus DRM, vmwgfx (QEMU's
                            vmware-svga; the driver may refuse it -> fallback)
  gpu-rv100, gpu-rage128    ati-vga as Radeon 7000 (radeon KMS, but Mesa 25 has
                            no GL driver for R100: glamor must fall back) and as
                            Rage 128 Pro (no KMS driver: Xfbdev on vesafb)
  apps-kvm                  KMS path under KVM, every XXRI program started and
                            screenshotted in turn (the quick app check)
  cpu-pentium, cpu-pentium2, cpu-pentium3, cpu-sse2
                            TCG with that CPU model (cpu-sse2 = pentium3 + SSE2,
                            a Pentium 4-class feature set) and isa-sentinel:
                            desktop + every XXRI program started in turn
                            (`xxriqaapps`), each screenshotted; every executed
                            instruction the model lacks is recorded (hits.tsv,
                            map it with isa-sentinel-map.py).  Without SSE2,
                            Xorg must run without glamor/GLX (xxri-hw-init)

Writes OUTDIR/<scenario>/{xqa.log,shot-*.png,result.json} and OUTDIR/results.json.
"""
import importlib.util, json, os, re, shutil, subprocess, sys, tempfile, time

HERE = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location('bt', os.path.join(HERE, '..', 'qemu-boot-test.py'))
bt = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bt)

SCENARIOS = {
    'kms-bios':   dict(fw='bios', append='', cpu=None, timeout=420),
    'kms-ia32':   dict(fw='ia32', append='', cpu=None, timeout=420),
    'fbdev-bios': dict(fw='bios', append='nomodeset', cpu=None, timeout=420),
    'fbdev-ia32': dict(fw='ia32', append='nomodeset', cpu=None, timeout=420),
    'vesa-bios':   dict(fw='bios', append='nomodeset', grub='; set gfxpayload=text', cpu=None, timeout=420),
    'gpu-cirrus':  dict(fw='bios', append='', cpu=None, vga=['-vga', 'cirrus'], timeout=420),
    'gpu-vmware':  dict(fw='bios', append='', cpu=None, vga=['-vga', 'vmware'], timeout=420),
    'gpu-rv100':   dict(fw='bios', append='', cpu=None, vga=['-vga', 'none', '-device', 'ati-vga,model=rv100'], timeout=420),
    'gpu-rage128': dict(fw='bios', append='', cpu=None, vga=['-vga', 'none', '-device', 'ati-vga,model=rage128p'], timeout=420),
    'apps-kvm':    dict(fw='bios', append='xxriqaapps=25', cpu=None, timeout=900),
    'cpu-pentium':  dict(fw='bios', append='xxriqaapps=40', cpu='pentium', sentinel='pentium', timeout=3600),
    'cpu-pentium2': dict(fw='bios', append='xxriqaapps=40', cpu='pentium2', sentinel='pentium2', timeout=3600),
    'cpu-pentium3': dict(fw='bios', append='xxriqaapps=40', cpu='pentium3', sentinel='pentium3', timeout=3600),
    'cpu-sse2':     dict(fw='bios', append='xxriqaapps=60', cpu='pentium3,+sse2', sentinel='sse2', timeout=3600),
}


def make_hook(iso, out):
    """the hook initrd: the ISO's own /opt/bootlocal.sh plus one QA line"""
    w = tempfile.mkdtemp()
    subprocess.run(['xorriso', '-osirrox', 'on', '-indev', iso, '-extract', '/boot/core.gz', w + '/core.gz'],
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True)
    os.makedirs(w + '/root/opt')
    subprocess.run('zcat %s/core.gz | (cd %s/root && cpio -idm --quiet opt/bootlocal.sh)' % (w, w), shell=True)
    with open(w + '/root/opt/bootlocal.sh', 'a') as f:
        f.write('\n# QA ONLY (tools/qa/x-hook, never shipped)\n'
                'grep -q xxriqax /proc/cmdline && /opt/xxri-qa-x.sh >/dev/null 2>&1 &\n')
    shutil.copy(os.path.join(HERE, 'x-hook', 'opt', 'xxri-qa-x.sh'), w + '/root/opt/')
    os.chmod(w + '/root/opt/xxri-qa-x.sh', 0o755)
    subprocess.run('cd %s/root && find . | sort | cpio -o -H newc --quiet -R 0:0 | gzip -9 -n > %s/xxri-qa-x.gz'
                   % (w, w), shell=True, check=True)
    subprocess.run(['mkfs.fat', '-C', '-n', 'XXRIQAX', out, '4096'], stdout=subprocess.DEVNULL, check=True)
    subprocess.run(['mcopy', '-i', out, w + '/xxri-qa-x.gz', '::/'], check=True)
    shutil.rmtree(w)


def run(iso, hook, name, sc, d):
    os.makedirs(d, exist_ok=True)
    logd = d + '/log.img'
    with open(logd, 'wb') as f:
        f.truncate(2 * 1024 ** 2)
    qemu, code, vars_ = bt.FW[sc['fw']]
    tmp = tempfile.mkdtemp()
    mon, serial = tmp + '/mon', d + '/serial.log'
    cmd = [qemu, '-m', '1024', '-machine', 'pc'] + sc.get('vga', ['-vga', 'std']) + ['-display', 'none',
           '-monitor', 'unix:%s,server,nowait' % mon, '-serial', 'file:' + serial,
           '-netdev', 'user,id=n0', '-device', 'e1000,netdev=n0,romfile=']
    if sc['cpu']:
        cmd += ['-accel', 'tcg', '-cpu', sc['cpu'],
                '-plugin', '%s/isa-sentinel.so,cpu=%s,out=%s/hits.tsv' % (os.environ.get('XXRI_SENTINEL_DIR', HERE),
                                                                          sc['sentinel'], d)]
    else:
        cmd += ['-enable-kvm', '-cpu', 'host']
    if code:
        shutil.copy(vars_, tmp + '/vars.fd')
        cmd += ['-drive', 'if=pflash,format=raw,readonly=on,file=' + code,
                '-drive', 'if=pflash,format=raw,file=%s/vars.fd' % tmp]
    cmd += ['-drive', 'if=none,id=boot,media=cdrom,readonly=on,file=' + iso,
            '-device', 'ide-cd,drive=boot,bus=ide.0,unit=0,bootindex=0',
            '-drive', 'if=none,id=h,format=raw,snapshot=on,file=' + hook,
            '-device', 'ide-hd,drive=h,bus=ide.0,unit=1,bootindex=1',
            '-drive', 'if=none,id=l,format=raw,file=' + logd,
            '-device', 'ide-hd,drive=l,bus=ide.1,unit=0,bootindex=2']
    res = dict(scenario=name, fw=sc['fw'], append=sc['append'], cpu=sc['cpu'] or 'host',
               vga=' '.join(sc.get('vga', ['-vga', 'std'])),
               grub_menu=None, ready=None, done=None, shots=[])
    t0 = time.time()
    p = subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=open(d + '/qemu.err', 'w'))
    m = bt.Monitor(mon)
    injected = False
    try:
        while time.time() - t0 < sc['timeout'] and p.poll() is None:
            now = time.time() - t0
            if res['grub_menu'] is None:
                text = m.vga_text() if sc['fw'] == 'bios' else open(serial, 'rb').read().decode('latin1', 'replace')
                if 'XXRI OS Lite' in text and ('Reboot' in text or 'GNU GRUB' in text):
                    res['grub_menu'] = round(now, 1)
                    edit = [['down'], ['type', 'search --no-floppy --set=h --file /xxri-qa-x.gz' + sc.get('grub', '')],
                            ['down'], ['end'], ['type', ' xxriqax ' + sc['append'] +
                                                      ('' if sc['cpu'] or 'xxriqaapps' in sc['append'] else ' xxriqawait=240')],
                            ['down'], ['end'], ['type', ' ($h)/xxri-qa-x.gz']]
                    m.key('e'); time.sleep(1)
                    for step in edit:
                        m.type(step[1]) if step[0] == 'type' else m.key(step[0])
                    m.key('ctrl-x')
            ser = open(serial, 'rb').read().decode('latin1', 'replace')
            if not injected and 'XQA_READY' in ser:
                res['ready'] = round(now, 1)
                shot(m, d, tmp, 'desktop', res)
                # pointer: a walk over the empty desktop (25 packets - a server
                # that detects the mouse protocol consumes the first few, as a
                # real mouse sends dozens a second), then clicks; keys: 'a' x3
                for i in range(25):
                    dx, dy = ((40, 30), (60, -20), (-30, 40), (80, 10), (-50, -60))[i % 5]
                    m.cmd('mouse_move %d %d' % (dx // 2, dy // 2)); time.sleep(0.1)
                for _ in range(2):
                    m.cmd('mouse_button 1'); time.sleep(0.2); m.cmd('mouse_button 0'); time.sleep(0.4)
                for dz in (1, 1, -1, -1):   # wheel: buttons 4/5
                    m.cmd('mouse_move 0 0 %d' % dz); time.sleep(0.3)
                for _ in range(3):
                    m.key('a'); time.sleep(0.3)
                time.sleep(2)
                shot(m, d, tmp, 'after-input', res)
                injected = True
            for app in re.findall(r'XQA_APP (\S+)', ser):
                if app not in res.setdefault('app_shots', []):
                    res['app_shots'].append(app)
                    shot(m, d, tmp, 'app-' + app, res)
            if 'XQA_DONE' in ser and res['done'] is None:
                res['done'] = round(now, 1)
            if res['done'] is None and not res['shots'] and now > sc['timeout'] - 60 and not res.get('late_shot'):
                res['late_shot'] = True
                shot(m, d, tmp, 'late', res)
            time.sleep(1)
    finally:
        res['powered_off'] = p.poll() is not None
        p.kill(); p.wait()
    rep = open(logd, 'rb').read().split(b'\0')[0].decode('latin1', 'replace')
    open(d + '/xqa.log', 'w').write(rep)
    res.update(parse(rep))
    res['elapsed'] = round(time.time() - t0, 1)
    shutil.rmtree(tmp, ignore_errors=True)
    json.dump(res, open(d + '/result.json', 'w'), indent=1)
    return res


def shot(m, d, tmp, tag, res):
    ppm = '%s/%s.ppm' % (tmp, tag)
    m.cmd('screendump ' + ppm)
    time.sleep(1)
    if os.path.exists(ppm):
        subprocess.run(['magick', ppm, '%s/shot-%s.png' % (d, tag)], stderr=subprocess.DEVNULL)
        res['shots'].append(dict(tag=tag, brightness=bt.ppm_brightness(ppm)))


def parse(rep):
    r = {}
    dsk = re.search(r'^XQA desktop=(\w+)', rep, re.M)
    r['desktop'] = dsk.group(1) if dsk else None
    s = re.search(r'^SERVER=(\S+) pid=\S+ args=(.*?) rss_kb=(\d*)', rep, re.M)
    r['server'] = s.group(1) if s else None
    r['server_rss_kb'] = int(s.group(3)) if s and s.group(3) else None
    ext = re.search(r'number of extensions:\s*(\d+)\n((?:\s{4}\S.*\n)+)', rep)
    r['extensions'] = [e.strip() for e in ext.group(2).splitlines()] if ext else []
    r['kernel_input'] = {m.group(1): int(m.group(2)) for m in re.finditer(r'^KERNEL (\S+) bytes=(\d+)', rep, re.M)}
    for k in ('XI_MOTION', 'XI_BUTTONPRESS', 'XI_WHEEL', 'XI_KEYPRESS', 'XI_KEY_A', 'XI_RAW', 'XI_RAWMOTION'):
        v = re.search(r'^%s=(\d+)' % k, rep, re.M)
        r[k.lower()] = int(v.group(1)) if v else None
    # "glamor initialization failed" must not count: modesetting refuses glamor
    # on Mesa's software rasteriser (llvmpipe) and falls back to plain fb
    r['glamor'] = bool(re.search(r'glamor initialized\b', rep)) and \
        not re.search(r'glamor initialization failed|AccelMethod.*none|Failed to initialize glamor', rep, re.I)
    r['glamor_llvmpipe_refused'] = 'Refusing to try glamor on llvmpipe' in rep
    r['vesa'] = bool(re.search(r'VESA\(0\): (Total Memory|Setting up VESA Mode|VESA VBE Version)', rep))
    r['accel_none'] = bool(re.search(r'AccelMethod.*"?none', rep, re.I))
    r['glx'] = 'GLX' in r['extensions']
    r['xinput2'] = 'XInputExtension' in r['extensions']
    r['libinput'] = bool(re.search(r'libinput', rep)) and r['server'] == 'Xorg'
    r['errors'] = re.findall(r'^.*\(EE\).*$', rep, re.M)[:10]
    r['apps'] = {m.group(1): m.group(2) for m in re.finditer(r'^APP (\S+) (\S+)', rep, re.M)}
    return r


def main():
    iso, outdir = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2])
    names = sys.argv[3:] or list(SCENARIOS)
    os.makedirs(outdir, exist_ok=True)
    hook = os.path.join(outdir, 'x-hook.img')
    make_hook(iso, hook)
    results = {}
    for n in names:
        results[n] = run(iso, hook, n, SCENARIOS[n], os.path.join(outdir, n))
        print(n, json.dumps({k: results[n][k] for k in ('desktop', 'server', 'ready', 'glamor', 'glamor_llvmpipe_refused', 'vesa', 'accel_none', 'glx',
              'xinput2', 'xi_motion', 'xi_buttonpress', 'xi_wheel', 'xi_key_a', 'server_rss_kb')}), flush=True)
    json.dump(results, open(os.path.join(outdir, 'results.json'), 'w'), indent=1)


if __name__ == '__main__':
    main()
