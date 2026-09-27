# XXRI OS Lite 2.6 — boot architecture

One ISO, `XXRI-Lite-2.6.iso`, boots:

| Firmware | Medium | Loader | Status |
|---|---|---|---|
| BIOS / legacy / UEFI-CSM | CD/DVD (El Torito) | GRUB 2.14 `i386-pc` | QEMU-verified |
| BIOS / legacy / UEFI-CSM | USB stick or disk written with dd/Etcher/Rufus-DD (hybrid MBR) | GRUB 2.14 `i386-pc` | QEMU-verified |
| BIOS / legacy / UEFI-CSM | Rufus "ISO Image mode", MBR | Rufus's GRUB 2.14 core + our modules/config | QEMU-verified (emulated) |
| 32-bit (IA32) UEFI | CD/DVD (El Torito EFI) | GRUB 2.14 `i386-efi`, `BOOTIA32.EFI` | QEMU-verified |
| 32-bit (IA32) UEFI | dd'd USB/disk (MBR partition 2, type 0xEF) | same | QEMU-verified |
| 32-bit (IA32) UEFI | Rufus "ISO Image mode", MBR or GPT | same (`/EFI/BOOT` in the file tree) | QEMU-verified (emulated) |
| BIOS and IA32 UEFI | Ventoy 1.1.17 | Ventoy's GRUB + Ventoy's Tiny Core hook | QEMU-verified |
| **64-bit-only (x64) UEFI** | any | — | **not supported in 2.6** (see below) |

The installed system uses the same GRUB build (see "Installed system").

## Upstream components and exact versions

* **GNU GRUB 2.14**, the upstream release tarball `grub-2.14.tar.xz`
  (sha256 `bc8d3c73535b8838d8c8e2654d73edc4e6ae8c8acdb45d5df5dc9a1547446d43`,
  signature by Daniel Kiper, key `BE5C23209ACDDACEB20DB0A28C8189F1988C2166`,
  verified against the GNU keyring by `tools/build-grub.sh`). **No patches.**
  Built for `i386-pc`, `i386-efi` (and `x86_64-efi`, only for the experiment
  below) on the host, and as i686 userland (`grub-install`, `grub-mkimage`,
  `grub-bios-setup`, `grub-probe`, `grub-editenv`) inside an i686 root made
  from XXRI's own `core.gz` + Tiny Core `compiletc`, so the tools link against
  XXRI's glibc 2.40 (highest symbol version used: `GLIBC_2.38`).
* One configure-time setting differs from a plain `./configure`:
  `ax_cv_check_ldflags___Wl___image_base_0x400000=no`. GRUB 2.14's configure
  switches to `-Wl,--image-base` whenever the linker accepts it (a change made
  for LLVM lld). GNU ld ≥ 2.44 accepts it but counts the ELF headers into the
  first segment, so `i386-pc/kernel.img` starts at 0x9074 instead of 0x9000
  and `grub-mkimage` rejects it ("kernel.img is miscompiled"). Arch Linux
  reverts upstream commit `1a5417f3` for the same reason; XXRI seeds the
  configure cache instead, which selects GRUB's traditional `-Ttext` without
  touching the source.
* **Linux 6.12.11-tinycore**, the unmodified Tiny Core 16.x kernel
  (`initrd_addr_max` 0x7FFFFFFF, EFI stub with 32-bit handover, no 64-bit
  support: `xloadflags` 0x0004). XXRI still adds **no** `mem=`, no initrd
  address and no other memory ceiling anywhere.
* Installer tools from the Tiny Core 16.x repository, unmodified:
  `gdisk` 1.0.4 (sgdisk), `dosfstools` 3.0.3, `efibootmgr` 16, `efivar` 37.

### Debian/Ubuntu/distribution patches

