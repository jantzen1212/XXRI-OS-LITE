# X server: XLibre (replaces X.Org and Xvesa)

## What 2.6 shipped (traced from the 2.6 ISO, `aa8137be…`)

| Package | What it is | Who needed it |
|---|---|---|
| `xorg-server.tcz` (Tiny Core, X.Org 21.1.16) | `Xorg` with the built-in modesetting driver, glamor, GLX; also Xvfb, Xnest | `~/.xsession` (`start_xorg`, the preferred path on a KMS device) |
| `xf86-input-libinput.tcz` | X.Org's libinput driver | `Xorg`: `.xsession` refuses to start it without an input driver |
| `xf86-video-fbdev.tcz` | X.Org's fbdev driver | nothing: `.xsession` starts `Xorg` only when a KMS device is ready, and then modesetting drives it |
| `Xvesa.tcz` (TinyX 1.3, 2018 build) | VESA-BIOS server, setuid root | the fallback in `.xsession`; `xxri-display` (mode list via `Xvesa -listmodes`, session restart via `pkill Xvesa`); `xxri-desktop restart`; Tiny Core's `xsetup.sh` (the `xsetup` boot code) |
| `libXfont.tcz` (libXfont 1.x) | old font library | **only `Xvesa`** (the NEEDED scan of every ELF in core.gz and all 196 extensions finds no other user) |
| `/etc/X11/Xwrapper.config`, `/usr/local/etc/X11/Xwrapper.config` | configuration of `Xorg.wrap` (`allowed_users=anybody`, `needs_root_rights=yes`) | `Xorg`: `/usr/local/bin/Xorg` is a script that runs the setuid-root `/usr/local/lib/xorg/Xorg.wrap`; with this file the wrapper keeps root for the autologin session, which it would otherwise refuse as not being on a console VT. The file that counts is `/usr/local/etc/X11/Xwrapper.config` (the wrapper's `SYSCONFDIR`) |

Xvesa needs a VESA BIOS, which UEFI machines do not have: on a UEFI machine
without a KMS driver 2.6 had no working X server at all.

## What the PTXdist build ships

`xxri-xlibre.tcz` (rules `xxri-xlibre`, `xxri-xlibre-input-libinput`,
`image-xxri-xlibre-tcz`), built from source:

* **XLibre 25.1.9** (the current stable release, 2026-07-27), one source
  tree, two servers:
  * `Xorg` — the full server with the in-tree **modesetting** driver
    (KMS/DRM), **glamor** acceleration through Mesa, **GLX** (Mesa DRI),
    DRI3/Present, RandR, XKB, XInput2, MIT-SHM. The preferred path.
  * `Xfbdev` — XLibre's kdrive server on the kernel framebuffer (vesafb on
    BIOS, efifb on UEFI, legacy fbdev drivers). Software rendering, no Mesa,
    no driver modules, a few MB of RSS. Replaces Xvesa, and unlike Xvesa it
    also works on UEFI.
* **XLibre's `xf86-input-libinput` 25.0.1** — X.Org's driver modules are not
  ABI-compatible with XLibre.
* **XLibre's `xf86-video-vesa` 25.0.0** and the server's `int10` (x86emu),
  `vgahw` and DGA modules it needs: the video-BIOS fallback (below).
  XLibre 25.1.9 carries one upstream fix from after the release
  (`patches/xlibre-xserver-25.1.9/0001`, upstream `7dd2a11ae6`): its meson
  compared `host_machine.cpu()` ("i586") with CPU family names, so 32-bit
  x86 builds lost `-D_PC` - int10 never mapped the real video BIOS - and
  Linux ACPI support. (`-Dint10=vm86` does not build in 25.1.9: its
  sources moved without the meson file.)
* Tiny Core's paths are kept (`/usr/local/lib/xorg/modules`,
  `/usr/local/share/X11/xkb`, `xkbcomp` in `/usr/local/bin`, the
  `/usr/local/lib/X11/fonts` default font path, `/var/log/Xorg.0.log`), so
  Tiny Core's `xkeyboard-config`, `xkbcomp` and font extensions work as
  before. The extension's install script writes `/etc/sysconfig/Xserver`
  (`Xorg`) exactly as `xorg-server.tcz`'s did — `~/.profile` starts X only
  when that file exists.
