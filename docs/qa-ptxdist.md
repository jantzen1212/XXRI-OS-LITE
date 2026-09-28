# QA — the PTXdist-native build with XLibre (XXRI OS Lite 2.6)

Everything here was measured on one ISO: the output of **one clean PTXdist
build** — `rm -rf platform-xxri; ptxdist --j-intern=6 images` at commit
`35eaf7d` (branch `xxri-ptxdist-native`, clean tree), 201.5 min wall /
1010 min CPU on an 8-core i7-9700 with 32 GB, OSELAS.Toolchain-2024.11.1
(i586, GCC 14.2.1, glibc 2.40).

`XXRI-Lite-2.6.iso` — 254,650,368 bytes, sha256
`75904012dd31a4fc9e05ca47d6c55093cade254ca44ed2d4192d395087eb764d`.

The reference is the 2.6 release ISO (`aa8137be…`, Build A). Harnesses:
`tools/qa/release-qa.sh` (everything below in one run), `full-qa.sh`,
`x-test.py`, `validate-release.sh`, the TCG plugin `isa-sentinel.c`.

## Sizes

| | 2.6 | PTXdist | |
|---|---|---|---|
| ISO | 278,568,960 B (265.7 MiB) | 254,650,368 B (242.9 MiB) | −8.6 % |
| `core.gz` | 16,676,285 B | 15,933,784 B | −4.5 % |
| `xxri-browser.tcz` | 93,298,688 B | 70,942,720 B | −24 % |
| X server | 3,387,392 B (5 extensions: `xorg-server`, `Xvesa`, `libXfont`, `xf86-input-libinput`, `xf86-video-fbdev`) | 2,482,176 B (`xxri-xlibre.tcz`) | −27 % |
| `xxri-grub.tcz` | 3,629,056 B | 3,723,264 B | +2.6 % |

## Reproducibility

* **Image assembly is deterministic**: deleting every image and running
  `ptxdist images` again reproduced `core.gz`, all three `.tcz` and the ISO
  byte for byte (`75904012…` again).
* **No build path in any shipped file**: none of the 2,026 files in
  `core.gz` and the three XXRI extensions contains the build directory
  (two leaks were found and fixed on the way — see below).
* **Packages built in a second directory**: the 22 non-Qt packages
  (every XXRI program and library, XLibre and both drivers, the GRUB
  utilities, `xxri-base`) were built again in another workspace, at another
  path, from a later commit that touches no package content - **22 of 22
  payloads identical** file by file (`tools/qa/ipk-compare.py`). Before the
  two fixes below, the same comparison found the build path inside XLibre.
* Not measured: a second clean build of Qt WebEngine (3+ hours).

## Release gate (`validate-release.sh --boot`)

0 failures. ELF closure: 97 XXRI ELF files, every library and symbol version
resolves inside the image. Static ISA scan: the 43 ELF files XXRI compiles
for the OS (programs, libraries, `Xorg`, `Xfbdev`, every XLibre module, the
GRUB utilities) contain **no instruction above i586**; the browser's Qt is
SSE2 code throughout, as designed. One warning — no `sources/` next to the
ISO — is fixed (commit `11d1a1a`; the release directory carries it, see
"Corresponding Source").

## Boot matrix (`full-qa.sh`, same as for Build A/B)

| | Result |
|---|---|
| SeaBIOS + 32-bit OVMF × CD / disk / USB → desktop | 6/6 (userspace 24–35 s) |
| 64-bit OVMF | refuses the medium cleanly (documented) |
| direct kernel + `core.gz`, 1 GiB and 16 GiB | 2/2 |
| RAM 256 MiB – 16 GiB (BIOS), 256 MiB – 8 GiB (IA32 UEFI), default + kernel log | 30/30 (userspace 22.6–24.7 s) |
| media × RAM | 12/12 (18.1–25.6 s) |
| Rufus ISO mode: MBR/BIOS, MBR/IA32, GPT/IA32 | 3/3 (GPT on BIOS does not boot, as expected — Rufus does not offer it) |
| Ventoy 1.1.17, MBR + GPT × BIOS + IA32 | 4/4 |
| Installer, 5 scenarios, installed disk booted alone | 5/5 installs; installed disks booted to the desktop 5/5 in reruns — see note |