None are used, and none were needed. What distributions add on top of
upstream GRUB is chiefly Secure Boot machinery (shim lock verification,
`linuxefi`, signed monolithic images) and `grub-mkconfig` integration
(Arch's three patches in 2.14 touch only `grub-mkconfig` scripts). XXRI ships
unsigned loaders and writes its `grub.cfg` directly, so neither applies.
Upstream 2.14 already contains what distributions used to patch in for x86
UEFI: its `linux` command boots EFI-stub kernels through the kernel's own stub
with the LoadFile2 initrd protocol (`grub-core/loader/efi/linux.c`).

## ISO layout

```
MBR (GRUB boot_hybrid.img)            partition 1  0xCD  active   ISO 9660 (from LBA 64)
                                      partition 2  0xEF           FAT ESP image (appended)
El Torito  BIOS  boot/grub/i386-pc/eltorito.img   (cdboot + GRUB core)
           UEFI  --> partition 2 (the same FAT image)

/.disk/xxri-lite-2.6-<id>   marker every core image searches for
/.disk/info
/boot/vmlinuz  /boot/core.gz
/boot/grub/grub.cfg                   shared by BIOS and UEFI GRUB
/boot/grub/i386-pc/*.mod  /boot/grub/i386-efi/*.mod  (one 2.14 build)
/boot/grub/COPYING  /boot/grub/SOURCE.txt        GPLv3 licence + source notice
/EFI/BOOT/BOOTIA32.EFI                identical to the copy inside the ESP
/cde/...                              extensions (+ xxri-grub, gdisk, dosfstools,
                                      efibootmgr, efivar for the installer)
```

This is the layout upstream `grub-mkrescue` produces (MBR-only hybrid, no
GPT), plus two things it does not do:

* **Root discovery by marker file.** The early config embedded in
  `eltorito.img` and `BOOTIA32.EFI` is
  `search --no-floppy --set=root --file /.disk/xxri-lite-2.6-<id>` followed by
  `set prefix=($root)/boot/grub`. `grub-mkrescue` searches by the ISO 9660
  UUID, which disappears when a tool copies the files to FAT32 (Rufus ISO
  mode). `<id>` is derived from vmlinuz + core.gz, so two different XXRI
  builds (or an installed XXRI disk) never capture each other's boot.
* **`/EFI/BOOT/BOOTIA32.EFI` in the ISO 9660 tree** as well as inside the ESP
  image, so file-copy writers still produce a UEFI-bootable stick.

Why MBR and not GPT for the hybrid: a type 0xEF MBR partition is found by
UEFI firmware on removable media; an active MBR partition keeps old BIOSes
that insist on one happy; and it avoids "hybrid GPT/MBR" layouts, which the
UEFI specification says to treat as MBR anyway. It is also the layout the 2.6
RC already proved on the NEC VK24MX (GRUB loaded and handed over the initrd).
GPT on USB is covered by Rufus/Ventoy and by the installer.

## How the initrd is placed (measured, QEMU)

| Path | Who places `core.gz` | Policy | 256 MiB | 1–16 GiB |
|---|---|---|---|---|
| BIOS (`i386-pc`) | GRUB `loader/i386/linux.c` | highest free RAM below min(`initrd_addr_max`, 0x37FFFFFF) − 64 KiB, from the firmware E820 map | top of RAM (0x0E97D000), used in place | 0x3601F000 – 0x37006FFF in the final build; the kernel copies it to 0x35037000 because it crosses the lowmem limit |
| IA32 UEFI (`i386-efi`) | the kernel's EFI stub, asked by GRUB via LoadFile2 | UEFI `AllocatePages` below 4 GiB (`initrd_addr_max` is only a soft limit in 6.12's stub), then the kernel copies it into lowmem | 0x0CD6B000, used in place | 0x7CD6B000 at 2 GiB, 0xBCD6B000 at ≥ 3 GiB → copied to 0x36016000 |
| x64 UEFI (experiment) | GRUB `loader/i386/linux.c` (fallback: the PE header is not native) | as BIOS, from the EFI memory map | — | as BIOS |

This i686 kernel maps **879 MiB** of low memory (it logs `879MB LOWMEM
available`, i.e. up to about 0x36FFE000). GRUB's upstream cap, 0x37FFFFFF, is
the classic 896 MiB figure, and GRUB's own heap occupies the top of that
window, so where a ~17 MB initrd ends relative to 879 MiB depends on the
build: the first 2.6 build and RC1 (0x35F99000–0x36FC3FFF) ended below it and
were used in place; the final build ends 28 KiB above it and is copied down
by the kernel's `relocate_initrd()` during early setup. Both are normal
upstream paths and both boot at every RAM size tested.

