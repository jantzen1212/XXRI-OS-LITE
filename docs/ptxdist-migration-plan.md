# XXRI OS Lite — migration to a PTXdist-native production build

Status: plan, written after inspecting the whole tree on branch
`xxri-ptxdist-native` (from `xxri-2.6-ptxdist` b54b20a). Every number below was
measured on this tree; nothing is estimated unless it says so.

## 1. What the tree contains today

### 1.1 Repository (2247 tracked files)

| Area | Files | What it really is | Problem |
|---|---|---|---|
| `rootfs/` | 1762 | the initramfs tree, committed file by file | **1636 files are byte-identical to Tiny Core 16.0's `core.gz`** (an upstream release artefact); only 61 are XXRI modifications of Tiny Core files and 486 are XXRI additions |
| XXRI programs in `rootfs/usr/local/bin`, `sbin` | 12 ELF | compiled once by hand with the host's `gcc -m32` (Arch GCC → **SSE2 code**: installer 136, dialog 136, media 8, compositor 2 SSE2 instructions) | binaries in git; `xxri-dialog` has no recipe; README's "K6-2 minimum" is false for them |
| copies in `rootfs/usr/local/src/*` | 3 ELF + 46 sources | sources shipped *inside the initramfs* (costs RAM on every boot) plus three stale binaries | |
| `xxri-browser` launcher | 1 ELF | built with clang in `work/` against Tiny Core's Qt | no reproducible recipe |
| `flwm`, `flwm-stock` | 2 ELF | XXRI-patched window manager and a stock copy; the patched **source exists only in the untracked `work/flwm-xxri/`** (no pristine base, no patch) | the fork is not in version control |
| libfm 1.3.2 + `xxri-file` | 10 ELF | built in an untracked 1.9 GB i686 build root (`work/xxri-file/buildroot`) | |
| `libexif`, `libmenu-cache`, `xset` | 3 ELF | copied out of Tiny Core packages | `xset` is no longer called anywhere (its `.xsession` block was removed in the boot-speed work) |
| gdk-pixbuf 2.42.10 | 15 ELF | **Debian i386 build**, ported during Phase 9. In the live session it shadows Tiny Core's own `gdk-pixbuf2.tcz` (same version, same NEEDED set); installed systems unsquash Tiny Core's over it, so live and installed systems already run different gdk-pixbuf builds | the Phase 9 notes record that the port was unnecessary once `loaders.cache` exists |
| libglvnd (`libGLdispatch`, `libOpenGL`, `libGLESv2`), `libglapi` | 4 ELF | Debian i386 builds, added so AppImages from the Store find these sonames (Tiny Core 16 has none of them: Mesa 25 folded glapi into libgallium) | |
| `xxri-bootsplash` | 1 ELF | static binary | |
| `rootfs-overrides/` | 18 | generated caches (MIME, icon caches, GSettings schemas, `loaders.cache`) and a second copy of `flwm`/`flwm-stock` | generated output committed by hand after running `build-caches.sh` |
| `src/xxri-browser/` | 98 | the browser fork **plus 44 build products** (`*.o`, `moc_*`, `qrc_*`, `ui_*`, `Makefile`, `.qmake.stash`, the binary) | |
| `src/xxri-file/` | 130 | the PCManFM fork (autotools tree) | |
| build scripts | 6 + 14 per-app | `build-iso.sh`, `build-core.sh`, `build-caches.sh`, `build-writable-image.sh`, `qverify.sh`, per-app `build.sh`/`package.sh` with absolute paths into `work/` | |
| `ptxdist/` | 147 | Build B (2.6 comparison): PTXdist orchestration of the above, 11 programs compiled | still ships all the committed binaries except those 11 |

### 1.2 Inputs that live outside git today

`work/iso` (kernel, 196 `.tcz`), `work/xxri-file/buildroot` (i686 build root,
Tiny Core compiletc), `work/xxri-browser` (Qt WebEngine 5.15.10 sources, a
self-built Python 2.7, the engine build tree), `work/flwm-xxri` (the only copy of
the window-manager fork), and `sources/vendor/xxri-browser.tcz` (93 MB, the
shipped browser engine). Build B already pinned the kernel and the 196
extensions; the rest is still unreproducible from the repository.

### 1.3 The browser as shipped

`xxri-browser.tcz` = Qt WebEngine 5.15.10 (Chromium 87) compiled with clang
against **Tiny Core's Qt 5.15 libraries**, which the package re-bundles into
`/usr/local/lib` together with ICU 70, OpenSSL **1.1** (end of life since
2023-09) and libopus. Engine features on: JavaScript/V8, WebGL, accelerated
2D canvas, WebRTC, PDF viewer and printing, spellchecker, Pepper plugins,
extensions (PDF viewer), proprietary codecs (H.264/AAC/MP3, `ffmpeg_branding=Chrome`),
Widevine CDM loading, PulseAudio + ALSA, geolocation (QtPositioning),
QtWebChannel. DevTools resources are deleted by `package.sh`; only the en-US
locale ships.

