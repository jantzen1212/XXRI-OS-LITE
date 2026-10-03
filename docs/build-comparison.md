# XXRI OS Lite 2.6 — Build A (existing scripts) vs Build B (PTXdist)

Written before the switch to the PTXdist-native build
(`ptxdist-migration-plan.md`), which replaced Build A after all.

Both builds start from the same commit (`cb23859`, branch `xxri-2.6-boot`;
Build B adds only `ptxdist/` on `xxri-2.6-ptxdist`) and produce the same
unified BIOS + IA32 UEFI ISO layout, because Build B calls the same
`build-iso.sh` for the final assembly. The comparison is therefore about
orchestration, toolchain, input pinning and packaging — not about two
different boot designs.

## What each build is

| | Build A | Build B |
|---|---|---|
| Orchestrator | shell: `tools/build-grub.sh`, `build-iso.sh` | PTXdist 2026.09.0 project (`ptxdist/`) |
| Compiler for XXRI's own programs | none — 11 **prebuilt binaries committed in git**, made once per app by per-app scripts with the host's `gcc -m32` (Arch GCC of the day); `xxri-dialog` has no build script at all | OSELAS.Toolchain-2024.11.1 `i586-unknown-linux-gnu` (GCC 14.2.1, glibc 2.40, binutils 2.43.1), built from source; all 11 programs compiled every build |
| GRUB 2.14 (boot images, modules) | host GCC 16 / binutils 2.47 | host GCC 16 / binutils 2.47 (PTXdist host rule) — **byte-identical to A** |
| GRUB utilities for the installer | native i686 build inside an ad-hoc 1.9 GB build root (core.gz + Tiny Core compiletc, not in the repo) | cross-compiled with the OSELAS toolchain |
| Kernel | `work/iso/boot/vmlinuz` (git-ignored, unpinned) | Tiny Core 16.0 archive URL, sha256-pinned — same bytes |
| 196 extensions | `work/iso/cde/optional` (git-ignored, unpinned) | 191 from the 16.x mirror + 4 from the archived TinyCore-16.0.iso, md5-pinned, + 1 vendored (`xxri-browser.tcz`, sha256) — same bytes |
| Libraries to compile against | the ad-hoc build root | 111 md5-pinned Tiny Core 16 packages (`config/tc-sysroot.manifest`) |
| core.gz | `rootfs/` + `rootfs.meta`, packed by build-iso.sh | same tree with the rebuilt programs laid over it, packed by build-iso.sh (see "rootfs generation") |

## Results

| # | Criterion | Build A | Build B |
|---|---|---|---|
| 1 | Builds successfully | yes | yes (after the fixes listed below) |
| 2 | ISO size | 278,568,960 B (265.7 MiB) | 278,546,432 B (265.6 MiB) |
| 3 | Build time, clean, 8-core i7-9700 | **236–270 s** (GRUB ×3 161 s, GRUB utilities 70 s, ISO 5 s); excludes the one-off i686 build root | **687–718 s** (warm source cache); plus the toolchain once: ~45–60 min, 22 GB build tree, 3 GB installed |
| 4 | Reproducibility | clean builds byte-identical (`aa8137be…` twice at `cb23859`, `6af901ad…` twice before) | 3 clean builds byte-identical (`2b11296c…`); PTXdist fixes timestamps itself |
| 4b | Reproducible **from the repository alone** | no — kernel, 196 extensions and the i686 build root live in git-ignored `work/`; 4 of the extensions are no longer on the Tiny Core mirror (rebuilt upstream in place after 16.0) | yes, except the vendored browser binary: every input is fetched by URL and checked |
| 5 | Boot loader | GRUB 2.14 i386-pc + i386-efi | **identical files** (all 588 modules) |
| 6 | Kernel / initramfs | TC 6.12.11 / 2208 entries | same kernel; 2208 entries, same names/modes/mtimes; **only the 11 rebuilt programs differ** |
| 7 | Root filesystem | Tiny Core 16 base + XXRI | same |
| 8 | Packages / dependencies | 196 extensions | same 196, byte-identical; dependency closures declared in files |
| 9 | Hardware compatibility | XXRI programs need a **P6-class CPU** (CMOV: installer 46, settings 27, static boot splash 580 instructions) | XXRI programs are **i586** (no CMOV) |
| 10–14 | QEMU BIOS / UEFI / IA32 UEFI / GPT+MBR / desktop | all pass (QA below) | all pass (QA below) |
| 15 | Maintainability | ~840 lines of shell/Python; hidden manual steps (per-app builds, build root, `work/` inputs) | ~500 lines of rules/scripts + kconfig; everything declared; but a second PTXdist version for the toolchain and host-compiler workarounds |
| 16 | Complexity | low; readable top to bottom | high: PTXdist concepts (stages, host/target/cross, kconfig, permission records, image rules), two projects |
| 17 | Adding/updating packages | Tiny Core extension: `work/tcz-fetch.sh` + edit `onboot.lst` (not recorded in git) | Tiny Core extension: one manifest line + `.dep` (recorded); source-built package: a PTXdist rule |
| 18 | Rebuilding the OS from source | not possible: XXRI programs are rebuilt by hand; the base system is Tiny Core binaries | XXRI programs, GRUB and the toolchain from source; the Tiny Core base, kernel modules and extensions are still Tiny Core binaries (see limits) |
| 19 | Regressions vs RC1 / vs A | none found; gains 32-bit UEFI, which RC1 lacks | none vs A: installed desktops differ only in the clock (29×10 px) |