* **Privileges as in 2.6.** Tiny Core has no logind, so an unprivileged
  `Xorg` can neither become DRM master nor take over the VT. XLibre is
  built with its setuid wrapper (`-Dsuid_wrapper=true`) in Tiny Core's
  layout: `/usr/local/bin/Xorg` is the script that runs
  `/usr/local/lib/xorg/Xorg.wrap` (setuid root), which runs the server
  `/usr/local/lib/xorg/Xorg` as root because
  `/usr/local/etc/X11/Xwrapper.config` says `needs_root_rights=yes` and
  `allowed_users=anybody`. (An earlier revision of this document called the
  2.6 wrapper ineffective and dropped the file; the first boot test of the
  XLibre ISO showed `Xorg` failing on every KMS machine as a result.)
  `/etc/X11/Xwrapper.config` is not read by this wrapper and is not shipped.
  `Xfbdev` is setuid root, as Xvesa was: kdrive opens the console, the
  framebuffer and the input devices itself.
  `validate-release.sh` checks both setuid bits and the wrapper
  configuration.

Removed from the extension set: `xorg-server.tcz`, `xf86-input-libinput.tcz`,
`xf86-video-fbdev.tcz`, `Xvesa.tcz`, `libXfont.tcz`. Nothing else needed
them (NEEDED scan of every ELF in `core.gz` and all extensions). Xvesa's
*role* - a desktop through the video BIOS when the kernel has no display
device - is still needed; the vesa driver in `Xorg` fills it.

### Server selection (`~/.xsession`)

1. **KMS/DRM: `Xorg` + modesetting**, whenever a DRM connector is ready
   (`xxri-hw-init` loads the KMS driver of every display-class PCI device
   first; the existing ENODEV
   wait and the second attempt on a KMS machine are unchanged). Input:
   libinput on the evdev devices.
2. **`Xfbdev`** when a framebuffer is registered
   (`/sys/class/graphics/fb0`) but there is no KMS device: a GPU without a
   KMS driver in the Tiny Core kernel, `nomodeset`, a firmware framebuffer
   only. Console keyboard and `/dev/input/mice`, as Xvesa used; the mode is
   the boot loader's (GRUB sets 1024x768 on BIOS; on UEFI it keeps the
   firmware's GOP mode).
