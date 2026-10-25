#!/usr/bin/env python3
"""xxri-kabi-check.py TC_CONFIG CONFIG FRAGMENT TC_SYMVERS.gz SYMVERS

The x86_64 kernel is Tiny Core's source and config plus a fragment, but Tiny
Core's module packages (graphics, wireless, ...) stay, so the image has to
remain ABI-compatible with theirs. CONFIG_MODVERSIONS is off, so the kernel
would never notice a mismatch; this does. It fails on:

  - a config value Tiny Core sets that changed (compiler version strings aside,
    they name the compiler and configure nothing);
  - a new built-in (=y) option that isn't on a '# xxri-kabi-allow: CONFIG_A ...'
    line of the fragment (new =m options are fine, they only add modules);
  - a symbol Tiny Core's vmlinux exports to modules that is missing here or
    exported differently (GPL-only, other namespace).
"""
import gzip, re, sys

TOOLCHAIN = re.compile(r'^CONFIG_(CC_VERSION_TEXT|GCC_VERSION|CLANG_VERSION|AS_VERSION|LD_VERSION|'
                       r'LLD_VERSION|RUSTC_VERSION|RUSTC_LLVM_VERSION|PAHOLE_VERSION|BINDGEN_VERSION_TEXT)$')


def config(path):
    vals = {}
    for line in open(path):
        m = re.match(r'^(CONFIG_\w+)=(.*)$', line.strip())
        if m:
            vals[m.group(1)] = m.group(2)
    return vals


def symvers(path, opener=open):
    out = {}
    with opener(path, 'rt') as f:
        for line in f:
            p = line.rstrip('\n').split('\t')
            if len(p) >= 4 and p[2] == 'vmlinux':
                out[p[1]] = (p[3], p[4] if len(p) > 4 else '')
    return out


def main():
    tc_cfg, cfg, frag, tc_sv, sv = sys.argv[1:6]
    tc, new = config(tc_cfg), config(cfg)
    allow = set()
    for line in open(frag):
        if line.startswith('# xxri-kabi-allow:'):
            allow.update(line.split(':', 1)[1].split())
    errors = []
    for k, v in tc.items():
        if TOOLCHAIN.match(k):
            continue
        if new.get(k) != v:
            errors.append('%s: Tiny Core %s, this build %s' % (k, v, new.get(k, 'unset')))
    for k, v in new.items():
        if k not in tc and v == 'y' and k not in allow:
            errors.append('%s=y: new built-in option, not on a xxri-kabi-allow line' % k)
    a, b = symvers(tc_sv, gzip.open), symvers(sv)
    for s, how in sorted(a.items()):
        if s not in b:
            errors.append('vmlinux no longer exports %s' % s)
        elif b[s] != how:
            errors.append('vmlinux exports %s as %s, Tiny Core as %s' % (s, b[s], how))
    added = sorted(set(b) - set(a))
    for e in errors:
        print('xxri-kabi-check: ' + e, file=sys.stderr)
    if errors:
        sys.exit('xxri-kabi-check: %d incompatibilities with Tiny Core\'s kernel' % len(errors))
    print('xxri-kabi-check: Tiny Core ABI kept (%d exports, %d new: %s%s)'
          % (len(a), len(added), ' '.join(added[:8]), ' ...' if len(added) > 8 else ''))


if __name__ == '__main__':
    main()
