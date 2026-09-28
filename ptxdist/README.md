# XXRI OS Lite — the PTXdist build

This PTXdist 2026.09.0 project is the only way XXRI OS Lite is built. One
command produces the release ISO (BIOS + 32-bit UEFI, one image) from pinned
sources:

```sh
cd ptxdist
ptxdist toolchain <OSELAS.Toolchain-2024.11.1 i586 prefix>/bin   # once
ptxdist --j-intern=6 images    # -> platform-xxri/images/XXRI-Lite-2.6.iso
```

## What goes into the image

| Layer | Where it comes from | Rules |
|---|---|---|
| Tiny Core 16.0 base: `core.gz` (busybox, glibc 2.40, udev, Tiny Core's scripts), `vmlinuz` 6.12.11 | the Tiny Core 16.0 release archive, sha256-pinned, used as released | `xxri-tc-base`, `xxri-kernel` |
| 190 Tiny Core extensions (GTK 3, Mesa, X libraries, codecs, …) | the 16.x mirror / archived 16.0 ISO, md5-pinned in `config/cde/manifest` | `xxri-cde` |
| XXRI configuration, scripts, theme, icons, fonts | `projectroot/` | `xxri-base` |
| XXRI programs | `../src/<name>`, compiled here | one rule each: `xxri-settings`, `xxri-store`, `xxri-installer`, `xxri-dialog`, `xxri-control-center`, `xxri-launcher`, `xxri-media`, `xxri-power-menu`, `xxri-compositor`, `xxri-wctl`, `xxri-bootsplash` |
| Window manager | tinycorelinux/flwm @196f61c + `patches/flwm-git-196f61c` | `xxri-flwm` |
| XXRI File | `../src/xxri-file/pcmanfm` (PCManFM fork) on libfm 1.3.2 (GTK 3), menu-cache 1.1.0, libexif 0.6.21 | `xxri-file`, `xxri-libfm`, `xxri-libfm-extra`, `xxri-menu-cache`, `xxri-libexif` |
| AppImage runtime sonames Tiny Core lacks | libglvnd 1.7.0, Mesa 24.3.4's shared glapi | `xxri-glvnd`, `xxri-glapi` |
| XXRI Browser | `../src/xxri-browser` on Qt 5.15.17 + Qt WebEngine (Chromium 87) built here | `xxri-browser`, `xxri-qt5` |
| X server | XLibre 25.1.9 (`Xorg` + modesetting/glamor/GLX, the setuid wrapper, int10/vgahw/DGA for vesa, `Xfbdev`; two upstream fixes from after the release in `patches/xlibre-xserver-25.1.9`), XLibre's xf86-input-libinput 25.0.1 and xf86-video-vesa 25.0.0 - git tags, sha256-pinned; replaces Tiny Core's X.Org and Xvesa (`../docs/x-server.md`) | `xxri-xlibre`, `xxri-xlibre-input-libinput`, `xxri-xlibre-video-vesa` |
| GRUB 2.14 | upstream tarball, unpatched: boot images (host) and the installer's utilities (target) | `host-xxri-grub`, `xxri-grub-tools` |

Images (`rules/image-*.make`):

* `core.gz` — `scripts/xxri-mkinitramfs.py` lays the XXRI packages over
  Tiny Core's released `core.gz`: deterministic order, Tiny Core's owners and
  modes kept for everything XXRI does not add (see the script's header for
  the merge rules), `config/tc-base.remove` lists what XXRI deletes.
* `xxri-browser.tcz`, `xxri-xlibre.tcz` (loaded at boot) and `xxri-grub.tcz`
  (loaded by the installer) — squashed from their packages.
* `XXRI-Lite-2.6.iso` — `scripts/xxri-mkiso.sh`: GRUB core images, hybrid
  MBR + appended ESP, El Torito BIOS + EFI.
* `sources/` next to the ISO — the Corresponding Source: every source archive
  of the packages XXRI builds and ships (GRUB with its signature), the project
  itself as one archive, where Tiny Core publishes its sources, SHA256SUMS.

## Layout decisions

* **No merged /usr.** Tiny Core has real `/bin`, `/sbin`, `/lib`; PTXdist
  rewrites package paths there to `/usr`. XXRI's packages install nothing
  into those three directories, and Tiny Core's come from its own
  `core.gz`, so the rewrite never applies (`rules/usr-rootfs.in` also turns
  off PTXdist's compatibility links).
* **`/usr/local`** for everything XXRI builds, as Tiny Core does for
  everything outside its base; the browser's Qt lives in its own prefix,
  `/usr/local/lib/xxri-browser`, where it cannot collide with a Tiny Core Qt.
* **Tiny Core's libraries are the build sysroot** (`xxri-tc-sysroot`,
  `config/tc-sysroot.manifest`): XXRI's programs link against exactly the
  GTK, FLTK, X11, NSS… that the system ships.

## Compiler flags (docs/cpu-baseline.md has the measurements)

* Toolchain: OSELAS.Toolchain-2024.11.1 `i586-unknown-linux-gnu` — GCC
  14.2.1, glibc 2.40 (Tiny Core 16's glibc), binutils 2.43.1.
* OS programs and libraries: `-march=i586 -mtune=generic -Os`
  (`XXRI_OS_CFLAGS`, `rules/pre/xxri.make`).
* XLibre: `-march=i586 -mtune=generic -O2` — the server's software
  rendering and input loops are CPU-bound on exactly the machines that
  use them.
* Qt and the browser: `-march=pentium4 -mtune=generic -mfpmath=sse`
  (`XXRI_SSE2_CFLAGS`) — the engine cannot run without SSE2.
* Platform-wide: `-fstack-protector-strong`, `_FORTIFY_SOURCE=2`, RELRO,
  `--as-needed`, GNU hash. Not PIE and not BIND_NOW: on i386 PIE costs a
  register and measurable speed, BIND_NOW costs start-up time. (Chromium's
  own link flags add `-z now` to `libQt5WebEngineCore.so`.)

## Qt 5.15.17 patches

`patches/qt-everywhere-opensource-src-5.15.17/`: PTXdist's own Qt 5.15
series (0001–0600) plus two for this toolchain:

* `0700` — `<cstdint>` includes Chromium 87 relies on and GCC 13+ no
  longer provides transitively.
* `0701` — FFmpeg's x86-32 assembly is not position independent
  (x86inc.asm ignores PIC on x86-32, FFmpeg ticket 7878), so
  `libQt5WebEngineCore.so` has text relocations. Chromium already permits
  them with `-z notext`, but only for lld; the patch applies the same
  permission for GNU ld, whose warning Chromium's `--fatal-warnings` would
  otherwise make fatal. Cost: the dynamic loader writes the relocations
  into those code pages at load time, so the pages are private per process
  instead of shared.

Qt WebEngine is configured as a desktop build
(`--disable-webengine-embedded-build`): Qt switches to its Android-like
embedded mode by itself when cross-compiling, which would change how web
pages lay out (overlay scrollbars, mobile viewport, no CSS hover).

The `qtlocation` map plugins (`geoservices_*`) are disabled: Mapbox GL
does not build with GCC 14 and nothing in XXRI uses Qt Location maps.

## Reproducibility

Every download is checksum-pinned (sha256, or Tiny Core's md5 for its
packages); `SOURCE_DATE_EPOCH` comes from PTXdist; the image scripts sort
their inputs and stamp every file with it. Measured reproducibility:
`../docs/qa-ptxdist.md`.

## Host requirements

PTXdist 2026.09.0; a host `g++` that can build 32-bit programs (`-m32`,
Qt WebEngine's V8 snapshot generator runs on the build host); `bison`;
`fakeroot`; about 30 GB of disk for a clean build including Qt WebEngine.
Everything else (Python 3.13, Node.js, ninja, gperf, meson, squashfs-tools,
xorriso, mtools, dosfstools, GRUB) is built by PTXdist from pinned sources.

## Tools for maintainers

* `scripts/xxri-tc-closure.py` — extend `config/tc-sysroot.manifest` with a
  Tiny Core package and its `.dep` closure.
* `../tools/qa/tcz-deps.py` — the `.dep` list an extension needs, from its
  binaries (used for `config/cde/dep/xxri-browser.tcz.dep`).
* `../tools/qa/elf-closure.py` — every XXRI binary's libraries and symbol
  versions resolve inside the image (part of the release gate).
