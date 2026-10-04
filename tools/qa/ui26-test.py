#!/usr/bin/env python3
"""ui26-test.py ISO OUTDIR [SCENARIO...] - the 2.6 UI work on the built ISO (QA only).

Boots the ISO's live session through its own firmware and GRUB (x-test.py's
machinery) with tools/qa/ui-hook as the hook: Settings > Wallpaper & Style,
Software Update, Display and About; wallpapers painted by xxri-wallpaper
(Fill, Fit); the dock at Small and Large; dark mode (Settings dark, Files
still light).  Screenshots every scene the hook announces and keeps the hook's
report (backend JSON, timings) as xqa.log.

  ui-kms     -vga std (bochs-drm): Xorg + modesetting, 1024x768
  ui-fbdev   nomodeset: Xfbdev on the firmware framebuffer
"""
import importlib.util, json, os, shutil, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location('xt', os.path.join(HERE, 'x-test.py'))
xt = importlib.util.module_from_spec(spec)
spec.loader.exec_module(xt)

SCENARIOS = {
    'ui-kms':   dict(fw='bios', append='', cpu=None, timeout=720),
    'ui-fbdev': dict(fw='bios', append='nomodeset', cpu=None, timeout=720),
}


def make_hook(iso, out):
    """x-test's hook initrd, with ui-hook's script in place of x-hook's"""
    w = tempfile.mkdtemp()
    subprocess.run(['xorriso', '-osirrox', 'on', '-indev', iso, '-extract', '/boot/core.gz', w + '/core.gz'],
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True)
    os.makedirs(w + '/root/opt')
    subprocess.run('zcat %s/core.gz | (cd %s/root && cpio -idm --quiet opt/bootlocal.sh)' % (w, w), shell=True)
    with open(w + '/root/opt/bootlocal.sh', 'a') as f:
        f.write('\n# QA ONLY (tools/qa/ui-hook, never shipped)\n'
                'grep -q xxriqax /proc/cmdline && /opt/xxri-qa-x.sh >/dev/null 2>&1 &\n')
    shutil.copy(os.path.join(HERE, 'ui-hook', 'opt', 'xxri-qa-x.sh'), w + '/root/opt/')
    os.chmod(w + '/root/opt/xxri-qa-x.sh', 0o755)
    subprocess.run('cd %s/root && find . | sort | cpio -o -H newc --quiet -R 0:0 | gzip -9 -n > %s/xxri-qa-x.gz'
                   % (w, w), shell=True, check=True)
    subprocess.run(['mkfs.fat', '-C', '-n', 'XXRIQAX', out, '4096'], stdout=subprocess.DEVNULL, check=True)
    subprocess.run(['mcopy', '-i', out, w + '/xxri-qa-x.gz', '::/'], check=True)
    shutil.rmtree(w)


def main():
    iso, outdir = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2])
    names = sys.argv[3:] or ['ui-kms']
    os.makedirs(outdir, exist_ok=True)
    hook = os.path.join(outdir, 'ui-hook.img')
    make_hook(iso, hook)
    results = {}
    for n in names:
        r = xt.run(iso, hook, n, SCENARIOS[n], os.path.join(outdir, n))
        results[n] = dict(desktop=r.get('desktop'), server=r.get('server'), done=r.get('done'),
                          scenes=r.get('app_shots', []), apps=r.get('apps', {}), elapsed=r.get('elapsed'))
        print(n, json.dumps(results[n]), flush=True)
    json.dump(results, open(os.path.join(outdir, 'results.json'), 'w'), indent=1)


if __name__ == '__main__':
    main()
