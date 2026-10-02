#!/usr/bin/env python3
"""update-test.py ISO OUTDIR - Settings > Software Update end to end (QA only).

  1. installs the ISO onto a disk (install-test.py bios-auto-mbr, reused if
     OUTDIR/inst already holds one);
  2. makes update 2.6.1 with tools/update/mkupdate.sh: a new file, a replaced
     file (/etc/motd), a removal, and the ISO's own vmlinuz + core.gz as
     system files - so the kernel/initrd path, the *.prev copies and GRUB's
     "previous system" entry all run;
  3. serves manifest + payload over HTTP from the host (10.0.2.2 in the guest);
  4. writes a one-shot hook into the installed system with debugfs: boot 1
     installs the update as root through xxri-system-update and reboots;
     boot 2 (the updated system) opens Settings > Software Update, which the
     host screenshots, and powers off;
  5. reads the report and the resulting files back from the disk.
Also a tampered payload: the SHA-256 check must refuse it and change nothing.
"""
import importlib.util, json, os, re, shutil, struct, subprocess, sys, tempfile, time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))
spec = importlib.util.spec_from_file_location('bt', os.path.join(HERE, '..', 'qemu-boot-test.py'))
bt = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bt)
PORT = 8781

BOOTHOOK = r'''#!/bin/sh
# QA ONLY (tools/qa/update-test.py) - never shipped
say() { echo "$*" > /dev/ttyS0; }
L=/var/log/xxri-qa-update.log
export XXRI_UPDATE_MANIFEST=http://10.0.2.2:%(port)d/manifest.json
i=0; while [ $i -lt 120 ] && ! route -n | grep -q '^0.0.0.0'; do sleep 1; i=$((i + 1)); done
if [ ! -f /var/lib/xxri-qa-update.boot2 ]; then
	say "UPDQA_BOOT1"
	{
		echo "--- before"; grep -E '^(IMAGE_VERSION|PRETTY_NAME|BUILD_ID)=' /etc/os-release; ls -l /boot
		echo "--- tampered"; XXRI_UPDATE_MANIFEST=http://10.0.2.2:%(port)d/tampered.json xxri-system-update install; echo "rc=$?"
		grep -E '^IMAGE_VERSION=' /etc/os-release; ls /usr/local/share/xxri-update-test 2>&1
		echo "--- check"; xxri-system-update check --json
		echo "--- install"; xxri-system-update install --json; echo "rc=$?"
		echo "--- progress"; xxri-system-update progress --json
		echo "--- after"; grep -E '^(IMAGE_VERSION|PRETTY_NAME|BUILD_ID|VERSION)=' /etc/os-release; ls -l /boot
		cat /usr/local/share/xxri-update-test/hello.txt; tail -1 /etc/motd
		ls /usr/local/share/doc/xxri-firmware-intel/ 2>&1
		echo "--- grub"; cat /boot/grub/grub.cfg
		echo "--- history"; cat /usr/local/etc/xxri/update-history
		echo "--- backup"; find /var/lib/xxri-update -type f
		echo "--- update.log"; cat /var/cache/xxri-update/update.log
	} > $L 2>&1
	touch /var/lib/xxri-qa-update.boot2
	sync; say "UPDQA_REBOOT"; sleep 2; reboot
else
	say "UPDQA_BOOT2"
	{ echo "--- boot 2"; cat /proc/cmdline; uname -r; grep -E '^(IMAGE_VERSION|PRETTY_NAME)=' /etc/os-release
	  xxri-system-update history --json; xxri-system-update check --json; } >> $L 2>&1
fi
''' % {'port': PORT}

SESSIONHOOK = r'''# QA ONLY (tools/qa/update-test.py) - never shipped
if [ -f /var/lib/xxri-qa-update.boot2 ]; then
	( sleep 20
	  XXRI_UPDATE_MANIFEST=http://10.0.2.2:%(port)d/manifest.json xxri-settings update &
	  sleep 25; sudo sh -c 'echo UPDQA_SHOT > /dev/ttyS0'
	  sleep 8; sudo sh -c 'echo UPDQA_DONE > /dev/ttyS0'; sleep 2; sudo poweroff ) >/dev/null 2>&1 &
fi
''' % {'port': PORT}