Note: in the release run the installed IA32-UEFI manual-MBR disk still showed
the boot splash at the 90 s screenshot (userspace at 22 s) while the host was
compiling with 6 jobs. Two reruns on an idle host reached the desktop before
60 s every time (screenshots at 60/90/150/240 s). Not reproduced; recorded.

## Display, input, acceleration and fallback (`x-test.py`, KVM)

Each scenario boots the ISO through its own firmware and GRUB, reports the X
server, its extensions and logs, records XInput2 events while the host
injects 25 pointer packets, 2 clicks, 4 wheel steps and 3 key presses.

| Scenario | Display device | Server | Motion / clicks / wheel / keys |
|---|---|---|---|
| `kms-bios`, `kms-ia32` | bochs-drm (KMS) | Xorg + modesetting | 33 / 2 / 4 / 3 |
| `fbdev-bios`, `fbdev-ia32` | `nomodeset`: vesafb / efifb | Xfbdev | 20 / 3 / 4 / 3 ¹ |
| `vesa-bios` | none (`nomodeset`, GRUB in text mode) | Xorg + vesa (VBE 3.0 via int10) | 33 / 2 / 4 / 3 |
| `gpu-cirrus` | cirrus-drm (KMS) | Xorg + modesetting | 33 / 2 / 4 / 3 |
| `gpu-vmware` | vmwgfx takes over, then rejects QEMU's adapter: none | Xorg + vesa | 33 / 2 / 4 / 3 |
| `gpu-rage128` | no KMS driver: vesafb | Xfbdev | 20 / 3 / 4 / 3 ¹ |

¹ kdrive's mouse driver detects the protocol from the first five packets
(about 50 ms of real movement) and does not deliver them.

Acceleration: with QEMU's adapters Mesa has only its software rasteriser, and
the modesetting driver deliberately refuses glamor on llvmpipe (`Refusing to
try glamor on llvmpipe`) and draws unaccelerated — so **glamor itself could
not be exercised here; it needs a real GPU with a Mesa driver**. GLX is
present on every path (Mesa swrast) except on processors without SSE2, where
both servers run without Mesa (below). Radeon 7000 (`gpu-rv100`): the radeon
KMS driver hangs QEMU's incomplete `ati-vga` emulation after failing to load
`radeon/R100_cp.bin` — no statement about a real Radeon is possible.

The live session's first screen (the installer's welcome) is
**pixel-identical to 2.6** (0 differing pixels, `kms-bios`, desktop and after
input).

## CPU classes (QEMU TCG + ISA sentinel)

The whole ISO boots to the desktop and every XXRI program is started (Settings,
Store, Control Center, Launcher, Media, Files, Power menu, a dialog,
Installer, Browser on its start page, Browser on `https://example.org/`); the
sentinel records every executed instruction the emulated CPU lacks.

| Model | Stands for | Desktop | Programs | Sentinel hits | Browser |
|---|---|---|---|---|---|
| `pentium,-mmx` | plain Pentium (P54C), K5, 6x86, WinChip C6 | Xorg, no acceleration, no GLX | 11/11 | **0** | refuses: needs SSE2 |
| `pentium` | Pentium MMX, K6/K6-2/K6-III (i586 + MMX) | same | 11/11 | 98 × MMX, all in Tiny Core's `libImlib2` behind its CPUID check (0 with MMX removed, above) | refuses |
| `pentium2` | Pentium Pro/II, Celeron (P6) | same | 11/11 | **0** | refuses |
| `pentium3` | Pentium III, Athlon XP (SSE, no SSE2) | same | 11/11 | **0** | refuses |
| `pentium3,+sse2` | Pentium 4, Pentium M, Athlon 64 class | Xorg, GLX | 11/11 | **0** | renders both pages |

