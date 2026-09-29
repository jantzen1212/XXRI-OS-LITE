# GPU and CPU support: firmware for 2015-era GPUs, CPU microcode

Branch `xxri-gpu-cpu-support` (from the 2.6 PTXdist RC, `876e989`),
2026-09-29; built into the 2.6 final ISO. **Status: not yet tested on
hardware.** The sizes below are exact (see "Sizes"); no statement here about
real GPUs is a test result.

**Scope** (kept to the 2015-era generations to save ISO space): GPU firmware for
* **Intel** through Gen9, plus the newer Atom- and Celeron-class iGPUs
  (Elkhart/Jasper Lake, Tiger Lake Celerons, Alder Lake-S/-P/-N, Twin Lake);
* **AMD** up to the 2015-era GCN generations (GCN 1-3: HD 7000, R7/R9
  200/300, R9 285/380/Fury/Nano, Carrizo and Stoney APUs) - no Polaris, Vega
  or RDNA;
* **NVIDIA** up to the 2015-era GTX generation (Maxwell: GTX 900) - no
  Pascal, no RTX, no GSP-RM;

plus the nouveau driver for those NVIDIA GPUs and early CPU microcode for
all Intel and AMD CPUs.

## What sets the ceiling: the kernel

XXRI uses Tiny Core 16.0's kernel unchanged (6.12.11, i486-class 32-bit):

| Kernel fact (`config-6.12.11-tinycore`) | Consequence |
|---|---|
| 32-bit, `HIGHMEM4G`, no PAE | at most 4 GiB of physical address space; with the PCI hole, typically 3-3.5 GiB of RAM usable |
| `NR_CPUS=8` | a CPU with more than 8 threads runs on 8 of them |
| 32-bit EFI stub only | boots on BIOS/CSM and IA32 UEFI; machines with 64-bit-only UEFI (most laptops from about 2020) cannot start the ISO (`docs/boot-2.6.md`, "64-bit UEFI") |
| `DRM_I915`, `DRM_XE`, `DRM_AMDGPU` (no `_SI`/`_CIK`), `DRM_RADEON`, `DRM_NOUVEAU` (separate Tiny Core package) | the GPU drivers; all stay, the scope limits only firmware |
| `MICROCODE=y`, `MICROCODE_INITRD32=y`, no late loading | microcode reaches the CPU only from the initrd, at boot |
| `X86_INTEL_PSTATE=y`, `X86_AMD_PSTATE=y`, `INTEL_IDLE=y`, `acpi-cpufreq` etc. in `core.gz` | CPU frequency scaling on new CPUs needs nothing extra |

Userland needs no change: Tiny Core's Mesa 25.2.5 (`mesa.tcz`) has iris,
crocus, i915, radeonsi, r600, r300 and nouveau, and its libdrm the amdgpu,
intel, nouveau and radeon back ends; XLibre's modesetting drives every KMS
device.

## Covered GPUs

| Vendor | GPUs | Driver | Firmware (extension) |
|---|---|---|---|
| Intel | GMA .. Broadwell, Bay/Cherry Trail | i915 | none needed |
| Intel | Gen9: Skylake .. Comet Lake, Apollo Lake, Gemini Lake | i915 | DMC (`xxri-firmware-gpu`, unchanged) |
| Intel | Gen11 Elkhart/Jasper Lake (Celeron N4500/N5100/N5105/N6210, Pentium Silver N6000, Atom x6000E); Ice Lake shares the file | i915 | DMC (`xxri-firmware-intel`) |
| Intel | Tiger Lake (Celeron 6305, Pentium Gold 7505, Core 11th gen U/H) | i915 | DMC |
| Intel | Alder Lake-S / Raptor Lake-S (Celeron G6900, Core 12th-14th gen desktop) | i915 | DMC, GuC (HuC loading on ADL-S, submission on RPL-S), HuC |
| Intel | Alder Lake-N and Twin Lake (Intel Processor N50/N95/N97/N100/N150/N200/N250, Core i3-N300/N305), Alder/Raptor Lake-P/U (Celeron 7305, Core mobile) | i915 | DMC, GuC submission, HuC |
| AMD | R100 .. Northern Islands, Palm/Sumo/Aruba APUs | radeon | `xxri-firmware-gpu`, unchanged |
| AMD | Southern Islands, GCN 1 (HD 7700-7900, R7 240/250, R9 270-280X, HD 8000M) and Sea Islands, GCN 2 (R7 260/360, R9 290/390, Kaveri/Kabini/Beema/Mullins APUs) | radeon | `xxri-firmware-amd` |
| AMD | Volcanic Islands, GCN 3: Tonga (R9 285/380/380X), Fiji (R9 Fury/Nano), Topaz (R7 M260/M265), Carrizo, Stoney Ridge APUs | amdgpu | `xxri-firmware-amd` |
| NVIDIA | NV04 .. Kepler (Riva TNT .. GTX 780 Ti), Maxwell 1 (GTX 750/750 Ti, 840M-940MX) | nouveau | none needed |
| NVIDIA | Maxwell 2 (GTX 950-980 Ti, Titan X, GTX 965M-980M) | nouveau | signed acceleration firmware (`xxri-firmware-nvidia`) |

