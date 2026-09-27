# XXRI OS Lite 2.6 — release status

Compatibility/stability release. Origin: Choutes' NEC VersaPro VK24MX
(i5-6300U, HD 520, 16 GB, Legacy/CSM) failed to boot 2.5 with
`Kernel panic - not syncing: VFS: Unable to mount root fs`, and then came up
with a blank graphical session once it did boot.

## Next candidate: unified boot (branch `xxri-2.6-boot`)

| | |
|---|---|
| ISO | `output/XXRI-Lite-2.6.iso` (265.7 MiB), sha256 `aa8137be…9362` at `cb23859`; two clean rebuilds byte-identical |
| Built by | `./tools/build-grub.sh` (once) + `./build-iso.sh` |
| Boots | BIOS/CSM and 32-bit UEFI: CD, dd'd USB/disk, Rufus ISO mode, Ventoy |
| Installer | GRUB 2.14 for BIOS (MBR/GPT) and 32-bit UEFI (GPT/MBR ESP); EXTLINUX removed |
| Details | `docs/boot-2.6.md`; hardware issues: `docs/hardware-issues-2.6.md` |
| QA | `tools/qa/full-qa.sh`: gate 0/0, 30/30 RAM, 12/12 media, Rufus 3/3, Ventoy 4/4, installer 5/5 (docs/boot-2.6.md) |
| Status | QEMU-verified only — **needs real-hardware runs before it replaces RC1** |

* **B1 (UEFI)** — addressed for 32-bit UEFI (`BOOTIA32.EFI`, GRUB i386-efi).
  64-bit-only UEFI remains unsupported by design (i686 kernel); an x64 GRUB
  experiment works on old firmware and crashes on current EDK2, so it is not
  shipped. 64-bit machines use CSM/Legacy until the x86-64 release.
* **B2 (installed system on EXTLINUX)** — closed: the installer writes GRUB
  2.14 (same build as the ISO) and EXTLINUX/syslinux are gone from the system.
* The NEC VK24MX Legacy/CSM failure is **not** fixed by this candidate: its
  BIOS path is RC1's. See `docs/hardware-issues-2.6.md` for the next data.

## RC1 candidate (as sent to the NEC tester)

| | |
|---|---|
| ISO | `output/XXRI-Lite-2.6-rc.iso` (268 MiB) |
| Built by | `./build-iso.sh` |
| Gate | `./validate-release.sh output/XXRI-Lite-2.6-rc.iso --boot` → 0 failures, 0 warnings |
| Status | **with Choutes for real-hardware validation** — frozen, do not rebuild |

Verified in QEMU only: boots through GRUB from a hybrid disk and from a USB 3
stick, reaches userspace at 1 GiB and 16 GiB, splash and desktop both render.
Neither fix can be reproduced locally, so the NEC run is the real test.

## Fixed in this candidate

1. **Initrd handoff.** BIOS boot moved from ISOLINUX 4.05 (2011) to GRUB 2.
   ISOLINUX always places the initrd as high as `initrd_addr_max` allows, i.e.
   just under 2 GB on any machine with ≥ 2 GB RAM; on the VK24MX those bytes
   came back damaged (1 of 259 64 KB blocks) and core.gz could not unpack.
   GRUB 2.14's i386-pc loader clamps `initrd_addr_max` to its own 0x37FFFFFF
   and places the initrd top-down below that in free RAM per the firmware map.
   Measured on the RC: 0x35F99000-0x36FC3FFF for the current core.gz on every
   QEMU size from 1 to 16 GiB (it only moves at 512 MiB, where it drops to the
   top of RAM). So this is NOT machine-adaptive placement - it is a fixed
   window derived from an upstream constant, and it had never been exercised on
   the VK24MX before RC1. The kernel ships **unpatched** (`initrd_addr_max` still 0x7FFFFFFF) and XXRI
   adds **no initrd or memory ceiling of its own**.
2. **Graphics.** `rootfs/etc/skel/.xsession` waited for `/dev/dri/card0` to
   exist. `drm_minor_register()` calls `device_add()` — which is what makes
   udev create the node — and publishes the minor for lookup only afterwards,
   so an `open()` in that window returns ENODEV and X.Org dies with
   "no screens found". It now waits for a `/sys/class/drm/card*-*/status`
   connector, which `drm_dev_register()` creates only after both minors are
   published, and no longer falls back to Xvesa while a DRM device is live
   (Xvesa drives the card through the VESA BIOS while i915 owns the display —
   that was the blank screen).
3. **Pre-release gate.** `./validate-release.sh` — gzip/cpio integrity, `/init`
   present, loader-referenced files exist and are openable by the loader, build
   junk, ISO size, then QEMU boots (direct kernel+initrd at 1 GiB and 16 GiB,
   plus the ISO through its own loader as disk and as USB 3).
4. **Build hygiene.** `rootfs/.kilo` (a 56 MiB nested worktree) is no longer
   swept into core.gz.

