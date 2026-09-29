# QA — the PTXdist-native build with XLibre (XXRI OS Lite 2.6)

**Release candidate: `XXRI-Lite-2.6.iso`, 254,803,968 bytes, sha256
`28d6304c6e14bc7c41dcf745a4e032151465ec2046b9e57b5c75e7b467ee003d`** — one
clean PTXdist build (`rm -rf platform-xxri; ptxdist --j-intern=6 images`) at
commit `3ae016b` (branch `xxri-ptxdist-native`, clean tree), 209.6 min wall /
1013 min CPU on an 8-core i7-9700 with 32 GB, OSELAS.Toolchain-2024.11.1
(i586, GCC 14.2.1, glibc 2.40).

It is the previous candidate (`75904012…`, clean build at `35eaf7d`, the
full QA below) plus the GPU firmware extension. Between the two clean builds
`core.gz`, `xxri-xlibre.tcz` and `xxri-grub.tcz` are byte-identical; the
browser extension differs only in `libQt5WebEngineCore` (see
Reproducibility). So the full QA carries over for everything byte-identical,
and on the new ISO were re-run: the release gate with its boots, the GPU
scenarios the firmware affects, the standard KMS path, an installer
scenario, and the programs including the browser ("GPU firmware" below).

The reference is the 2.6 release ISO (`aa8137be…`, Build A). Harnesses:
`tools/qa/release-qa.sh` (everything below in one run), `full-qa.sh`,
`x-test.py`, `validate-release.sh`, the TCG plugin `isa-sentinel.c`.

## Sizes

| | 2.6 | PTXdist | |
|---|---|---|---|
| ISO | 278,568,960 B (265.7 MiB) | 254,803,968 B (243.0 MiB) | −8.5 % |
| `xxri-firmware-gpu.tcz` | — (no GPU firmware) | 151,552 B | new |
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
* **Two clean builds** (`35eaf7d` and `3ae016b`, which differ in no package
  content): `core.gz`, `xxri-xlibre.tcz`, `xxri-grub.tcz` byte-identical.
  `xxri-browser.tcz` is not: `libQt5WebEngineCore.so` has the same size and
  the same 3,304 exported symbols, but `.text` differs by 64 bytes net, with
  code shifted by -1,648 to +192 bytes at several places - some Chromium
  translation units compile differently from build to build under GCC.
  Everything else in the browser extension is identical. Not root-caused.

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

## GPU firmware (`xxri-firmware-gpu.tcz`)

**What ships** (`config/firmware/gpu.list`): 81 files the kernel's own
drivers declare, unmodified from Tiny Core 16's `firmware-radeon.tcz` and
`firmware-i915.tcz` (linux-firmware 20240220), 760 KiB unpacked:

| Driver | Hardware | Files |
|---|---|---|
| radeon | R100–R500: Radeon 7000 … X1950 (1999–2008) | CP microcode |
| radeon | R600–R700: HD 2000 … HD 4000, HD 3200/4200 IGPs | PFP, ME, RLC, SMC |
| radeon | Evergreen HD 5000; Palm/Sumo APUs (E-350/C-50 netbooks, Llano); Northern Islands HD 6000; Aruba (Trinity/Richland) | PFP, ME, RLC, SMC; MC for NI, without which radeon cannot start an NI card at all |
| i915 | Gen9: Skylake, Kaby Lake, Broxton, Gemini Lake | DMC (display power states) |

**Left out**, as not useful on XXRI's hardware or unused: radeon UVD/VCE
(~0.7 MiB for these families — nothing decodes video on the GPU), radeon
SI/CIK (HD 7000 and newer, 5.4 MiB — they take the vesa fallback on BIOS
machines), amdgpu (20.8 MiB compressed, GCN 1.2+), i915 GuC/HuC (off by
default before Gen12) and Gen11+ DMC (64-bit-UEFI machines this ISO does not
boot). The Tiny Core kernel has no nouveau: NVIDIA cards always use the
framebuffer or vesa path, and no NVIDIA firmware would help.

**Size impact**: the extension is 151,552 B; the ISO grows by 153,600 B
(+0.06 %, 254,650,368 → 254,803,968). It is loop-mounted like every other
extension, not copied into RAM. For comparison, Tiny Core's complete
packages are 2.85 MB (radeon) + 12.5 MB (i915) + 20.8 MB (amdgpu).

**Re-run on the new ISO:**

| Test | Result |
|---|---|
| release gate: firmware extension in `onboot.lst`, contents = `gpu.list` | ok (81 files, 148 KiB) |
| release gate: ELF closure, ISA, X server checks | ok (187 extensions) |
| SeaBIOS + 32-bit OVMF × CD/disk/USB → desktop; 64-bit OVMF refusal; direct boot 1/16 GiB | 6/6, ok, 2/2 |
| `gpu-rv100` (Radeon 7000, radeon KMS), kernel log on serial | the firmware now reaches the driver: before, `Failed to load firmware "radeon/R100_cp.bin"` and `failed initializing CP (-2)`; now the file is served, the microcode uploaded, and the CP ring test fails (`-22`) because QEMU's `ati-vga` does not emulate the command processor. The guest then hangs in QEMU as before. |
| `kms-bios`, `gpu-rage128` (unaffected paths, regression) | desktop, input 33/2/4/3 and 20/3/4/3, as before |
| installer `bios-auto-mbr`, installed disk read back | installs and boots; the installed root holds all 81 files (content identical) and both licences |
| all XXRI programs incl. the browser (start page, HTTPS page) | 11/11 |

Not verifiable here: that a real Radeon or Intel Gen9 GPU uses the
firmware. QEMU emulates neither the Radeon command processor nor any Intel
GPU; the delivery path (Tiny Core's udev helper serving
`/usr/local/lib/firmware`) is proven, the file names match what the drivers
request.

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
* The Radeon 7000 path (QEMU hangs) and whether real legacy Radeon / Intel
  Gen9 GPUs use the shipped firmware. GPUs whose firmware is not shipped
  (radeon SI/CIK, amdgpu) may fail KMS and land on the vesa fallback (BIOS)
  or no desktop (UEFI).
* XXRI Browser runs **without Chromium's sandbox**, as 2.6 did (see above).
* The live session's first-run screen runs without the dock and without a
  window manager — identical to 2.6 (`/etc/sysconfig/desktop` is written only
  by the installer); not changed here.