The pinned `i915.ko` binds every Intel platform listed (checked by PCI ID,
e.g. Alder Lake-N 0x46D0-0x46D2, Twin Lake 0x46D3/0x46D4, Jasper Lake
0x4E55/0x4E61/0x4E71, Elkhart Lake 0x4555/0x4571). Stoney Ridge (2016) is in
because it is the same GCN 3 generation and common in low-cost laptops that
still boot BIOS/CSM. nouveau runs every NVIDIA GPU at the clocks the video
BIOS sets; NV04 .. NV4x run without GPU acceleration (see "Runtime").

## GPUs beyond the scope - what the 6.12.11 drivers do without firmware

From the driver source (untested). The "safe graphics" boot entry is the way
out wherever the screen stays dark.

* **AMD Polaris (RX 400/500) and newer** - Vega 20, Navi/RDNA 1-4 (RX 5000
  .. RX 9000), Renoir and newer APUs: amdgpu removes the firmware
  framebuffer right after its early init (`amdgpu_device.c`, l. 4244) and
  requests most firmware only afterwards, so it fails with the screen taken.
  BIOS/CSM machines then get Xorg + vesa through the card's video BIOS (the
  2.6 RC's behaviour for these cards); UEFI machines get no desktop unless
  booted with "safe graphics". Vega 10/12, Raven/Picasso and Navi 12 differ:
  amdgpu needs a `gpu_info` file already in early init and stops *before*
  touching the framebuffer, so the desktop runs on Xfbdev.
* **NVIDIA Pascal, Volta, Turing, Ampere** (GTX 10/16, RTX 20/30): nouveau's
  graphics engine has a "firmware unavailable" fallback (`gm200_gr_nofw`), so
  nouveau drives the display (KMS, native modes) without acceleration.
