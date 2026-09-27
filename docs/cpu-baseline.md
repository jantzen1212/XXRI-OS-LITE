# CPU baseline: what XXRI OS Lite actually runs on

Measured, not assumed. Two methods, used together:

1. **Static scan** (`tools/qa/isa-scan.py`): disassembles every 32-bit ELF of
   an image and counts instructions an i586 lacks, by class (P6: CMOV,
   FCMOV, FCOMI, NOPL; MMX; 3DNow!; SSE; SSE2; SSE3+), and whether the file
   contains CPUID (i.e. may choose code at run time). A hit is only a
   candidate: a CPUID-dispatched SSE2 path never runs on a CPU without SSE2.
2. **Runtime proof** (`tools/qa/x-test.py cpu-*`): the ISO boots to the
   desktop under QEMU TCG with a period CPU model and every XXRI program is
   started in turn. QEMU does not enforce every feature — `-cpu pentium3`
   executes SSE2 and no model traps multi-byte NOP (verified with a SIGILL
   probe) — so the TCG plugin `tools/qa/isa-sentinel.c` records every
   *executed* instruction the model lacks, and
   `tools/qa/isa-sentinel-map.py` names the file it came from.

| Model | Stands for | Lacks |
|---|---|---|
| `pentium` | Pentium, K6-2, VIA C3 "Samuel/Ezra", Geode GX (i586) | CMOV, NOPL, MMX (on the plain P5), SSE, SSE2 |
| `pentium2` | Pentium Pro/II, K6-III class P6 | SSE, SSE2 |
| `pentium3` | Pentium III, Athlon XP (SSE, no SSE2) | SSE2 |
| `pentium3,+sse2` | Pentium 4 / Pentium M / Athlon 64 / VIA C7 class | SSE3 and later |

## 2.6 (Build A ISO `aa8137be…`, reference)

On `pentium3`, idle desktop:

* `core.gz:/usr/local/sbin/xxri-bootsplash` — 103 distinct SSE2
  instructions (the committed Build A binary, compiled by the host's
  `gcc -m32`, whose default is `-march=x86-64`-level SSE2 code);
* `mesa.tcz:/usr/local/lib/libgallium-25.2.5.so` — 10 distinct SSE2
  instructions, when X.Org's glamor initialises (Tiny Core builds Mesa with
  Mesa's default `-Dsse2=true`).

Static scan of the other committed Build A programs: SSE2/CMOV without
CPUID in installer, dialog, media, settings, store, control center,
launcher, wctl, compositor, flwm and the Debian GL libraries. The 2.6 README's
"Pentium II / K6-2" minimum was not true for 2.6.

## PTXdist build

(results of `x-test.py cpu-*` on the PTXdist ISO — filled in below)
