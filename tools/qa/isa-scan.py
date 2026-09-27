#!/usr/bin/env python3
"""isa-scan.py PATH... - which x86 instruction-set extensions each ELF file uses.

For every 32-bit x86 ELF under the given paths (files, directories, .tcz
squashfs images, or gzip'd cpio initramfs), disassembles it and counts the
instructions that need more than an i586 (Pentium/K6-2):

  p6     cmov*, fcmov*, fcomi/fucomi*, multi-byte nop (0f 1f)   Pentium Pro/II
  mmx    %mm registers                                        Pentium MMX
  3dnow  pf*/pi2f* 3DNow! ops                                  AMD K6-2
  sse    single-precision / MXCSR / prefetch / sfence          Pentium III
  sse2   double-precision, integer ops on %xmm, lfence/mfence   Pentium 4
  sse3+  SSE3/SSSE3/SSE4/AVX mnemonics
  cpuid  the file probes the CPU (so extension use may be dispatched at runtime)

A hit is a requirement only if the code path runs unconditionally; files with
cpuid often dispatch.  Runtime tests on an emulated CPU settle it.
Output: one TSV line per file with instructions > 0 in any class.
"""
import os, re, subprocess, sys, tempfile, concurrent.futures as cf

P6 = re.compile(r'^\s*(cmov\w*|fcmov\w*|f?u?comip?|nop[wl])\b')
SSE2 = re.compile(r'^\s*(\w+pd|\w+sd|movdq[au]|pshuf[dlh]w?|punpck\w+q?dq|cvt\w*2(sd|pd|dq)|cvt(sd|pd|dq)2\w+|lfence|mfence|clflush|maskmovdqu|movnti|movntpd|movntdq|pmuludq|paddq|psubq|pslldq|psrldq)\b')
SSE = re.compile(r'^\s*(\w+ps|\w+ss|ldmxcsr|stmxcsr|sfence|prefetch\w*|movntq|pavg[bw]|pextrw|pinsrw|pmaxsw|pmaxub|pminsw|pminub|pmovmskb|pmulhuw|psadbw|pshufw|maskmovq)\b')
SSE3P = re.compile(r'^\s*(addsub\w+|hadd\w+|hsub\w+|lddqu|movddup|movs[hl]dup|fisttp\w*|pshufb|palignr|pabs\w|psign\w|phadd\w|phsub\w|pmaddubsw|pmulhrsw|pmov[sz]x\w+|ptest|round[ps][sd]|blendv?\w+|pextr[bdq]|pinsr[bdq]|pcmpeqq|pcmpgtq|pmin[su][dw]|pmax[su][dw]|crc32\w*|popcnt|v\w+)\b')
DNOW = re.compile(r'^\s*(pf\w+|pi2f\w|pavgusb|pmulhrw|femms|pswapd)\b')


def classify(path):
    try:
        with open(path, 'rb') as f:
            h = f.read(20)
        if h[:4] != b'\x7fELF' or h[4] != 1 or h[18] != 3:     # 32-bit, EM_386
            return None
    except OSError:
        return None
    out = subprocess.run(['objdump', '-d', '--no-show-raw-insn', path], capture_output=True, text=True).stdout
    c = dict(p6=0, mmx=0, dnow=0, sse=0, sse2=0, sse3p=0, cpuid=0)
    for line in out.splitlines():
        t = line.split('\t')
        if len(t) < 2:
            continue
        ins = t[-1]
        if ins.startswith('cpuid'):
            c['cpuid'] += 1
        elif DNOW.match(ins) and '%mm' in ins:
            c['dnow'] += 1
        elif SSE3P.match(ins) and ('%xmm' in ins or '%mm' in ins or ins.startswith(('fisttp', 'crc32', 'popcnt'))):
            c['sse3p'] += 1
        elif '%xmm' in ins and SSE2.match(ins):
            c['sse2'] += 1
        elif ins.startswith(('lfence', 'mfence', 'clflush', 'movnti')):
            c['sse2'] += 1
        elif SSE.match(ins) and ('%xmm' in ins or '%mm' in ins or ins.startswith(('ldmxcsr', 'stmxcsr', 'sfence', 'prefetch'))):
            c['sse'] += 1
        elif '%xmm' in ins:
            c['sse2' if re.match(r'^\s*(p\w+|movq|movd)\b', ins) else 'sse'] += 1
        elif '%mm' in ins:
            c['mmx'] += 1
        elif P6.match(ins):
            c['p6'] += 1
    return c


def files_under(p, tmp):
    if os.path.isdir(p):
        for top, _, names in os.walk(p):
            for n in names:
                yield os.path.join(top, n)
    elif p.endswith('.tcz'):
        d = tempfile.mkdtemp(dir=tmp)
        subprocess.run(['unsquashfs', '-q', '-n', '-d', d + '/x', p], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        yield from files_under(d + '/x', tmp)
    elif p.endswith('.gz'):
        d = tempfile.mkdtemp(dir=tmp)
        subprocess.run('zcat "%s" | (cd "%s" && cpio -idm --quiet 2>/dev/null)' % (p, d), shell=True)
        yield from files_under(d, tmp)
    else:
        yield p


def main():
    tmp = tempfile.mkdtemp()
    jobs = []
    with cf.ThreadPoolExecutor(os.cpu_count()) as ex:
        for arg in sys.argv[1:]:
            label = os.path.basename(arg)
            for f in files_under(arg, tmp):
                if os.path.isfile(f) and not os.path.islink(f):
                    jobs.append((label, f, ex.submit(classify, f)))
        print('source\tfile\tp6\tmmx\t3dnow\tsse\tsse2\tsse3+\tcpuid')
        for label, f, fut in jobs:
            c = fut.result()
            if c and any(c[k] for k in ('p6', 'mmx', 'dnow', 'sse', 'sse2', 'sse3p')):
                rel = re.sub(r'^.*?/(x|tmp\w+)/', '', f)
                print('%s\t%s\t%d\t%d\t%d\t%d\t%d\t%d\t%d' % (label, rel, c['p6'], c['mmx'], c['dnow'],
                      c['sse'], c['sse2'], c['sse3p'], c['cpuid']), flush=True)
    subprocess.run(['rm', '-rf', tmp])


if __name__ == '__main__':
    main()