0x37FFFFFF (896 MiB) is not an XXRI choice: it is GRUB's upstream constant
and matches the i686 kernel's directly-mapped lowmem, the region the kernel
can use in place. On UEFI the address comes from the firmware's own
allocator, and firmware DMA stops at `ExitBootServices()` before the kernel
proper runs.

## 64-bit UEFI

Not supported by 2.6, by design and by architecture:

* 64-bit UEFI firmware only executes x86-64 EFI applications
  (`\EFI\BOOT\BOOTX64.EFI`). `BOOTIA32.EFI` is never run there — measured:
  64-bit OVMF reports *"No bootable option or device was found."*
* The i686 kernel's own EFI stub supports only 32-bit firmware
  (`xloadflags`: handover 32 yes, 64 no).
* **Experiment (not shipped):** `build-iso.sh --with-x64-efi` adds a GRUB
  `x86_64-efi` `BOOTX64.EFI`. For a kernel whose PE header is not native,
  upstream GRUB falls back to the legacy 32-bit boot protocol after
  `ExitBootServices()`. Results:
  * EDK2 **2020.11** (no DXE memory protection): boots to the desktop; the
    kernel reports *"No EFI runtime due to 32/64-bit mismatch"* and still
    finds ACPI through the EFI configuration table and uses efifb.
  * EDK2 **2026.05** (memory protection on, like current firmware): GRUB's own
    relocator trampoline takes a **page fault** before the kernel starts
    (write to a read-only page). Upstream master has a candidate fix not in
    2.14 ("relocator: Switch to own page table while moving chunks",
    `6898fcf7`), unverified here.
  So this path depends on the firmware generation — acceptable for an
  experiment, not for a release. 64-bit UEFI machines should use CSM/Legacy
  with 2.6; the proper answer is the x86-64 release (2.7), whose kernel boots
  natively through its own EFI stub.

## Secure Boot

Not supported. The loaders are unsigned; Secure Boot must be disabled in
firmware (on IA32 tablets it is often enabled by default). Supporting it
would require shim + a Microsoft-signed first stage + signed GRUB and kernel.

## Installed system

`xxri-install-backend` now installs GRUB 2.14 with `grub-install` (the tool
Debian/Ubuntu installers also use) from `xxri-grub.tcz`, loaded on demand from
the live medium. EXTLINUX 6.03 and syslinux's `mbr.bin` are removed from the
system: they placed the initrd as high as `initrd_addr_max` allows (the 2.5
VK24MX failure) and could not boot GPT disks or UEFI.

| Machine booted XXRI via | Automatic layout | Boot loader(s) written |
|---|---|---|
| BIOS / CSM (disk ≤ 2 TiB) | MBR, one active ext4 partition from 1 MiB | GRUB `i386-pc` in the MBR + gap |
| BIOS / CSM (disk > 2 TiB) | GPT as below | `i386-pc` in the BIOS boot partition |
| IA32 UEFI | GPT: 1 MiB BIOS boot (EF02) + 260 MiB ESP (FAT32, ≥ 65525 clusters also on 4Kn disks) + ext4 | `i386-efi` (`EFI/XXRI/grubia32.efi` + NVRAM entry when efivarfs is writable, and always `EFI/BOOT/BOOTIA32.EFI`) **and** `i386-pc`, so the disk also starts on legacy firmware |
| x64 UEFI | refused with an explanation | — |

Manual installs into an existing partition: BIOS needs either an MBR disk
(the core image fits even a legacy 63-sector gap: 59 sectors) or a GPT disk
with a BIOS boot partition; IA32 UEFI needs an ESP on the same disk (GPT
EF00 or MBR 0xEF). An existing `EFI/BOOT/BOOTIA32.EFI` of another OS is not
replaced. The installed `grub.cfg` finds the root by filesystem UUID and uses
the live ISO's boot codes minus `cde`; `gfxpayload` replaces 2.5's `vga=791`
and also works on UEFI.

## Compatibility fixes outside GRUB

* **Ventoy**: Ventoy's Tiny Core hook edits `/etc/init.d/tc-config`. XXRI's
  `rcS` called `xxri-config` directly, bypassing the edit, so under Ventoy the
  kernel and initrd booted but the extensions (`cde`) were never found and
  the desktop never started. `rcS` now calls the upstream Tiny Core entry
  point `tc-config` (a symlink to `xxri-config`, so normal boots are
  unchanged).