def part_offset(img):
    with open(img, 'rb') as f:
        mbr = f.read(512)
    start = struct.unpack('<I', mbr[446 + 8:446 + 12])[0]
    return start * 512


def dfs(img, off, cmds, write=False):
    args = ['debugfs'] + (['-w'] if write else []) + ['-f', '-', '%s?offset=%d' % (img, off)]
    return subprocess.run(args, input='\n'.join(cmds) + '\n', capture_output=True, text=True).stdout


def main():
    iso, out = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2])
    os.makedirs(out, exist_ok=True)
    pristine = os.path.join(out, 'inst', 'bios-auto-mbr', 'target.img')
    if not os.path.exists(pristine):
        subprocess.run([sys.executable, os.path.join(HERE, 'install-test.py'), iso, os.path.join(out, 'inst'),
                        'bios-auto-mbr'], check=True)
    img = os.path.join(out, 'target.img')
    subprocess.run(['cp', '--sparse=always', pristine, img], check=True)
    off = part_offset(img)
    w = tempfile.mkdtemp()
    # 2. the update
    tree = os.path.join(w, 'tree')
    os.makedirs(tree + '/files/usr/local/share/xxri-update-test')
    os.makedirs(tree + '/files/etc')
    os.makedirs(tree + '/boot')
    open(tree + '/files/usr/local/share/xxri-update-test/hello.txt', 'w').write('hello from XXRI OS Lite 2.6.1\n')
    dfs(img, off, ['dump /etc/motd %s/files/etc/motd' % tree])
    with open(tree + '/files/etc/motd', 'a') as f:
        f.write('updated to 2.6.1 by xxri-system-update\n')
    open(tree + '/remove.list', 'w').write('/usr/local/share/doc/xxri-firmware-intel/intel.list\n')
    for n in ('vmlinuz', 'core.gz'):
        subprocess.run(['xorriso', '-osirrox', 'on', '-indev', iso, '-extract', '/boot/' + n, tree + '/boot/' + n],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True)
    open(w + '/notes.txt', 'w').write('QA update: a new file, a replaced file, a removal, kernel + initrd.\n')
    srv = os.path.join(out, 'srv')
    shutil.rmtree(srv, ignore_errors=True)
    subprocess.run([os.path.join(ROOT, 'tools', 'update', 'mkupdate.sh'), '-v', '2.6.1', '-t', tree,
                    '-u', 'http://10.0.2.2:%d' % PORT, '-o', srv, '-n', w + '/notes.txt', '-m', '2.6',
                    '-b', 'qa', '-r', 'reboot'], check=True, env=dict(os.environ, SOURCE_DATE_EPOCH='1788220800'))
    # the same manifest pointing at a payload with one byte changed
    m = json.load(open(srv + '/manifest.json'))
    shutil.copy(srv + '/xxri-update-2.6.1.tar.gz', srv + '/tampered.tar.gz')
    with open(srv + '/tampered.tar.gz', 'r+b') as f:
        f.seek(100); b = f.read(1); f.seek(100); f.write(bytes([b[0] ^ 0xff]))
    m['payload_url'] = 'http://10.0.2.2:%d/tampered.tar.gz' % PORT
    json.dump(m, open(srv + '/tampered.json', 'w'))
    # 4. the hook, written into the installed system
    open(w + '/hook.sh', 'w').write(BOOTHOOK)
    open(w + '/sess', 'w').write(SESSIONHOOK)
    dfs(img, off, ['dump /opt/bootlocal.sh %s/bootlocal.sh' % w])
    with open(w + '/bootlocal.sh', 'a') as f:
        f.write('\n# QA ONLY (tools/qa/update-test.py)\n/opt/xxri-qa-update.sh >/dev/null 2>&1 &\n')
    pw = re.search(r'^xxri:[^:]*:(\d+):(\d+):', dfs(img, off, ['cat /etc/passwd']), re.M)
    uid, gid = (pw.group(1), pw.group(2)) if pw else ('1001', '50')
    dfs(img, off, ['rm /opt/bootlocal.sh', 'write %s/bootlocal.sh /opt/bootlocal.sh' % w,
                   'sif /opt/bootlocal.sh mode 0100775', 'sif /opt/bootlocal.sh uid 0', 'sif /opt/bootlocal.sh gid 50',
                   'write %s/hook.sh /opt/xxri-qa-update.sh' % w, 'sif /opt/xxri-qa-update.sh mode 0100755',
                   'write %s/sess /home/xxri/.X.d/zz-qa-update' % w, 'sif /home/xxri/.X.d/zz-qa-update mode 0100644',
                   'sif /home/xxri/.X.d/zz-qa-update uid ' + uid, 'sif /home/xxri/.X.d/zz-qa-update gid ' + gid],
        write=True)
    # 3 + 5. serve, boot until the guest powers off, screenshot Settings
    http = subprocess.Popen([sys.executable, '-m', 'http.server', str(PORT), '--bind', '127.0.0.1'], cwd=srv,
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    qemu, code, _ = bt.FW['bios']
    mon, serial = w + '/mon', out + '/serial.log'
    accel = ['-accel', 'tcg'] if os.environ.get('XXRI_QA_ACCEL') == 'tcg' else ['-enable-kvm', '-cpu', 'host']
    limit = 900 * float(os.environ.get('XXRI_QA_TIME', '1'))
    cmd = [qemu, '-m', '2048', '-machine', 'pc'] + accel + ['-vga', 'std', '-display', 'none',
           '-monitor', 'unix:%s,server,nowait' % mon, '-serial', 'file:' + serial,
           '-netdev', 'user,id=n0', '-device', 'e1000,netdev=n0,romfile=',
           '-drive', 'file=%s,format=raw,if=ide' % img]
    t0 = time.time()
    p = subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=open(out + '/qemu.err', 'w'))
    m = bt.Monitor(mon)
    seen, shot_done = [], False
    try:
        while time.time() - t0 < limit and p.poll() is None:
            ser = open(serial, 'rb').read().decode('latin1', 'replace')
            for mark in re.findall(r'UPDQA_\w+', ser):
                if mark not in seen:
                    seen.append(mark)
                    print('%5.0fs %s' % (time.time() - t0, mark), flush=True)
            if 'UPDQA_SHOT' in ser and not shot_done:
                m.cmd('screendump %s/settings-update.ppm' % w); time.sleep(1)
                subprocess.run(['magick', w + '/settings-update.ppm', out + '/settings-update-after.png'])
                shot_done = True
            time.sleep(1)
    finally:
        powered_off = p.poll() is not None
        p.kill(); p.wait(); http.kill()
    rep = dfs(img, off, ['cat /var/log/xxri-qa-update.log'])
    open(out + '/update-qa.log', 'w').write(rep)
    res = dict(marks=seen, powered_off=powered_off, elapsed=round(time.time() - t0),
               tampered_refused='does not match its SHA-256' in rep,
               installed='"install":{"ok":true' in rep,
               version_after=re.findall(r'^IMAGE_VERSION=(\S+)', rep, re.M),
               new_file='hello from XXRI OS Lite 2.6.1' in rep,
               motd_replaced='updated to 2.6.1 by xxri-system-update' in rep,
               removed='No such file' in rep.split('--- after')[-1].split('--- grub')[0] or
                       'intel.list' not in rep.split('--- after')[-1].split('--- grub')[0],
               prev_kernel='vmlinuz.prev' in rep and 'core.gz.prev' in rep,
               grub_previous='--id xxri-previous' in rep,
               boot2='UPDQA_BOOT2' in seen, screenshot=shot_done)
    json.dump(res, open(out + '/result.json', 'w'), indent=1)
    print(json.dumps(res), flush=True)
    shutil.rmtree(w, ignore_errors=True)


if __name__ == '__main__':
    main()