3. **`Xorg` + vesa** on a BIOS machine with neither a KMS device nor a
   framebuffer: a KMS driver took the display over from vesafb and then
   failed (a GPU whose firmware Tiny Core does not ship, a driver that
   rejects the device). XLibre's vesa driver sets the mode through the
   video BIOS (VBE, executed by XLibre's x86emu int10) - the case Xvesa
   covered in 2.6, now with the full server (XInput2, XKB, RandR). UEFI
   firmware has no VBE: there, no desktop.
4. None of these: no desktop (text console), as 2.6 when Xvesa also failed.

The test is the registered framebuffer, not `/dev/fb0`: Tiny Core's
`core.gz` carries a static `/dev/fb0` node that exists with no device
behind it.

**Boot splash and KMS takeover.** A KMS driver takes the display from the
firmware framebuffer by unregistering it, and the framebuffer releases its
memory only when the last process with `/dev/fb0` open lets go. The boot
splash paints on `/dev/fb0` until X is up, so drivers that request their
PCI regions exclusively (cirrus, vmwgfx, ...) failed with EBUSY and left no
display device at all. `xxri-splash-handover` pauses the splash around every
boot step that can load a display driver (the udev trigger in `xxri-setup`,
`xxri-hw-init`) and resumes it afterwards. 2.6 had the same failure; Xvesa,
which does not need the kernel framebuffer, hid it.

**Pointer on Xfbdev.** kdrive's mouse driver detects the protocol of
`/dev/input/mice` from the first packets: the first five packets after the
server starts (about 50 ms of movement) are not delivered. Afterwards it
switches the device to IntelliMouse / Explorer mode, so the wheel works.

Resolution: `xres=WxH` boot code, then Tiny Core's `xvesa=WxHxD` (kept as an
alias), then the mode chosen in Settings (`xxri-display set-resolution`,
applied live through RandR), default 1024x768. The `xsetup` boot code (Tiny
Core's Xvesa mode picker, which rewrites `~/.xsession`) is no longer honoured.

### Processors without SSE2

Tiny Core's Mesa 25 is i686 code (CMOV) compiled with Mesa's default
`-Dsse2=true`. Measured under QEMU with the ISA sentinel
(`docs/cpu-baseline.md`): on a Pentium III, `libgallium` executes SSE2 as
soon as glamor or GLX initialise; on a Pentium (i586), `libEGL` executes
CMOV - both X servers died with SIGILL there. So on a processor without
SSE2, `~/.xsession` starts the servers without Mesa:

* `Xorg` on KMS: `-config xxri-nosse2-kms.conf` - modesetting with
  `AccelMethod none`, no GLX module;
* `Xorg` + vesa: `-config xxri-nosse2.conf` - no GLX module, no Device
  section (Xorg must still autoconfigure vesa);
* `Xfbdev`: `-extension GLX`.

The files are in `/usr/local/etc/X11` (`xxri-xlibre.tcz`); the decision is
taken at session start, after the KMS device has settled (DRM connectors
register asynchronously, so hardware init cannot know yet).

### GPUs Mesa cannot accelerate safely (branch `xxri-gpu-cpu-support`)

With nouveau in the extension set (`docs/gpu-cpu-support.md`; not yet tested
on hardware), NVIDIA GPUs up to Ampere get a KMS device and Xorg + modesetting
(accelerated up to Maxwell, whose firmware ships; display only on Pascal ..
Ampere; RTX 40 stays on the framebuffer, as nouveau needs GSP-RM there). For the
oldest of them - NV04 .. NV4x, with the nForce and C51-C68 IGPs - Mesa has
no driver at all (NV04-NV2x) or only the limited nv30 one, on which glamor
misrenders. When such a GPU drives the primary display (`boot_vga`),
`~/.xsession` (`legacy_nv`) starts `Xorg -config xxri-noaccel-kms.conf`:
modesetting with `AccelMethod none` (a software-rendered shadow
framebuffer), GLX kept and rendered by llvmpipe. The chip family comes from
the `boot0` value nouveau logs at probe, decoded as the driver decodes it.
The no-SSE2 configuration takes precedence (no Mesa at all).

**Safe graphics.** The ISO and the installed system have a "safe graphics"
boot entry with `nomodeset`: no KMS driver binds (i915, xe, amdgpu, radeon,
nouveau and the virtual GPUs all honour it), so path 2 (`Xfbdev` on the boot
loader's framebuffer) or, without one, path 3 is used. It is the way out
when a KMS driver hangs or leaves the screen dark on real hardware.

XLibre 25.1.9's modesetting driver also asked GL for the renderer name to
choose the hardware cursor format, with or without glamor - through libepoxy
into Tiny Core's `libEGL`, which killed `Xorg` on the Pentium even with
`AccelMethod none`. `patches/xlibre-xserver-25.1.9/0002` is upstream
`a7454facc6` ("Don't rely on gl for setting the cursor mask interleave"),
which takes the kernel driver name from DRM instead.

### Not included: kdrive `Xmodesetting`

XLibre has a kdrive modesetting server (`Xmodesetting`, a KMS counterpart of
Xfbdev with optional glamor) **only as an open pull request**
([X11Libre/xserver#3695](https://github.com/X11Libre/xserver/pull/3695),
opened 2026-09-12, not merged, 49 commits). It is written against the 25.2
development branch and depends on the unmerged kdrive restructuring of
[#3688](https://github.com/X11Libre/xserver/pull/3688) (`hw/kdrive/os/linux`,
`hw/kdrive/fake`), which the 25.1.9 release does not have, so it cannot be
applied to a released XLibre. XXRI's KMS/DRM path is XLibre's modesetting
driver in `Xorg`. When `Xmodesetting` is released it fits between the two
paths above: a small KMS server for low-memory machines, before falling back
to Xfbdev.

## Test results

`docs/qa-ptxdist.md`, "Display, input, acceleration and fallback": every
path above reached the desktop with working pointer, buttons, wheel and
keyboard in QEMU - Xorg + modesetting (BIOS, IA32 UEFI, Cirrus), Xfbdev
(`nomodeset` on BIOS and UEFI, Rage 128), Xorg + vesa (no display device;
VMware SVGA after vmwgfx gave up) - and on emulated Pentium, Pentium II,
Pentium III and SSE2 processors. glamor itself was not exercised: QEMU offers
only Mesa's software rasteriser, on which modesetting refuses glamor by
design.