## 2. Measured CPU baseline (static scan, `tools/qa/isa-scan.py`)

| Component | Instruction classes found | Guarded by CPUID? |
|---|---|---|
| Kernel (Tiny Core 6.12.11) | built `CONFIG_M486` | — |
| Tiny Core base (busybox, glibc 2.40, udev) | i486; glibc/libgcc_s SSE/SSE2 only in ifunc variants | yes |
| XXRI programs, Build A binaries | SSE2 + CMOV | no → need a Pentium 4 class CPU |
| XXRI programs, Build B (`-march=i586`) | none | — |
| Browser engine `libQt5WebEngineCore` | 714,213 SSE2, 18,944 SSE3+ | 58 CPUID sites (V8, codecs) |
| Tiny Core's Qt 5.15 libs in the browser | SSE2 in Gui/Quick/Widgets | Qt dispatches its SIMD paths |
| Mesa `libgallium`, libavcodec, libvpx, x264, pixman, FLAC, webp | SSE2 | mostly (to be confirmed at runtime) |
| libasound, libsndfile, libavformat, Debian gdk-pixbuf/glvnd | CMOV | no |

QEMU TCG, used for CPU-class tests, **does not enforce every feature**: a
static probe (`scratchpad/isatest`) showed `-cpu pentium` traps CMOV, SSE and
SSE2, `-cpu pentium2` traps SSE and SSE2, but **`-cpu pentium3` executes SSE2
instructions** and no model traps multi-byte NOP (`0f 1f`). A pass on
`-cpu pentium3` alone therefore proves nothing about SSE2. The CPU-class QA
adds a TCG plugin that flags every executed instruction the emulated CPU does
not have (section 5).

### 2.1 Why the browser engine needs SSE2 (source evidence)

1. **V8** (`v8/src/codegen/ia32/assembler-ia32.cc`,
   `CpuFeatures::ProbeImpl`): `CHECK(cpu.has_sse2());  // SSE2 support is
   mandatory.` The ia32 back end has no x87 floating-point path; the x87 port
   (`v8/src/x87`) was deleted upstream in 2017. The interpreter is not a way
   around it: V8's bytecode handlers and builtins are generated at build time
   by the same ia32 back end into the embedded blob, so even `--jitless`
   executes SSE2.
2. **Chromium's toolchain config** (`build/config/compiler/BUILD.gn`): every
   x86 target is compiled with `-msse2 -mfpmath=sse -mmmx`, and Skia, the
   compositor and media code assume it.
3. Qt 5.15 itself does **not** require SSE2 (its SIMD paths are dispatched in
   `qsimd.cpp`), so the SSE2 floor comes from the engine alone.

Consequence: a Pentium II/III (or K6-2, K7 Athlon/Athlon XP, VIA C3) build of
*this* browser would need a new V8 floating-point back end — not realistic.
The plan therefore keeps the engine at SSE2, makes the rest of the OS as
compatible as it measurably is, and makes the browser **refuse to start with
an explanation** on a CPU without SSE2 instead of crashing. Alternatives that
do run on P2/P3 (QtWebKit 5.212 with the JavaScriptCore C loop, NetSurf) are
different engines with a large loss of web-platform features; they are not
the forked browser and are recorded as options, not implemented.

## 3. Target architecture

**One command** (`ptxdist images` in `ptxdist/`) produces the ISO. There is no
other production build path: Build A's scripts, the per-app scripts and every
committed binary are removed.

### 3.1 Layers

1. **Upstream base, pinned, not committed.** Tiny Core 16.0 `core.gz` and
   `vmlinuz` from the 16.0 archive, the 190 extensions from the 16.x mirror /
   the archived 16.0 ISO, all checksum-pinned (as in Build B). The base is
   consumed as released: rebuilding busybox/glibc/X.Org from source would
   replace working Tiny Core components and break the ABI of every Tiny Core
   extension users install with `tce-load`. This is the one deliberate
   binary input and it is documented as such.
2. **XXRI packages, built from source by PTXdist** with one rule each
   (section 3.3). Every XXRI file is installed through PTXdist's own
   `install_*` machinery, with explicit owner and mode — no `rootfs.meta`.
3. **Image rules**: a deterministic cpio composer merges Tiny Core's `core.gz`
   entries with PTXdist's `root/` (XXRI wins, a removal list deletes the
   Tiny Core files XXRI drops); the desktop caches are generated from the
   exact tree the device will see; `.tcz` packages are squashed from PTXdist
   package directories; the ISO assembly (GRUB 2.14, unchanged 2.6 layout)
   becomes a PTXdist image rule.

