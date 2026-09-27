#!/usr/bin/env python3
"""install-test.py ISO OUTDIR [SCENARIO...] - end-to-end installer test (QA only).

For each scenario:
  1. boot the live ISO through its own firmware + GRUB (USB), append the QA hook
     initrd in GRUB's editor, let xxri-install-backend install unattended onto
     a 6 GiB target disk, and wait for the guest to power off;
  2. boot the TARGET DISK ALONE through its firmware + the GRUB the installer
     wrote, and require userspace (DHCP) and a desktop screenshot.
"""
import json, os, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
BOOTTEST = os.path.join(HERE, '..', 'qemu-boot-test.py')
GIB6 = 6 * 1024 ** 3

# name: (live firmware, target preparation, xxriqa mode, [firmware to boot the result with])
SCENARIOS = {
    'bios-auto-mbr':      ('bios', 'blank', 'auto', ['bios']),
    'ia32-auto-gpt':      ('ia32', 'blank', 'auto', ['ia32', 'bios']),
    'bios-manual-gpt':    ('bios', 'gpt-biosboot', 'manual:2', ['bios']),
    'ia32-manual-mbr':    ('ia32', 'mbr-esp', 'manual:2', ['ia32']),
    'bios-manual-mbr63':  ('bios', 'mbr-sector63', 'manual:1', ['bios']),
}


def prepare(path, kind):
    with open(path, 'wb') as f:
        f.truncate(GIB6)
    if kind == 'gpt-biosboot':
        subprocess.run(['sgdisk', '-n', '1:2048:+1M', '-t', '1:EF02', '-n', '2:0:0', '-t', '2:8300', path],
                       check=True, stdout=subprocess.DEVNULL)
    elif kind == 'mbr-esp':
        subprocess.run(['sfdisk', '-q', path], input=b'label: dos\n2048,256M,ef\n,,83\n', check=True)
        # -s 1: a 256 MiB FAT32 needs 512-byte clusters to reach the 65525-cluster
        # FAT32 minimum; with fewer, UEFI's FAT driver reads it as FAT16 and fails.
        subprocess.run(['mkfs.fat', '-F', '32', '-s', '1', '--offset', '2048', '-n', 'OTHERESP', path,
                        str(256 * 1024)], check=True, stdout=subprocess.DEVNULL)
    elif kind == 'mbr-sector63':          # how 2.5's installer (busybox fdisk defaults) partitioned
        subprocess.run(['sfdisk', '-q', path], input=b'label: dos\n63,,83,*\n', check=True)


def boot(args):
    out = subprocess.run([sys.executable, BOOTTEST] + args, capture_output=True, text=True)
    return json.loads(out.stdout.strip().splitlines()[-1])


def main():
    iso, outdir = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2])
    names = sys.argv[3:] or list(SCENARIOS)
    os.makedirs(outdir, exist_ok=True)
    hook = os.path.join(outdir, 'qa-hook.img')
    subprocess.run([os.path.join(HERE, 'make-install-hook.sh'), hook], check=True)
    results = {}
    for name in names:
        fw, kind, mode, bootfw = SCENARIOS[name]
        d = os.path.join(outdir, name)
        os.makedirs(d, exist_ok=True)
        target, logd = os.path.join(d, 'target.img'), os.path.join(d, 'log.img')
        prepare(target, kind)
        with open(logd, 'wb') as f:
            f.truncate(2 * 1024 ** 2)
        edit = [['down'], ['type', 'search --no-floppy --set=h --file /xxri-qa-hook.gz'],
                ['down'], ['end'], ['type', ' xxriqa=' + mode],
                ['down'], ['end'], ['type', ' ($h)/xxri-qa-hook.gz']]
        inst = boot(['--fw', fw, '--media', 'usb', '--mem', '2048', '--img', iso, '--disk', hook,
                     '--disk-rw', target, '--disk-rw', logd, '--grub-edit', json.dumps(edit),
                     '--until-poweroff', '--shots', '', '--timeout', '900', '--out', d + '/install'])
        log = open(logd, 'rb').read().split(b'\0')[0].decode('latin1', 'replace')
        open(d + '/install.log', 'w').write(log)
        r = dict(install_powered_off=inst.get('powered_off'), install_elapsed=inst.get('elapsed'),
                 backend_ok='DONE|' in log and 'BACKEND_RC=0' in log,
                 backend_error=[l for l in log.splitlines() if l.startswith('E|')], boots={})
        for bf in bootfw:
            b = boot(['--fw', bf, '--media', 'hdd', '--mem', '2048', '--img', target,
                      '--shots', '90', '--timeout', '130', '--out', '%s/boot-%s' % (d, bf)])
            r['boots'][bf] = dict(grub_menu=b['grub_menu'], userspace=b['userspace'],
                                  screen=[s['brightness'] for s in b['shots']])
        results[name] = r
        print(name, json.dumps(r), flush=True)
    json.dump(results, open(os.path.join(outdir, 'results.json'), 'w'), indent=1)


if __name__ == '__main__':
    main()
