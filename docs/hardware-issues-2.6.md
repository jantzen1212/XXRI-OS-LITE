# XXRI OS Lite 2.6 — open real-hardware issues

Two machines, investigated separately. Nothing here was reproducible in QEMU;
QEMU results below only rule mechanisms in or out.

## 1. NEC VersaPro VK24M/X (i5-6300U, HD 520, 16 GB, Legacy/CSM)

### What the existing data shows

| Run | Loader | Where the initrd sat | Kernel path | Result |
|---|---|---|---|---|
| 2.5 diag, entry 1 | ISOLINUX 4.05 | ~0x7EE25000–0x7FFFFFFF (just below 2 GiB) | relocated into lowmem | **1 of 259 64 KB blocks differed** from core.gz → no /init → VFS panic |
| 2.5 diag, entry 3 | ISOLINUX 4.05, kernel copy with `initrd_addr_max` lowered | < 768 MiB | used in place | 259/259 identical, booted to userspace |
| 2.5 diag, entry 2 (~1 GiB) | — | — | — | **never run** |
| 2.6 RC1 | GRUB 2.14 i386-pc | below 0x37FFFFFF; in QEMU 0x35F99000–0x36FC3FFF at every size ≥ 1 GiB | used in place (ends below the kernel's 879 MiB lowmem limit) | panic `"" or unknown-block(1,0)`: GRUB's `initrd` succeeded (it sets root_dev 0x0100 only then); the kernel still found no /init |

The medium and the read path were the same in the two 2.5 runs, yet only one
was damaged: the bytes were damaged *where they were parked*, not while being
read. The NEC's own e820 map and the bad-block addresses exist only in the
tester's photos; they were never transcribed.

### What 2.6 changes for this machine — and what it does not

* **Unchanged for Legacy/CSM boot.** The 2.6 BIOS path is the RC1 path
  (same GRUB 2.14 i386-pc loader and placement policy). 2.6 should be
  expected to behave on the NEC as RC1 did until new data says otherwise. It
  is not a fix for this machine and must not be presented as one. One
  measurable difference: in the final build GRUB's window ends just above the
  kernel's 879 MiB lowmem limit, so the kernel copies the initrd down during
  early setup instead of using it in place at unpack time (see boot-2.6.md) -
  a different, shorter exposure, but not a controlled experiment.
* **Installed systems** no longer use EXTLINUX (the same "as high as possible
  below 2 GiB" placement as ISOLINUX 4.05); they use the same GRUB as the ISO.
* **Native UEFI is not available on this machine in 2.6**: its firmware is
  64-bit, and 2.6 ships only a 32-bit UEFI loader (see boot-2.6.md).

### Mechanisms, ranked (inference, not proof)

1. **Firmware still using memory it reported as free (CSM).** In CSM mode a
   UEFI firmware's own drivers (the USB stack that emulates INT 13h, SMM USB
   legacy support) keep running in boot-services memory that the legacy E820
   map may report as usable. Anything a loader places there can be overwritten
   until the kernel takes over the USB controller. Fits: damage depends on
   placement; one contiguous 64 KB block; never seen under SeaBIOS. For RC1 it
   requires such a region inside 0x35F9F000–0x36FC6FFF as well — unknown.
2. **Faulty RAM** at those physical addresses. Also placement-dependent; a
   memory test separates it from (1).
3. **Kernel-side discard** — ruled out by reading Linux 6.12's x86 code:
   `reserve_initrd()` either uses the initrd in place or copies it
   (`relocate_initrd()`); it has no path that drops it.

Under (1), no loader-side setting fixes it without knowing the region, and
guessing one would be exactly the magic ceiling XXRI refuses. The robust
answer is native UEFI boot, where the firmware's own allocator places the
initrd and `ExitBootServices()` stops firmware DMA before the kernel runs — on
this 64-bit-UEFI laptop that means the x86-64 release (2.7). The x64 GRUB
experiment (boot-2.6.md) could boot the i686 kernel natively on older
firmware, but is firmware-generation-dependent and not shipped.

### Cheapest next data (no diagnostic ISO needed; uses 2.6's own menu)

1. Boot the 2.6 ISO in Legacy mode, choose **"XXRI OS Lite 2.6 (verbose
   boot)"**, photograph the final screen. It shows the kernel's
   `RAMDISK: [mem …]` line (the exact placement on this machine) and either
   `rootfs image is not initramfs (<reason>)` (damaged) or no unpack message.
2. Run any memtest86+ stick for one full pass (rules mechanism 2 in or out).
3. Transcribe from the earlier diagnostic photo: the `BIOS-e820` lines and
   the bad-sector lines (physical addresses of the damaged block).

With (1) and (3) the RC1 window can be checked against the firmware's map.

## 2. Acer (boot / runtime issue)

**No data for this machine exists in the repository, the release notes, the
project notes, or earlier session records** — no model name, no symptom
description, no photos or logs. Nothing was assumed or invented, and the NEC
analysis above is not transferred to it: the two may well be unrelated.

Needed to start: exact model and firmware version; Legacy/CSM or UEFI (and
whether 32- or 64-bit UEFI); which medium and writer (dd, Rufus DD/ISO mode,
Ventoy); how far it gets (firmware boot menu → GRUB menu → splash → panic
text → blank screen → desktop); a photo of the last screen of the **verbose
boot** entry; and, if it reaches a shell, `dmesg` and `/var/log/Xorg.0.log`.

## 3. CPU baseline (found while comparing the builds)

`README.md` states a minimum of "Intel Pentium II / AMD K6-2". Measured:

| Component | Instruction-set need | Runs on K6-2? | Pentium II? |
|---|---|---|---|
| Tiny Core base (busybox, glibc via runtime dispatch) | i486 | yes | yes |
| XXRI programs as shipped (prebuilt in git, host `gcc -m32`) | **CMOV** (P6): installer 46, settings 27, static boot splash 580 instructions | **no** (illegal instruction) | yes |
| the same programs built by Build B (i586 toolchain) | no CMOV | yes | yes |
| `xxri-browser.tcz` (Qt WebEngine 5.15 / Chromium 87) | **SSE2** (~1.3 M XMM instructions) | no | **no** |

So the stated minimum is not met today: the browser needs a Pentium 4 /
Athlon 64-class CPU, and the other XXRI programs a P6-class CPU. Either the
README should say so, or the programs should be built for i586 (Build B does)
and the browser documented as SSE2-only.
