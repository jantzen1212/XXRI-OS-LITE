# Phase 8.1 — Settings Launcher / Dock Integration & Desktop QA

## Scope
Fix desktop-integration regressions (no UI redesign): duplicate Settings icons in
the dock, "clicking Settings does nothing", the installer persisting on installed
systems, and general launcher hygiene. Verified on the **installed system** booted
in QEMU (not "verified by code").

---

## Bugs found → root cause → fix

### 1. Three "Settings" icons in the dock  ← primary bug
**Root cause (three compounding sources):**
1. `dot.wbar` listed Settings once (correct), **but** `setupdesktop` then iterates
   every non-`tinycore-*` `.desktop` in `/usr/local/share/applications` and calls
   `desktop.sh APPNAME`, which appended each app to the dock via `wbar_update.sh`.
   That re-added **Settings, Terminal and Wifi** on top of `dot.wbar` → dock grew to
   9 icons with a duplicate Settings gear and a duplicate Terminal.
2. The old `wbar_setup.sh` additionally ran `wbar_update.sh xxri-settings` and an
   on-demand loop, appending more icons.
3. `cpanel.png` was **byte-identical** to `xxri-settings.png`, so the leftover Tiny
   Core Control Panel launcher rendered as a *third* Settings gear.

**Fix (eliminated the sources, did not hide duplicates):**
- `rootfs/usr/local/share/wbar/dot.wbar` — rewritten as the **complete authoritative
  dock**: 6 icons (Settings, Files, Terminal, Editor, Apps, Exit), each exactly once,
  with full-path `c: exec /usr/local/bin/<app>` commands.
- `rootfs/usr/local/bin/wbar_setup.sh` — now copies `dot.wbar` verbatim and appends
  **nothing** (removed the tinycore loop, the `wbar_update xxri-settings`, the
  on-demand loop).
- `rootfs/usr/local/bin/desktop.sh` — **root-cause fix**: builds the Applications
  **menu only** (`flwm_makemenu`); no longer calls `wbar_update.sh`, so the per-app
  pass can never touch the dock again.
