# Phase 8 — xxri Settings (GTK3 Control Center) — Report

## Summary
xxri Settings is now XXRI OS Lite's first official **GTK3** application, replacing
the Tiny Core Control Panel. It is a pure frontend over the Phase 7 hardware layer:
native GtkListBox sidebar + GtkStack pages, styled entirely by `xxri.css`
(One UI / GNOME / elementary inspired), rendered with the shipped Avenir Next system
font. Every page was hand-designed to production quality and verified rendering on the
real device (Xvesa) with live backend data.

## Pages implemented (12 sidebar sections)
| Section | Backend(s) consumed | Notes |
|---|---|---|
| Wi-Fi & Network | xxri-network | Compact interface rows (IP inline), single Network Details card, live scan list |
| Bluetooth | xxri-bluetooth | Adapter switch, paired list; full-page empty state when no adapter |
| Connected Devices | xxri-hardware, xxri-input, xxri-audio, xxri-display | USB / input / camera / audio / display / printer groups |
| Wallpaper & Style | xxri-desktop | Preview, scaling, dock position + size, appearance, theme |
| Sounds & Vibration | xxri-audio | Output/input devices, volume + mic sliders, system sounds |
| Help & Support | xxri-open-url | Opens https://xxri.flows.best |
| Storage | xxri-storage | Per-disk cards, gradient usage bars, mount/eject, SMART health |
| Battery | xxri-power | Battery, CPU governor chips, brightness, suspend capabilities |
| Apps | xxri-app | Installed AppImages, launch/remove; full-page empty state |
| General Management | xxri-desktop, xxri-input | Language/region, date/time, hostname, autologin, reset/restart |
| Software Update | xxri-updates | Package count, check/refresh/update, history log |
| About Device | xxri-hardware, xxri-storage, xxri-display, xxri-power | Hero logo, hardware + system + legal |

## Design language
- **Font:** Avenir Next (ships in `/usr/local/share/fonts/xxri`, wired via
  `60-xxri-avenir.conf`). Fixed a fallback-to-monospace issue observed in preview.
- **Cards:** 16px radius, soft shadow, One UI key/value + action rows.
- **Sidebar:** lavender gradient, user chip, gradient nav icons, rounded selection.
- **Empty / unavailable states:** new vertically-centred component — gradient badge +
  section icon + title + subtitle (Bluetooth off, no sound card, no storage, no apps).
- **Content column:** widened to 600px, centred, consistent 32px gutters.
- **Controls:** gradient switches/scales/level-bars, pill buttons, accent gradient CTAs.

## Dock
Root-caused the floating dock: the `c: wbar <options>` line in `dot.wbar` must precede
all icon triples, otherwise `-pos bottom` is ignored and wbar defaults to `-pos right`.
Fixed ordering + `-nofont` (icon-only). wbar computes its centre from the live screen
width (it reads the root-window width at runtime; no code path special-cases a width),
so centring is resolution-adaptive by construction. Verified with exact QEMU captures:
- 1024×768 ✓  1280×720 ✓  1280×768 ✓  — dock centred, no float/clip/stretch/drift.
- 1366×768 / 1360×768 — the wbar centring math is identical at any width, but Xvesa on
  QEMU stdVGA/bochs cannot present widths above 1280 (its VBE mode list caps there; 1366
  is also not 8-aligned) and falls back to 1280×768. On real 1366 panels whose BIOS
  advertises the mode, `xres=1366x768` drives Xvesa directly; true KMS 1366 is future work.

## Tiny Core components replaced / removed
- Tiny Core Control Panel launcher/menu entry (`tinycore-cpanel.desktop`) removed.
- Dock rebuilt around xxri Settings; `wbar_setup.sh` de-branded.

## Verification
- Fast host render loop (Xvfb + bubblewrap + stub backends) — same i686 binary against
  the exact device GTK 3.24.07 libraries; all 11 pages + empty states screenshotted.
- On-device: booted the installed image in QEMU; Settings launched on Xvesa with live
  `xxri-network` data and rendered identically to preview. flwm decoration + centred dock.

## Adaptive resolution
`.xsession` now honours an `xres=WxH` boot code (default 1024×768) so the desktop can
match the panel.

## Artifacts
- `rootfs/usr/local/bin/xxri-settings` — GTK3 binary (i686)
- `rootfs/usr/local/share/xxri-settings/xxri.css` — theme
- `output/xxri-disk.img` — installed system (rebuilt)
- `output/core.gz`, `output/XXRI-Lite.iso` — live/installer (rebuilt)
