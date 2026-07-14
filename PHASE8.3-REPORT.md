# Phase 8.3 — Native XXRI Boot Splash

## Goal
Hide the Tiny Core boot process behind a native, lightweight XXRI splash so the
system boots like a modern commercial OS. No Plymouth.

## What was built
`xxri-bootsplash` — a self-contained C program (~260 lines) that draws directly
to the Linux framebuffer. No toolkit, no X, no dependencies.

- **Binary:** `rootfs/usr/local/sbin/xxri-bootsplash` — static i686 ELF (~680 KB),
  no runtime libraries (safe to run in early boot).
- **Assets** (pre-rendered raw RGBA, so there is no PNG decoding at runtime):
  `rootfs/usr/local/share/xxri-bootsplash/logo.rgba` (176×183, the XRi logo) and
  `title.rgba` (the "Starting XXRI OS Lite…" text rendered in Avenir Next).
- **Source + build:** `rootfs/usr/local/src/xxri-bootsplash/{xxri-bootsplash.c,build.sh}`.
  `build.sh` renders the assets with ImageMagick and cross-compiles `-m32 -O2 -static`.

## Appearance
Full-screen, 1024×768. Dark background (#0E0D16) with a soft concentric
purple/blue radial glow behind the centred XRi logo, the title line, and five
XXRI-accent **pulsing dots** (a travelling brightness wave). Uses the existing
XXRI design language and system font.

## How it starts
1. **Kernel command line** gains `vga=791` (gives a 1024×768×16 **vesafb**
   `/dev/fb0` — the kernel has vesafb built in), `quiet` (suppresses kernel
   messages), `vt.global_cursor_default=0` (no blinking cursor) and
   `consoleblank=0`. Set in `work/iso/boot/isolinux/isolinux.cfg` (live/installer)
   and recommended by `install2disk` for installed systems.
2. **`/etc/init.d/rcS`** starts the splash in the background, immediately after
   mounting the filesystems and *before* any boot output, gated to graphical
   boots (installed root has `/etc/sysconfig/Xserver`; the live desktop ISO
   passes `cde`). It never runs on text/cli boots.
3. The splash `setsid()`s into its own session (so the tty churn when
   getty/login take over tty1 can't kill it), waits briefly for `/dev/fb0`, then
   paints every frame (~18 fps) directly to the framebuffer.

## How boot text is hidden
- `quiet` silences the kernel.
- `/init` (e2fsck, "switching root") output redirected to `/dev/null`.
- `rcS` runs `xxri-config` (the big coloured Tiny Core boot output) with
  `>/dev/null 2>&1`.
- `.profile` runs `startx >/dev/null 2>&1`; `.xsession` already starts Xvesa
  silently.
- `/etc/motd` and `/etc/issue` blanked (+ `/root/.hushlogin`) so the login banner
  never prints.
- The splash's full-frame repaint covers any residual glyph.

## How it exits (smooth transition)
The splash polls for the X server socket `/tmp/.X11-unix/X0`. The instant Xvesa
is up (it has already set the VESA video mode and taken the display by then), the
splash exits — no fade, no console, no flash. `.xsession` draws the wallpaper
immediately after `waitforX`. A `MAX_SECONDS` safety cap (90 s) guarantees it can
never hold the screen if X fails to start.

**Design note:** an earlier revision put the VT into `KD_GRAPHICS` mode; this
interfered with Xvesa's own VT switch and produced a black desktop, so it was
removed. Text suppression is handled entirely by the quiet kernel + blanked
banners + redirected scripts + the covering repaint, which needs no VT games.

## Performance
Static binary, no libraries, no asset decoding. It runs only while the boot is
otherwise I/O-bound (loading X), so it adds no measurable time to boot; the
desktop appears in ~2–3 s in QEMU. Memory footprint is a single framebuffer-sized
draw plus two small RGBA assets.

## Verification (QEMU, installed image)
Booted `output/xxri-disk.img` with the splash command line and screenshotted the
sequence:
- ~0–1 s: black (quiet kernel; **no Tiny Core text**).
- ~1–2 s: **XXRI splash** — logo, "Starting XXRI OS Lite…", animated dots.
- ~3 s onward: **desktop** — XXRI wallpaper and the clean 6-icon dock.

No Tiny Core console text, no login banner, no blinking cursor, no console flash;
the splash disappears correctly and the desktop loads normally.

## Files modified / added
| Path | Change |
|---|---|
| rootfs/usr/local/src/xxri-bootsplash/{xxri-bootsplash.c,build.sh} | new splash + build |
| rootfs/usr/local/sbin/xxri-bootsplash | built binary |
| rootfs/usr/local/share/xxri-bootsplash/{logo,title}.rgba | assets |
| rootfs/etc/init.d/rcS | start splash (gated), silence xxri-config |
| rootfs/init | redirect e2fsck / switch-root messages |
| rootfs/etc/skel/.profile | `startx` silenced |
| rootfs/etc/motd, rootfs/etc/issue, rootfs/root/.hushlogin | blank boot banner |
| work/iso/boot/isolinux/isolinux.cfg | `quiet vga=791 …` on desktop labels |
| rootfs/usr/sbin/install2disk | recommend the splash cmdline for installs |

## Remaining notes
- The splash is intentionally brief because the boot itself is fast (~2–3 s). No
  artificial minimum-display delay was added, to respect the "must not slow boot"
  requirement.
- On real 1366×768 panels use `xres=1366x768`; the splash always uses the vesafb
  mode from `vga=791` (1024×768) and the desktop then adopts `xres`.
