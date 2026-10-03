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

## PTXdist build (ISO `75904012…`, one clean build)

Static scan: the 43 ELF files XXRI compiles for the OS (`core.gz` programs
and libraries, `Xorg`, `Xfbdev`, all XLibre modules, GRUB utilities) contain
no instruction above i586. The browser's Qt/Chromium is SSE2 code.

Runtime, whole desktop and every XXRI program (`x-test.py cpu-*`):

| Model | Desktop + 11 program runs | Executed instructions the model lacks |
|---|---|---|
| `pentium,-mmx` (plain P5) | yes | **none** |
| `pentium` (P5 + MMX) | yes | 98 MMX, all in Tiny Core's `libImlib2`, chosen by its CPUID check (none once MMX is removed from CPUID) |
| `pentium2` | yes | **none** |
| `pentium3` | yes | **none** |
| `pentium3,+sse2` | yes | **none** |

The browser's launcher refuses cleanly on every model without SSE2 and the
browser renders its start page and an HTTPS page on the SSE2 model.

Two things had to change for the non-SSE2 results (both measured here):

* Tiny Core's Mesa is i686 code (its `libEGL` executed CMOV on the Pentium
  model) built for SSE2 (`libgallium` executed SSE2 on the Pentium III model).
  On a processor without SSE2, `~/.xsession` now starts both X servers
  without Mesa (`docs/x-server.md`).
* XLibre 25.1.9's modesetting driver queried GL for the cursor format even
  without glamor; upstream `a7454facc6` is applied.

### What this does and does not show

It shows instruction-set compatibility: nothing an i586 lacks is executed by
the desktop and XXRI's programs (MMX only behind CPUID checks). It is
**emulation**: no physical Pentium, K6-2, Pentium II or Pentium III was
available, and speed on such a machine is not measured (the emulated P5 needed
~4 minutes to the desktop). Tiny Core's own extensions beyond those the
desktop loads at boot were not run.

| Class | Desktop + XXRI programs | XXRI Browser |
|---|---|---|
| i586 (Pentium, K5, K6-2, 6x86, Geode GX, VIA C3 "Samuel/Ezra") | runs in emulation | no (needs SSE2) |
| P6 without SSE (Pentium Pro/II, Celeron, K6-III+ ...) | runs in emulation | no |
| SSE without SSE2 (Pentium III, Athlon XP) | runs in emulation | no |
| SSE2 (Pentium 4, Pentium M, Athlon 64, VIA C7 and newer) | runs in emulation | runs in emulation |

## Newer CPUs (branch `xxri-gpu-cpu-support`, not yet tested on hardware)

The baseline above is about the oldest CPUs. For new ones the limits come
from Tiny Core's 32-bit 6.12.11 kernel, not from XXRI's code:

* **8 CPUs** (`NR_CPUS=8`) and **about 4 GiB** (`HIGHMEM4G`, no PAE).
* **Frequency scaling** needs nothing extra: `intel_pstate` and `amd_pstate`
  (active/EPP) are built in, `acpi-cpufreq` and the older drivers are in
  `core.gz`. With the first two, Settings > Power offers only `performance`
  and `powersave`, and `powersave` means dynamic scaling, not the lowest
  clock.
* **Microcode**: new - `boot/intel-ucode.img` (Intel microcode-20260925,
  222 updates) and `boot/amd-ucode.img` (families 10h-1Ah), loaded by GRUB
  in front of `core.gz`, only the image for the CPU's vendor
  (`docs/gpu-cpu-support.md`). This kernel has no late loading, so this is
  the only way newer microcode (errata and security fixes) reaches the CPU
  when the board's firmware carries an older one. Coverage is what the
  vendors publish for Linux: Intel's release covers its client and server
  CPUs broadly; AMD's linux-firmware files cover fewer models (client Ryzen
  updates often come only with a BIOS update). The kernel does not load
  microcode on Intel family 5 (Pentium) or AMD before family 10h, nor inside
  a virtual machine.