## Release blockers — must close before 2.6 final

### B1. UEFI boot is missing from the candidate

The candidate is BIOS/CSM only. Add UEFI while preserving BIOS/hybrid boot.

There is a hard architectural constraint, read from the shipped kernel:

```
xloadflags = 0x0004   XLF_KERNEL_64 no | XLF_EFI_HANDOVER_32 yes | XLF_EFI_HANDOVER_64 no
```

EFI handover requires matching bitness, so **a 64-bit UEFI machine cannot boot
this i686 kernel at all** — no bootloader change fixes that.

> Correction (2026-09-26): that holds for the kernel's own EFI stub. Upstream
> GRUB's `x86_64-efi` loader falls back to the 32-bit boot protocol for a
> non-native kernel and does boot it on older firmware (EDK2 2020.11: desktop,
> no EFI runtime services) - but its relocator page-faults on current firmware
> with memory protection (EDK2 2026.05). Firmware-dependent, so still not a
> release path; see docs/boot-2.6.md. While 2.6 stays
i686 (goal D), UEFI means:

* **Buildable now:** `bootia32.efi` (GRUB `i386-efi`, present on the build host)
  in an EFI system partition on the same hybrid ISO. Covers 32-bit UEFI
  machines. BIOS/hybrid boot unchanged.
* **Not possible in 2.6:** 64-bit UEFI. Machines like the VK24MX must use
  Legacy/CSM until a 64-bit kernel ships. **Decision needed:** ship ia32-only
  UEFI for 2.6 and defer x86_64 UEFI to 2.7, or revisit goal D.

### B2. The installed system still boots via EXTLINUX 6.03

`rootfs/usr/sbin/xxri-install-backend` → `install_bootloader()` writes
`extlinux.conf` and dd's syslinux's `mbr.bin`. EXTLINUX uses the same
"as high as possible" initrd policy as ISOLINUX, so **the VK24MX failure can
return the moment a user installs to disk** — the live ISO would be fixed and
the installed system would not. Not acceptable for a release whose purpose is
to fix this class of issue permanently.

Plan: ship a prebuilt GRUB i386-pc `boot.img` + `core.img` (built with
`grub-mkimage` at ISO build time, so no GRUB tooling is needed inside XXRI),
have the installer write them plus `/boot/grub/grub.cfg` and the modules, and
verify by installing to a disk image in QEMU and booting the result. Same
rules: stock kernel, no ceilings.

## Constraints to hold

* Stock kernel. No artificial initrd or memory-address ceilings. The only real
  limit is the genuine one: a non-PAE HIGHMEM4G i686 kernel tops out at 4 GB,
  so the NEC's 16 GB shows as ~3.2 GB. Preserve it, do not shrink it.
* Keep the ISO near 268 MiB. Never repack `rootfs/.kilo`.
* Re-run the gate after every change:
  `./validate-release.sh output/<iso> --boot`
* No further tester-facing diagnostic ISOs unless a new failure appears that
  cannot be diagnosed locally. The superseded ones are in `debug/vk24mx-diag/`.

## Still unknown

The VK24MX corruption was observed with the initrd near 2 GB and absent below
768 MB. Those two runs differ in **two** ways — the physical address, and
whether the kernel relocates the initrd out of high memory — and the run that
would separate them was never made. GRUB moves the initrd out of the ~2 GiB
region, but into a fixed window (~864-880 MiB) that has not been tested on the
NEC - see the RC1 section below. This is now the open question, not a settled one.

## RC1 on the VK24MX — first real-hardware result (2026-09-23)

Choutes' RC still panics: `VFS: Unable to mount root fs on "" or
unknown-block(1,0)`. Analysis of the frozen RC (sha256 6f627ff2...), no rebuild:

* The RC's `grub.cfg` and files are correct. `/proc/cmdline` is
  `BOOT_IMAGE=/boot/vmlinuz quiet loglevel=3 vt.global_cursor_default=0
  consoleblank=0 cde` (GRUB adds BOOT_IMAGE=, never initrd=).
* **`(1,0)` is a GRUB fingerprint that means GRUB's `initrd` command succeeded.**
  GRUB writes `root_dev = 0x0100` (= /dev/ram0) only after an initrd is loaded.
  No/failed `initrd` -> `(0,0)`; invalid-but-readable initrd -> `"" or (1,0)`.
  ISOLINUX never sets root_dev, so the 2.5 panics were always `(0,0)`
  whether an initrd was handed over or not - the two numbers are not comparable
  across loaders.
* So the failure is after hand-off, same class as 2.5: the kernel started with
  an initrd descriptor and ended with no /init. At `loglevel=3` a kernel-side
  discard (INITRD: ... not a memory region) and a damaged initrd
  (rootfs image is not initramfs) print the identical screen.
* Not yet known: discarded vs damaged, and whether the ~864-880 MiB window
  GRUB chose is itself bad on this machine. Neither the ~2 GiB (bad) nor the
  <768 MiB (good) run tested that window.