### 3.2 Filesystem layout: no merged /usr

PTXdist rewrites `/bin`, `/sbin` and `/lib` destinations to `/usr/…`. The
inspection shows **XXRI installs nothing into `/bin`, `/sbin` or `/lib`** (the
61 modified and 486 added files live in `/etc`, `/init`, `/opt`, `/root`,
`/usr/{bin,sbin,share}` and `/usr/local`), so PTXdist's install machinery can
own every XXRI file without the rewrite ever applying, while Tiny Core's real
`/bin`, `/sbin`, `/lib` come unchanged from `core.gz`. Tiny Core's layout is
kept; merged /usr is not introduced.

### 3.3 Packages

| Rule | Source | Replaces |
|---|---|---|
| `xxri-base` | `ptxdist/projectroot/` (XXRI's config, scripts, themes, icons, fonts, desktop files; the 61 modified Tiny Core files) | the committed `rootfs/` overlay |
| `xxri-settings`, `xxri-store`, `xxri-installer`, `xxri-dialog`, `xxri-control-center`, `xxri-launcher`, `xxri-media`, `xxri-power-menu`, `xxri-compositor`, `xxri-wctl`, `xxri-bootsplash` | `src/<app>/` (moved out of the initramfs), each app owning its data files | 12 committed binaries, 3 stale copies, 14 scripts |
| `xxri-flwm` | upstream flwm (tinycorelinux/flwm `196f61c`, sha256-pinned) + `patches/xxri-flwm/0001-xxri-decorations.patch`; builds `flwm` (XXRI) and `flwm-stock` (unpatched) | 2 binaries ×2 copies, `work/flwm-xxri` |
| `libfm` (1.3.2), `libexif`, `menu-cache` | upstream tarballs, sha256-pinned | 5 copied/ad-hoc libraries |
| `xxri-file` | `src/xxri-file/` | ad-hoc build-root binary |
| `xxri-glvnd`, `xxri-glapi` | libglvnd and Mesa (last release with shared glapi) sources; only the sonames XXRI shipped are installed | 4 Debian binaries |
| gdk-pixbuf | Tiny Core's `gdk-pixbuf2.tcz` (already on the boot list) + a generated `loaders.cache` | 15 Debian binaries; live = installed again |
| `xset` | dropped (unused) | 1 binary |
| `xxri-qt5` | Qt 5.15.17 (the last open-source 5.15; module tarballs, md5 from qt.io + sha256) — qtbase, qtdeclarative, qtwebchannel, qtlocation (positioning), qtsvg, qtx11extras, **qtwebengine** — cross-compiled with the OSELAS toolchain against Tiny Core's system libraries, private prefix `/usr/local/lib/xxri-browser` | Tiny Core's Qt, bundled ICU 70 and OpenSSL 1.1, the vendored `xxri-browser.tcz` |
| `xxri-browser` | `src/xxri-browser/browser` (build products deleted) | the vendored launcher binary |
| existing: `host-xxri-grub`, `xxri-grub-tools`, `host-xxri-xorriso`, `xxri-kernel`, `xxri-cde`, `xxri-tc-sysroot` | as in Build B | — |

### 3.4 Toolchain and flags

* OSELAS.Toolchain-2024.11.1 `i586-unknown-linux-gnu` (GCC 14.2.1, glibc
  2.40, binutils 2.43.1): the same glibc as Tiny Core 16, the same GCC 14
  libstdc++ ABI as Tiny Core's `gcc_libs`. A closure check (every NEEDED
  soname and versioned symbol of every ELF resolvable inside the image) runs
  on each build.
