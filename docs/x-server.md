# X server: XLibre (replaces X.Org and Xvesa)

## What 2.6 shipped (traced from the 2.6 ISO, `aa8137be…`)

| Package | What it is | Who needed it |
|---|---|---|
| `xorg-server.tcz` (Tiny Core, X.Org 21.1.16) | `Xorg` with the built-in modesetting driver, glamor, GLX; also Xvfb, Xnest | `~/.xsession` (`start_xorg`, the preferred path on a KMS device) |
| `xf86-input-libinput.tcz` | X.Org's libinput driver | `Xorg`: `.xsession` refuses to start it without an input driver |
| `xf86-video-fbdev.tcz` | X.Org's fbdev driver | nothing: `.xsession` starts `Xorg` only when a KMS device is ready, and then modesetting drives it |
| `Xvesa.tcz` (TinyX 1.3, 2018 build) | VESA-BIOS server, setuid root | the fallback in `.xsession`; `xxri-display` (mode list via `Xvesa -listmodes`, session restart via `pkill Xvesa`); `xxri-desktop restart`; Tiny Core's `xsetup.sh` (the `xsetup` boot code) |
| `libXfont.tcz` (libXfont 1.x) | old font library | **only `Xvesa`** (the NEEDED scan of every ELF in core.gz and all 196 extensions finds no other user) |
| `/etc/X11/Xwrapper.config`, `/usr/local/etc/X11/Xwrapper.config` | configuration of `Xorg.wrap` | nothing effective: Tiny Core's `Xorg.wrap` is not setuid, so `Xorg` always ran as the session user |

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
* Tiny Core's paths are kept (`/usr/local/lib/xorg/modules`,
  `/usr/local/share/X11/xkb`, `xkbcomp` in `/usr/local/bin`, the
  `/usr/local/lib/X11/fonts` default font path, `/var/log/Xorg.0.log`), so
  Tiny Core's `xkeyboard-config`, `xkbcomp` and font extensions work as
  before. The extension's install script writes `/etc/sysconfig/Xserver`
  (`Xorg`) exactly as `xorg-server.tcz`'s did — `~/.profile` starts X only
  when that file exists.
* No `Xorg.wrap` (`-Dsuid_wrapper=false`): 2.6's wrapper was never setuid,
  so nothing changes; the two `Xwrapper.config` files are removed.
  `Xfbdev` is setuid root, as Xvesa was: kdrive opens the console and the
  framebuffer itself.

Removed from the extension set: `xorg-server.tcz`, `xf86-input-libinput.tcz`,
`xf86-video-fbdev.tcz`, `Xvesa.tcz`, `libXfont.tcz`.

### Server selection (`~/.xsession`)

1. **KMS/DRM: `Xorg` + modesetting**, whenever a DRM connector is ready
   (`xxri-hw-init` loads the GPU's KMS driver first; the existing ENODEV
   wait and the second attempt on a KMS machine are unchanged).
2. **`Xfbdev`** on `/dev/fb0` when there is no KMS device (a GPU without a
   KMS driver in the Tiny Core kernel, `nomodeset`, a firmware framebuffer
   only). Console keyboard and `/dev/input/mice`, as Xvesa used; the mode is
   the boot loader's (GRUB sets 1024x768 on BIOS; on UEFI it keeps the
   firmware's GOP mode).
3. Neither: no desktop (text console), as 2.6 when Xvesa also failed.

Resolution: `xres=WxH` boot code, then Tiny Core's `xvesa=WxHxD` (kept as an
alias), then the mode chosen in Settings (`xxri-display set-resolution`,
applied live through RandR), default 1024x768. The `xsetup` boot code (Tiny
Core's Xvesa mode picker, which rewrites `~/.xsession`) is no longer honoured.

### Processors without SSE2

Tiny Core's Mesa 25 is compiled with Mesa's default `-Dsse2=true`; the 2.6
Pentium III run (`docs/cpu-baseline.md`) shows `libgallium` executing SSE2
as soon as X.Org's glamor starts. `xxri-hw-init` therefore writes
`/etc/X11/xorg.conf.d/10-xxri-no-sse2.conf` on a CPU without SSE2:
modesetting with `AccelMethod none` (shadow framebuffer) and no GLX module,
so `Xorg` never loads Mesa. `Xfbdev` never loads Mesa at all.

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

See the "X server" section of `docs/qa-ptxdist.md` (filled from
`tools/qa/x-test.py`).