* **Reproducible core.gz**: git cannot store empty directories, setuid/sticky
  bits or modes; a `core.gz` packed from a clean checkout lacked `/proc`,
  `/sys`, `/dev/pts`, `/home`, `/mnt`, `/run`, had no setuid `busybox.suid`/
  `sudo` and no sticky `/tmp`. `rootfs.meta` (from `tools/rootfs-meta.py
  capture`) restores them; the result unpacks to exactly the RC1 tree
  (`tools/cpio-diff.py`: 0 differences in 2214 entries).

## Testing

Everything boots through real firmware and the real loader; nothing uses
`-kernel/-initrd` except the gate's separate core.gz integrity check.

* `tools/qemu-boot-test.py` — one boot: SeaBIOS / 32-bit OVMF (EDK2 2025.02,
  Debian `ovmf-ia32`) / 64-bit OVMF; CD, disk or USB; userspace is proven by
  the guest's own DHCP request (NIC option ROMs disabled so firmware PXE
  cannot fake it); screenshots; optional edit of the entry in GRUB's own
  editor to add a serial console.
* `tools/boot-matrix.py` — RAM sizes and media.
* `tools/qa/install-test.py` — installs from the live ISO and boots the result
  alone; `tools/qa/rufus-emulate.sh`, `tools/qa/ventoy-emulate.sh`.
* `validate-release.sh ISO --boot` — the release gate.

## Results (QEMU, final ISO `aa8137be…`, idle host, 2026-09-27)

Every boot goes through the real firmware and GRUB; "desktop" = the live
Welcome wizard or, for installed disks, the full desktop, by screenshot.

| Suite | Result |
|---|---|
| Release gate (`validate-release.sh --boot`) | 0 failures, 0 warnings |
| SeaBIOS + IA32 OVMF × CD / dd'd disk / USB (xHCI) | 6/6 desktop; userspace 19–32 s |
| RAM, BIOS: 256 MiB, 512 MiB, 1, 2, 3, 4, 8, 16 GiB | 16/16 (default entry + serial-console run each) |
| RAM, IA32 UEFI: 256 MiB … 8 GiB | 14/14 |
| Media × RAM (1, 4 GiB) | 12/12 |
| Rufus ISO mode (emulated, Rufus's own GRUB 2.14 core): MBR/BIOS, MBR/IA32, GPT/IA32 | 3/3 (GPT/BIOS correctly not bootable — Rufus does not offer it) |
| Ventoy 1.1.17: MBR and GPT sticks × BIOS and IA32 UEFI | 4/4 |
| Installer: BIOS auto (MBR) · IA32 auto (GPT) · BIOS into GPT+EF02 · IA32 into MBR+0xEF ESP · BIOS into a 2.5-style sector-63 disk | 5/5 installed; each disk boots alone to the desktop; the IA32 auto disk also boots on BIOS |
| 64-bit OVMF | refuses the medium ("No bootable option") — documented limitation |
| RC1 for comparison | BIOS yes, IA32 UEFI no |

**Not verified** (needs real hardware): any physical machine (NEC VK24MX in
particular), real Rufus and Ventoy on Windows/physical sticks, USB-3-only
IA32 tablets (Bay Trail/Cherry Trail) with eMMC and touch, Secure Boot
(unsupported), 4Kn disks, NVMe, firmware that only boots GPT media, and
Apple IA32 EFI Macs.

## Licensing

GRUB is GPLv3+. The ISO carries GRUB object code (core images, modules, and
the utilities in `xxri-grub.tcz`), so each release must be distributed with
the Corresponding Source: `build-iso.sh` writes `output/sources/`
(`grub-2.14.tar.xz` + signature, and the scripts that configured, built and
assembled the images) — publish it at the same place as the ISO (GPLv3 §6d).
`COPYING` and `SOURCE.txt` are on the ISO in `/boot/grub/` and in the
extension. gptfdisk, dosfstools and efibootmgr are GPLv2 (efivar LGPL-2.1),
redistributed unmodified from Tiny Core like the rest of the extension set,
whose existing source obligations (kernel, busybox, etc.) are unchanged.