- Removed `cpanel.png` + all Tiny Core config launchers (see #4).

**Verified:** dock is now exactly 6 icons, one Settings — screenshot + the live
`/usr/local/tce.icons` dumped from the running system (`proof.txt`).

### 2. Clicking Settings sometimes did nothing
**Cause:** the duplicate/`cpanel` gears — clicking the dead cpanel gear (app removed)
did nothing; bare `xxri-settings` also relied on `$PATH`.
**Fix:** one working launcher; `Exec=/usr/local/bin/xxri-settings` (absolute path,
no `$PATH` dependency) in both `dot.wbar` and `xxri-settings.desktop`.
**Verified:** Settings launches and paints the Wi-Fi page on the installed system,
with live backend data (eth0 · 10.0.2.15, gateway 10.0.2.2, dns 10.0.2.3).

### 3. Installer ("Install xxri OS Lite") persisted after installation
**Cause:** `install2disk` copies the whole rootfs — including
`xxri-installer.desktop`, the `.X.d` auto-start hook and the pre-built
`~/.wmx/*/InstallxxriOSLite` menu entry — to the target; the installer was
deliberately kept available.
**Fix (installer is now Live-ISO-only):**
- `rootfs/usr/sbin/install2disk` — strips the installer `.desktop`, icon, GUI binary,
  `.X.d` hook and menu entries from the installed target after the copy.
- `build-writable-image.sh` — same removal for the baked installed image.
- `rootfs/etc/init.d/xxri-hw-init` — boot-time safety net: on a persistent
  (non-`tmpfs`/`rootfs`) root, remove any installer launcher/menu; on the Live ISO it
  is kept.
**Verified:** installed menu Applications = `Terminal, Wifi, XXRISettings` (no
installer); installer `.desktop` + binary absent. Live ISO rootfs still ships the
installer.

### 4. Leftover Tiny Core control-panel launchers
`cpanel`, `services`, `settime`, `tc-wbarconf` `.desktop`/pixmap removed in the image
build and `xxri-hw-init`; xxri Settings replaces them.

---

## Files modified
| File | Change |
|---|---|
| rootfs/usr/local/share/wbar/dot.wbar | authoritative 6-icon dock, full-path exec |
| rootfs/usr/local/bin/wbar_setup.sh | copy dot.wbar verbatim, no appends |
| rootfs/usr/local/bin/desktop.sh | **new override** — menu only, never touch dock |
| rootfs/usr/local/share/applications/xxri-settings.desktop | canonical (Exec=full path, Terminal=false, Categories=Settings;) |
| rootfs/usr/sbin/install2disk | strip installer from installed target |
| rootfs/etc/init.d/xxri-hw-init | remove cpanel/config launchers + installer on installed systems |
| build-writable-image.sh | purge cpanel + installer from baked image; re-apply desktop.sh |
| rootfs/usr/local/share/pixmaps/cpanel.png | deleted (duplicate gear) |

Artifacts rebuilt: `output/xxri-disk.img`, `output/core.gz`, `output/XXRI-Lite.iso`.

---

## Desktop-integration audit (installed system, from `proof.txt`)
- `which xxri-settings` → `/usr/local/bin/xxri-settings` (exists, executable, `STARTED_OK`).
- `xxri-settings.desktop`: `Type=Application`, `Exec=/usr/local/bin/xxri-settings`,
  `Icon=xxri-settings`, `X-FullPathIcon=…`, `Terminal=false`, `Categories=Settings;`.
- `gtk-launch` present at `/usr/local/bin/gtk-launch`.
- Dock `/usr/local/tce.icons` = 6 icons, one Settings, **cpanel: 0**.
- Menu Applications = `Terminal, Wifi, XXRISettings`; SystemTools = `Apps, Editor, Exit,
  MountTool, RunProgram, ScreenShot, Top, Xkill` (no cpanel/installer/duplicates).
- All 6 dock launchers point to existing binaries + existing pixmaps (no dead launchers,
  no missing icons).

## Browser / Files / Store
There are **no** Browser or Store launchers in the build (those names came from the
Settings "Apps" page stub data used during development), so no duplicates are possible.
"Files" = `mnttool`, present exactly once.

## GTK verification (host render harness + on-device)
GTK3 starts, `xxri.css` loads, Avenir Next loads, nav/app icons load, no missing
assets, no crash. All 11 Settings pages render (re-verified post-8.1). On-device the
Wi-Fi page renders identically with real `xxri-network` data.

## Reboot persistence
By construction the dock cannot accumulate: `wbar_setup.sh` does `rm -f tce.icons;
cp dot.wbar tce.icons` every boot, and `desktop.sh` writes idempotent menu files.
Booted the installed system multiple times — dock stays at 6 icons; Settings launches
on each boot.

---

## Screenshots captured
- Installed desktop + dock (before fix: 9 icons/2 gears; after fix: 6 icons/1 gear) — before/after montage
- Settings opened on the installed system (Wi-Fi page, real data)
- Live `tce.icons` + menu dump (`proof.txt`)

## Limitations / not fully automated
- The **full interactive install flow** (boot ISO → drive the GUI installer → install)
  was not driven end-to-end headlessly; instead the equivalent installed image and the
  `install2disk` removal logic were verified directly.
- Individual menu apps (Editor, MountTool, ScreenShot, etc.) were verified structurally
  (valid Exec + icon) rather than each clicked; Settings/Terminal/Apps were exercised.
- The QEMU KVM backend intermittently raised `KVM: entry failed, hardware error 0x0`
  (a host-emulation issue, not an OS defect); worked around with boot retries.
- 1366×768 display remains capped to 1280×768 by Xvesa/QEMU VBE (from Phase 8).
