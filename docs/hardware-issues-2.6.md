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

## 2. Acer TravelMate 5744

The model is known now; the NEC analysis above is still not assumed to
apply. Its radios and wired network, from the model's driver list
(hwdrivers.com), against what 2.6 ships
([wireless-bluetooth.md](wireless-bluetooth.md)):

| Option fitted | Driver | 2.6 |
|---|---|---|
| Intel WiFi Link 5100/5300, Centrino Wireless-N 1000, Wireless-N 100/130, Advanced-N 6200/6205/6230, Advanced-N + WiMAX 6250 | iwlwifi (iwldvm) | works: firmware in `xxri-firmware-wireless` |
| Atheros AR5B125 (AR9485), AR928x | ath9k | works: no firmware needed |
| Atheros Bluetooth AR3011 / AR3012 | ath3k / btusb | works: `ath3k-1.fw`, `ar3k/*` shipped |
| Broadcom BCM4313, BCM43224, BCM43225 | brcmsmac | works: `brcm/bcm43xx*-0.fw` shipped |
| Broadcom BCM4312, BCM43227, BCM43228 | b43 | **no Wi-Fi**: Broadcom's firmware may not be redistributed; a USB adapter from the table above is the way out |
| Broadcom NetLink gigabit Ethernet | tg3 (in core.gz) | works |
| Wireless key / radio switch | acer-wmi (in core.gz) | the switch shows up as rfkill; Settings shows "switched off by the computer's wireless switch or key" |

Still needed for the boot/runtime issue reported on it: firmware version;
Legacy/CSM or UEFI; which medium and writer (dd, Rufus DD/ISO mode,
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

**Resolved in the PTXdist build** (`docs/cpu-baseline.md`, `docs/qa-ptxdist.md`):
every XXRI program is compiled for i586, and on emulated Pentium (with and
without MMX), Pentium II, Pentium III and SSE2 processors the whole desktop
and every XXRI program ran without executing an instruction the CPU lacks.
Two runtime traps had to be removed for that: Tiny Core's Mesa (i686, SSE2)
must not be loaded by the X servers on a processor without SSE2, and XLibre's
modesetting driver no longer asks GL for the cursor format. The README now
states i586 for the desktop and SSE2 for the browser, and that the result
comes from emulation, not from physical period hardware.

## 4. Display paths (found by the XLibre display matrix, QEMU)

* **KMS takeover blocked by the boot splash** (present in 2.6, hidden by
  Xvesa): drivers that request their PCI regions exclusively (cirrus,
  vmwgfx, ...) failed with EBUSY while the splash held `/dev/fb0`, leaving no
  display device. Fixed (`xxri-splash-handover`). Real-hardware relevance:
  VMware guests (vmwgfx) and any GPU whose KMS driver behaves the same.
* **A KMS driver that takes over and then fails** leaves neither KMS nor a
  framebuffer. On BIOS machines the session now falls back to Xorg with the
  vesa driver (video BIOS); on UEFI machines there is no such fallback and no
  desktop. Likely real-world trigger: missing GPU firmware. 2.6 shipped none;
  the PTXdist build now ships `xxri-firmware-gpu.tcz` (148 KiB,
  `config/firmware/gpu.list`): radeon R100 through Northern Islands and the
  Palm/Sumo/Aruba APUs (CP/PFP/ME/RLC/SMC, and the MC firmware without which
  NI cards do not initialise at all), and Intel Gen9 DMC. Still without
  firmware in the RC: radeon SI/CIK (HD 7000 and newer; they fail KMS and
  take the vesa fallback on BIOS machines), amdgpu, video decode (UVD/VCE),
  i915 GuC/HuC. NVIDIA cards use the framebuffer or vesa path in the RC
  because nouveau was not in its extension set. **Correction:** an earlier
  version of this paragraph said the Tiny Core kernel has no nouveau. It
  has - as a separate package, `nouveau-6.12.11-tinycore.tcz`, which that
  check missed. Branch `xxri-gpu-cpu-support` adds nouveau and firmware for
  GPUs up to the 2015-era generations - radeon SI/CIK, amdgpu GCN 3, the
  Atom/Celeron-class Intel Gen11/Gen12 iGPUs, nouveau Maxwell 2
  (`docs/gpu-cpu-support.md`; not yet tested on hardware).
* **Radeon R100 under QEMU** (`ati-vga`) hangs the guest in radeon KMS -
  an emulation limit; no statement about a real Radeon 7000 is possible.

## 5. Ventoy

How Ventoy 1.1.17 boots XXRI, from its sources and the QEMU runs
(`tools/qa/ventoy-emulate.sh`):

* **Normal mode** chains into the ISO's own boot (BIOS: `ipxe.krn`, then
  XXRI's GRUB), so XXRI's menu and boot codes are the ones that run. Ventoy's
  Tiny Core hook adds an initrd that edits `/etc/init.d/tc-config` with sed
  at two anchors, `[ -n "$WAITUSB" ]` and `if [ -n "$AOE" ]`. Each occurs
  exactly once in `xxri-config`, and both must stay. The hook exports the ISO
  over ATA-over-Ethernet on the loopback device, which is why `aoe.ko` is in
  core.gz. That is how Tiny Core finds `/cde` inside an ISO file on an exFAT
  stick.
* **memdisk mode** loads the whole ISO into RAM as a BIOS disk. Linux cannot
  see that disk, so `/cde` can never be found and the boot ends at a shell.
  This is a limitation of the mode, not something XXRI can fix; use normal
  or grub2 mode. The QA stick sets `VTOY_SECONDARY_BOOT_MENU=0` in
  `ventoy.json` so the emulated boots go straight into normal mode.
* **grub2 mode** runs XXRI's `grub.cfg` from Ventoy's GRUB; to be checked
  with every release's QA.

What a Ventoy boot exposed, and the fixes (2026-10):

* A computer with an earlier XXRI install on its own disk booted that
  install instead of the stick: `/init` switched to any `XXRIROOT` disk when
  no `root=` was given. Live boots (`cde`, on every ISO entry and never in an
  installed `grub.cfg`) no longer do that; an explicit `root=` still wins.
  The same change makes reinstalling over an XXRI disk from the live medium
  possible.
* `rcS` sent all of `tc-config`'s output to `/dev/null`. When an extension
  failed to load, its "Press Enter key" prompt was invisible behind the
  splash, so the boot looked hung. Quiet boots now log that output to
  `/var/log/xxri-boot.log`; the verbose entry shows it on the console; the
  splash only runs on quiet boots. The error prompt itself stops the splash
  and writes to `/dev/console` (`xxri-splash-handover stop`).
* The splash held the screen for up to 90 s whatever happened. It now
  leaves when X is up, or, once `rcS` has finished, when the console wants
  the user: the tty1 session (getty → autologin → login shell, one PID) is
  asleep with no children and no X server, xinit or startx is running.
  That covers a login prompt (`noautologin`), a text boot and an X server
  that gave up.

Rufus, plain BIOS and IA32 UEFI boots take the same paths as before.