Emulated CPUs are slow (the P5 model needed ~4 min to the desktop and more than
40 s per GTK program to map); these runs prove instruction-set compatibility,
not speed on period hardware.

## Programs and memory (KVM, 1 GiB guest, same harness for both ISOs)

Every program is started with the live session's own environment (read from
a program `~/.xsession` started), as the dock starts it.

| | 2.6 | PTXdist |
|---|---|---|
| X server resident | 95.4 MB (X.Org) | 97.8 MB (XLibre) |
| idle: used / available | 117.4 / 723.0 MB | 126.4 / 712.7 MB |
| Browser, start page (all processes) | 435 MB | 428 MB |
| Browser, `https://example.org/` | 446 MB | 444 MB |
| Settings, Store, Control Center, Launcher, Media, Files, Power menu, dialog, Installer | 8–33 MB each | within ±2 MB of 2.6 each |
| desktop ready | 46.7 s | 46.7 s |

The PTXdist system uses **9 MB more at idle**. Browser resident sizes count
shared library pages once per process, so they overstate real use (equally for
both).

## Defects found by this QA and fixed before the clean build

| Found by | Defect | Fix |
|---|---|---|
| ISO, first boot test | `Xorg` ran as the session user and failed on every KMS machine; 2.6's X.Org ran as root through Tiny Core's setuid wrapper | XLibre's `Xorg.wrap` + `Xwrapper.config` in Tiny Core's layout; gate checks the setuid bits |
| GPU scenarios | the boot splash held `/dev/fb0`, so cirrus/vmwgfx failed to take over with EBUSY — no display device at all | `xxri-splash-handover` around display-driver loading |
| GPU scenarios | no X server for a BIOS machine left without KMS and framebuffer (Xvesa's old role) | Xorg + XLibre vesa driver; XLibre needs upstream `7dd2a11ae6` (32-bit x86 lost `-D_PC`) |
| GPU scenarios | static `/dev/fb0` in `core.gz` made the session skip the vesa fallback | test `/sys/class/graphics/fb0` |
| CPU classes | on i586, both servers reached Tiny Core's Mesa (i686 CMOV code) → SIGILL, no desktop | upstream `a7454facc6` for modesetting; no Mesa on processors without SSE2 |
| app runs | the browser's renderer was killed by Chromium 87's seccomp policy (glibc's direct `socketpair` on i386); the session exported 2.6's prebuilt-Qt paths that the new browser does not have | the launcher owns its environment (flags as in 2.6, stale paths cleared) |
| package comparison | GLX compiled with the build machine's Mesa driver path; Qt `__FILE__` strings with the build path | `PTXDIST_PKG_CONFIG_VAR_NO_SYSROOT`; `-fmacro-prefix-map` |
| rule order | the ISO rule never depended on the X server extension (variable read before it was defined) | image prerequisites by file name |
| release gate | `sources/` promised by the ISO was never produced | Corresponding Source exported with the ISO |

## Not tested / limits of this QA

* **Real hardware.** Everything above is QEMU. No claim is made for any
  specific machine; the NEC VersaPro VK24MX findings in
  `docs/hardware-issues-2.6.md` still apply.
* **GPU acceleration** (glamor, GLX on a real driver): not reachable in QEMU.
* **Period CPUs**: instruction-set compatibility is proven in emulation; speed
  on a real Pentium/K6 is not measured.
* The Radeon 7000 path (QEMU hangs). GPU firmware is not shipped at all
  (as in 2.6): Radeon R100–R500 lose acceleration, newer GPUs may fail KMS and
  land on the vesa fallback (BIOS) or no desktop (UEFI).
* XXRI Browser runs **without Chromium's sandbox**, as 2.6 did (see above).
* The live session's first-run screen runs without the dock and without a
  window manager — identical to 2.6 (`/etc/sysconfig/desktop` is written only
  by the installer); not changed here.