* **OS programs and libraries: `-march=i586 -mtune=generic -Os`**. `i586`
  because the base system below them is i486-compatible and these programs
  are GUI code where P6/SSE instructions buy nothing measurable; `-Os`
  because on the target class memory and cache size, not ALU throughput,
  are the limit (it is also Tiny Core's own choice). No `-ffast-math`, no
  LTO games.
* **Browser (Qt + engine): `-march=pentium4 -mtune=generic -mfpmath=sse`**:
  the engine needs SSE2 anyway (§2.1), every SSE2 CPU also has CMOV, and
  this is exactly the instruction set Chromium assumes; Chromium's own
  optimisation levels are kept.
* The final baseline claims are made only from the CPU-class runs (§5).

### 3.5 Browser: memory, size, startup — without dropping features

* Same feature set as today (§1.3). PTXdist's stock `qt5` rule turns off
  WebRTC, PDF/printing, spellcheck, proprietary codecs and Pepper, so the
  project carries its own rule; the stock rule's patches are reused.
* Chromium's bundled ICU (`icudtl.dat`, memory-mapped) replaces the bundled
  system ICU 70 libraries; QtNetwork links Tiny Core's OpenSSL 3 instead of
  the bundled, unmaintained OpenSSL 1.1.
* Private prefix: Qt no longer drops `libQt5*.so.5` into the shared
  `/usr/local/lib`, so it cannot collide with a Tiny Core Qt extension.
* Only the Qt plugins a windowed X11 browser loads; DevTools resources and
  non-en-US locales stay out (unchanged from today).
* Chromium's low-end-device mode already switches on automatically at
  ≤ 512 MiB RAM (`base/system/sys_info.cc`); it stays automatic, no flag
  forces it on bigger machines.
* Launcher: `/usr/local/bin/xxri-browser` checks for SSE2 and, if absent,
  tells the user why the browser cannot run on this CPU (instead of a V8
  `CHECK` crash in the renderer).

## 4. Implementation order (each step has a gate)

1. Plan + tools committed (this file, `isa-scan.py`, `qemu-boot-test.py --cpu`).
2. Start the long pole early: `xxri-qt5` rule and the engine build run in the
   background while the rest is restructured. Gate: `QtWebEngineProcess`
   links, `ldd`-closure against the Tiny Core sysroot is clean.
3. Base split: remove the 1636 Tiny Core files from git, move the 61 modified
   and the XXRI-added data files into `projectroot/`, write the cpio composer.
   Gate: the composed `core.gz` equals Build B's `core.gz` except for files
   XXRI now builds from source (`tools/cpio-diff.py`).
4. App rules, flwm, libfm/xxri-file, libexif/menu-cache, glvnd/glapi, drop
   gdk-pixbuf/xset; delete every committed binary and build product. Gate: no
   ELF file tracked in git (`git ls-files | xargs file | grep ELF` empty).
5. Caches generated in the image rule; ISO assembly as an image rule; retire
   Build A scripts. Gate: release gate (`validate-release.sh --boot`).
6. Browser app + tcz; launcher SSE2 check. Gate: browser renders the start
   page and a remote page in QEMU.
7. Reproducibility: two clean builds from an empty `platform-xxri`, identical
   ISO sha256.
8. QA matrix (§5) and the CPU-class investigation; docs updated with results.

## 5. QA matrix

The Build A/B matrix unchanged (`tools/qa/full-qa.sh`): release gate;
SeaBIOS + IA32 OVMF × CD/disk/USB to desktop; 64-bit OVMF refusal; RAM
256 MiB–16 GiB; media × RAM; Rufus ISO mode (MBR/BIOS, MBR/IA32, GPT/IA32);
Ventoy MBR/GPT × BIOS/IA32; installer ×5 with the installed disk booted alone.
Added for this migration:

* **Apps**: every XXRI program started on the booted ISO and screenshotted
  (Settings, Store, Control Center, Launcher, Media, Power menu, File,
  Installer, Browser start page + a remote HTTPS page, dock/compositor).
* **Memory**: `free` after desktop idle, and browser RSS (all processes) on
  the start page, vs 2.6 (Build A ISO).
* **Size**: ISO, `core.gz`, `xxri-browser.tcz` vs 2.6.
* **CPU classes** (QEMU TCG, whole desktop from the ISO): `pentium` (i586),
  `pentium2` (P6+MMX), `pentium3` (+SSE), and an SSE2 model — each with the
  ISA-sentinel plugin, which reports every executed instruction the emulated
  CPU lacks, mapped back to the binary it came from. Support for a class is
  claimed only if the desktop and apps run there with zero sentinel hits.
* **Browser on non-SSE2 CPUs**: the launcher message, verified on `pentium3`.

## 6. Risks and how they are handled

| Risk | Handling |
|---|---|
| Chromium 87 does not compile with GCC 14 | the stock PTXdist rule builds Qt WebEngine 5.15.17 with current GCC; missing-include fixes are added as patches under `patches/` and listed here |
| Engine build time/disk (~27 GB free) | module tarballs instead of the 3 GB `qt-everywhere` tree, `symbol_level=0`, out-of-tree build, `-j` bounded by RAM |
| A Tiny Core file XXRI removes or replaces breaks something | the cpio composer's removal list is explicit and reviewed; step 3 gate compares against Build B |
| Mesa glapi from an older Mesa is costly to build | fallback: pinned Debian `.deb` by URL + sha256 (still no binary in git), documented |
| AvenirNext fonts (`usr/local/share/fonts/xxri`) are commercial Linotype fonts | not a build problem; flagged for a licensing decision, not changed here |