### Problems Build B had to solve (none needed in Build A)

1. **The pinned 2024 toolchain does not bootstrap on a 2026 host as shipped.**
   host `m4` and gdb's readline fail under GCC 16's C23 default, GCC 14's
   `libcody` under g++ 16's C++20 default: host compilers wrapped as
   `gcc -std=gnu17` / `g++ -std=gnu++17`; cross-gdb disabled. It also needs
   PTXdist 2024.11.0 alongside 2026.09.0.
2. **PTXdist's own rules did not fit:** its `grub` rule is GRUB Legacy 0.97;
   there is no xorriso rule; its host squashfs-tools is built without xz.
3. **PTXdist enforces merged /usr.** `ptxd_install_setup()` rewrites every
   `/bin`, `/sbin`, `/lib` destination to `/usr/…` (and the `USR_ROOTFS`
   default adds the symlinks). Tiny Core's base uses real `/bin`, `/sbin`,
   `/lib`, so PTXdist's rootfs generation and `image-root-cpio` produced a
   core.gz without `/bin/busybox` or `/sbin/init` — the release gate caught
   it. Build B therefore packs core.gz with build-iso.sh; a PTXdist-native
   rootfs would mean converting XXRI to merged /usr (only one name collides,
   `getopt`, but every Tiny Core script and extension would need testing).
4. PTXdist turns every installed file into a make prerequisite; the
   prebuilt binaries next to their sources in `rootfs/usr/local/src` then
   matched make's built-in `%: %.c` rule.
5. GTK's pkg-config chain reaches the GL/EGL/DRM stack; Tiny Core keeps zlib
   and libuuid in the base (their dev files in `*_base-dev`).

### What neither build rebuilds

The Tiny Core base (busybox, glibc, udev…), X.Org, GTK and the other 190-odd
extensions, the kernel and its ~60 `*-KERNEL.tcz` module packs, and
`xxri-browser.tcz` (Qt WebEngine 5.15). PTXdist has rules for most of these,
but rebuilding them would replace working Tiny Core components; a kernel
rebuild would also require regenerating every module pack (vermagic and
symbol CRCs). Build B pins them instead.

## QA (same harness, each ISO through its real firmware and loader, idle host)

`tools/qa/full-qa.sh` against Build A `aa8137be…` and Build B `2b11296c…`:

| Suite | Build A | Build B |
|---|---|---|
| Release gate (`validate-release.sh --boot`) | 0 failures, 0 warnings | 0 failures, 0 warnings |
| SeaBIOS + IA32 OVMF × CD / disk / USB → desktop | 6/6 (userspace 19.6–31.6 s) | 6/6 (19.6–30.1 s) |
| 64-bit OVMF | refused (documented) | refused (documented) |
| RAM 256 MiB–16 GiB BIOS, 256 MiB–8 GiB IA32 | 30/30 | 30/30 |
| Media × RAM | 12/12 | 12/12 |
| Rufus ISO mode: MBR/BIOS, MBR/IA32, GPT/IA32 | 3/3 | 3/3 ¹ |
| Ventoy MBR + GPT × BIOS + IA32 | 4/4 | 4/4 |
| Installer, 5 scenarios, installed disk booted alone | 5/5 (+ IA32 auto disk on BIOS) | 5/5 (+ same) |
| Installed desktop, pixel diff A vs B | — | 47 px (the clock) |

¹ The first Build B run failed Rufus MBR/BIOS because the emulation script
looked for GRUB's `boot.img` in `../../build/grub`, which only exists in Build
A's tree; it now takes `boot.img` from the ISO and checks that it was
written. Re-run: both builds boot (GRUB 0.6 s, userspace 23.1 s).

## Recommendation

**Keep Build A (the existing shell build) as the 2.6 release build.** On the
actual results the two produce the same system: identical boot loader bits,
identical kernel and extension set, identical root filesystem except eleven
recompiled programs that behave identically, and the same QA outcome. Build A
does that in a quarter of the time, with no second PTXdist version, no 22 GB
toolchain tree, no host-compiler workarounds, and with nothing between the
reader and what the scripts do.

PTXdist's headline strengths turned out to be either already achieved in
Build A this cycle (byte-reproducible ISOs, `rootfs.meta`) or blocked for this
system: its rootfs generation — the core of what it offers — cannot express
Tiny Core's classic `/bin` layout because PTXdist 2026 hard-codes merged /usr,
and it has no current GRUB or xorriso rules. What Build B proved,
Build A should adopt without PTXdist:

1. **Pin the inputs.** Move `ptxdist/config/cde/` (manifest with md5 and
   origin, `.dep` files, boot lists) and `scripts/xxri-fetch-cde.sh` into
   Build A, and archive the four Tiny Core 16.0 packages upstream has since
   rebuilt. Today Build A cannot be reproduced from the repository.
2. **Compile the XXRI programs in the build, for i586.** The committed
   binaries need a P6 CPU (CMOV), contradicting the README's K6-2 minimum;
   Build B's i586 builds run pixel-identically. The OSELAS toolchain (or any
   pinned i586 compiler) can be used as a plain cross compiler from a script,
   and `xxri-dialog` finally gets a build recipe.

Revisit PTXdist for the x86-64 release (2.7) only if XXRI moves to building
its base system from source; that is where its package, dependency and
toolchain management would pay for their complexity — and where adopting
merged /usr would be a deliberate, tested change rather than a side effect.