* **NVIDIA Ada (RTX 40)**: its only GSP entry needs the GSP-RM image, the
  GSP constructor fails, and nouveau's probe returns before it removes the
  firmware framebuffer ("check that the chipset is supported before booting
  fbdev off the hardware", `nouveau_drm.c`) - Xfbdev/vesa as before. RTX 50
  (Blackwell) is unknown to 6.12.11.
* **Intel Rocket Lake**: display works without DMC (no deep package power
  states). **DG1, DG2 (Arc A), Meteor/Arrow Lake**: i915 keeps the display;
  without GuC there is no acceleration. **Lunar Lake, Battlemage (xe)**: xe
  takes the framebuffer and then refuses the GPU - but Lunar Lake laptops
  are 64-bit-UEFI-only and Arc cards have no legacy video BIOS, so XXRI
  cannot reach them anyway.

## Sizes (exact)

Measured on a throwaway ISO assembled with the branch's `xxri-mkiso.sh` from
the RC's own `vmlinuz` and extensions plus the new and changed files, each
made exactly as the build makes it (firmware from the host's linux-firmware
20260810 package - the same release - staged with its licences and list,
root-owned, timestamps at `SOURCE_DATE_EPOCH`, `mksquashfs -comp xz`;
`core.gz` rewritten with the project's own cpio writer and gzip settings).
The method is checked: re-assembling the RC from its parts with the RC's
script gives a byte-identical ISO (`28d6304c…`), and re-squashing an
unchanged RC extension gives a byte-identical `.tcz`. That mock ISO passes
`validate-release.sh` with 0 failures and 0 warnings.

| Item | Files | On the ISO | Unpacked |
|---|---|---|---|
| `xxri-firmware-intel.tcz` | 7 | 450,560 B | 1,416,656 B |
| `xxri-firmware-amd.tcz` | 124 (amdgpu 52, radeon 72) | 1,933,312 B | 5,807,008 B |
| `xxri-firmware-nvidia.tcz` | 25 + 20 links | 28,672 B | 89,731 B |
| `nouveau-6.12.11-tinycore.tcz` | Tiny Core's, md5 `b0de2d28…` | 618,496 B | |
| `boot/intel-ucode.img` | 222 updates, Intel microcode-20260925 | 15,265,416 B | |
| `boot/amd-ucode.img` | 6 family files, linux-firmware 20260810 | 305,516 B | |
| licences, checksums, `grub.cfg`, `core.gz` (+1,978 B) | | ≈26 KB | |
| **ISO** | | **254,803,968 → 273,436,672 B: +18,632,704 B (+7.3%), 260.8 MiB** | |

The CPU microcode is 84% of the growth (15.57 MB); the GPU firmware is
2.41 MB and nouveau 0.62 MB. An installed system unpacks the firmware onto
its disk (+7.3 MB) and keeps nouveau (+0.6 MB) and the one microcode image
for its CPU (15.3 or 0.3 MB).

For comparison, the first version of this branch (GPU firmware up to Lunar
Lake/Battlemage, RDNA4 and Ada with GSP-RM) added 66.4 MB (ISO ≈321 MB).
Measured cost of putting families back (squashfs, same method):

| Family | Files | Adds |
|---|---|---|
| AMD Polaris (RX 400/500, 2016) | 73 | 1.30 MB |
| NVIDIA Pascal (GTX 10, 2016) | 68 + 61 links | 0.11 MB |
| AMD Vega, Raven .. RDNA4 (the rest of amdgpu's consumer GPUs) | 374 | 15.48 MB |
| NVIDIA Volta/Turing/Ampere acceleration (no GSP) | 130 + 65 links | 0.78 MB |
| NVIDIA Ada GSP-RM (RTX 40; lets Ampere use GSP too) | 7 + 9 links | 26.57 MB |
| Intel Rocket Lake, DG1/DG2, Meteor Lake, Lunar Lake, Battlemage | 18 | 3.59 MB |

(Each squashed alone; together in one extension they compress slightly
better.)

## Sources

* **linux-firmware 20260810** - the release PTXdist 2026.09.0 itself pins,
  fetched as `.tar.xz` (sha256 `ac17c34f…`, kernel.org's signed
  `sha256sums.asc`; `rules/pre/xxri.make`). One 649 MB download at build
  time, read by the three firmware packages and the AMD microcode.
* **Intel microcode-20260925** - GitHub release archive, sha256 `0ec19023…`
  (identical over repeated downloads; `rules/xxri-ucode.make`).
* **nouveau-6.12.11-tinycore.tcz** - md5-pinned in `config/cde/manifest`.

## How the files are chosen

`config/firmware/{intel,amd,nvidia,amd-ucode}.list` name firmware the way
the drivers request it; each list's header gives the scope and what is left
out. `scripts/xxri-fw-requested.sh <module> <tcz>` prints what a pinned
module declares. `scripts/xxri-fw-select.py` resolves every name through the
release's `WHENCE` (file, file link or directory link) and unpacks exactly
those; a name the release lacks stops the build.

* **Intel**: per platform, exactly what 6.12.11's i915 loads with default
  parameters (`intel_uc.c`: GuC/HuC from Gen12, not on Tiger/Rocket Lake,
  HuC-only on Alder Lake-S; DMC everywhere); older fallback versions left
  out because the preferred file is present.
* **AMD**: for each covered GPU, every file the driver declares - amdgpu
  gives up on a GPU when one IP block's firmware is missing. Radeon UVD/VCE
  (video) stays out, as in `gpu.list`.
* **NVIDIA**: every file nouveau declares for GM200/GM204/GM206.
* **Microcode**: all of Intel's `intel-ucode/` and `intel-ucode-with-caveats/`
  (its caveat is about late loading, which this kernel does not do); all
  six AMD family files.

## How it works at runtime

* **Loading**: nouveau and the firmware extensions are in `onboot.lst` right
  after `graphics-6.12.11-tinycore.tcz`, so they are mounted before
  `xxri-setup`'s `udevadm trigger`, which loads the KMS drivers by modalias.
  Firmware is served from `/usr/local/lib/firmware` by Tiny Core's udev
  helper (sysfs fallback, `FW_LOADER_USER_HELPER_FALLBACK=y`); all four GPU
  drivers request firmware through calls that allow it. `xxri-hw-init` loads
  drivers for every display class (0x03xx), not only VGA.
* **X server**: Xorg + modesetting + glamor on a KMS device, except nouveau
  on the primary display of a pre-GeForce-8 GPU (NV04 .. NV4x, nForce and
  C51-C68 IGPs): `~/.xsession` (`legacy_nv`) uses `xxri-noaccel-kms.conf`
  (no glamor), because Mesa has no driver for NV04-NV2x and glamor
  misrenders on nv30/nv40. The family is decoded from the `boot0` value
  nouveau logs, as the driver decodes it.
* **Safe graphics**: a boot entry (ISO and installed system) with
  `nomodeset`; no KMS driver binds, the desktop runs on the boot loader's
  framebuffer.
* **Microcode**: GRUB loads `boot/<vendor>-ucode.img` in front of `core.gz`,
  the vendor taken from SMBIOS (both images when unknown); the installer
  copies only the image for the CPU it runs on. `rcS` frees the copy the
  kernel leaves in the RAM root.

## Limits (documented, not fixed here)

1. **Nothing is tested on real GPUs.** QEMU emulates none of them; the
   kernel skips early microcode under a hypervisor.
2. **GPUs beyond the scope** behave as described above; on UEFI a Polaris-or-
   newer AMD card needs the "safe graphics" entry.
3. **64-bit-only UEFI** machines cannot boot XXRI at all.
4. **Kernel ceilings**: 8 CPUs, about 4 GiB, nothing newer than 6.12.11
   knows.
5. **Not part of this change**: SOF audio DSP firmware, Wi-Fi 6E/7 firmware,
   newer pinctrl/I2C-HID platform drivers, `hwmon` sensors (in Tiny Core's
   `hwmon-6.12.11-tinycore.tcz`; nothing in XXRI reads them).
6. **Microcode on a moved disk**: an installed system moved to a machine of
   the other CPU vendor boots without microcode until the image is copied.
   The kernel loads none on Intel family 5 or AMD before family 10h. AMD's
   linux-firmware files cover fewer models than Intel's release (client
   Ryzen updates often come only with a BIOS update).
7. **Settings > Power** on `intel_pstate`/`amd_pstate` offers only
   `performance` and `powersave`; there `powersave` is dynamic scaling
   (HWP/EPP), not the lowest clock.

## Building it (incremental, no clean build)

`ptxdist images` alone picks up every change: the affected leaf packages
depend on their input files (`xxri-base`, `xxri-installer`, `xxri-xlibre`
targetinstall, `xxri-cde` get, the firmware lists). New: the three firmware
packages and `xxri-ucode` (the 649 MB download) with their images;
re-packaged only: `xxri-base`, `xxri-installer`, `xxri-xlibre`,
`xxri-firmware-gpu`, `xxri-cde`, then `core.gz`, `xxri-xlibre.tcz` and the
ISO. No package compiles; if the log shows any `compile` stage other than
the four new packages, stop and investigate. Estimate: 10-15 minutes plus
the download (not measured). Expected ISO: 273,436,672 B.

## Test plan

1. **Artifacts**: the ISO's file list and hashes against the RC and against
   the mock above: new `boot/*-ucode.img`, `boot/LICENSE.*-ucode`, the four
   new extensions (+ md5/dep), changed `grub.cfg`, `core.gz`,
   `xxri-xlibre.tcz`, `xxri-firmware-gpu.tcz`, `onboot.lst`; nothing else.
   `xxri-browser.tcz` byte-identical.
2. **Release gate**: `validate-release.sh` (checks every listed firmware
   name resolves inside its extension, microcode format and placement,
   the safe-graphics entry, nouveau).
3. **QEMU, BIOS and IA32 UEFI**: every boot entry incl. "safe graphics";
   GRUB's microcode choice with `-smbios type=4,manufacturer=...` set to an
   Intel, an AMD and another string; `/kernel` gone after boot; RAM vs the
   RC; the display matrix (`x-test.py`) unchanged.
4. **Installer in QEMU** with an Intel and an AMD CPU model: the installed
   `grub.cfg` names the right image; the system boots.
5. **Real hardware**: one each of radeon SI/CIK, amdgpu GCN 3, a Jasper
   Lake or Alder Lake-N machine, an NVIDIA NV4x or older and a Maxwell 2 card;
   `dmesg | grep -i -E "firmware|drm"`, `/var/log/Xorg.0.log`,
   `/var/log/xxri-hw-init.log`.
