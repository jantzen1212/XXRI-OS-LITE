#!/usr/bin/env python3
"""boot-matrix.py ISO OUTDIR [SET] - run qemu-boot-test.py over a boot matrix.

Every case boots through real firmware + GRUB (never -kernel/-initrd).
'default' cases boot the shipped default entry untouched; 'klog' cases append
a serial console in GRUB's own editor so the kernel log (initrd placement,
EFI state) can be recorded.  Writes OUTDIR/summary.tsv.
"""
import concurrent.futures as cf, json, os, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
KLOG = 'console=ttyS0,115200 loglevel=7'

SETS = {
    'ram': [(fw, 'usb', mem, kind)
            for fw, mems in (('bios', (256, 512, 1024, 2048, 3072, 4096, 8192, 16384)),
                             ('ia32', (256, 512, 1024, 2048, 3072, 4096, 8192)))
            for mem in mems for kind in ('default', 'klog')],
    'media': [(fw, media, mem, 'default')
              for fw in ('bios', 'ia32') for media in ('cd', 'hdd', 'usb') for mem in (1024, 4096)],
}


def run(iso, out, case):
    fw, media, mem, kind = case
    name = '%s-%s-%dM-%s' % (fw, media, mem, kind)
    d = os.path.join(out, name)
    for attempt in range(2):
        cmd = [sys.executable, os.path.join(HERE, 'qemu-boot-test.py'), '--fw', fw, '--media', media,
               '--mem', str(mem), '--img', iso, '--out', d, '--shots', '90', '--timeout', '130']
        if kind == 'klog':
            cmd += ['--edit-append', KLOG]
        subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                       timeout=240 * float(os.environ.get('XXRI_QA_TIME', '1')))   # see qemu-boot-test.py
        r = json.load(open(os.path.join(d, 'result.json')))
        if r.get('grub_menu') is not None or not r.get('qemu_error'):
            break
    r['name'], r['attempts'] = name, attempt + 1
    return r


def main():
    iso, out = os.path.abspath(sys.argv[1]), sys.argv[2]
    cases = SETS[sys.argv[3] if len(sys.argv) > 3 else 'ram']
    os.makedirs(out, exist_ok=True)
    with cf.ThreadPoolExecutor(4) as ex:
        results = list(ex.map(lambda c: run(iso, out, c), cases))
    with open(os.path.join(out, 'summary.tsv'), 'w') as f:
        f.write('case\tgrub_menu_s\tuserspace_s\tscreen@90s\tramdisk\tkernel\n')
        for r in results:
            b = r['shots'][0]['brightness'] if r['shots'] else ''
            f.write('%s\t%s\t%s\t%s\t%s\t%s\n' % (r['name'], r['grub_menu'], r['userspace'], b,
                                                  r['ramdisk'] or '', '; '.join(r.get('kernel', []))))
    print(open(os.path.join(out, 'summary.tsv')).read())


if __name__ == '__main__':
    main()
