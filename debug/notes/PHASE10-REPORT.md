# Phase 10 — Desktop polish, XXRI window decorations, Control Center

Status: implemented and verified in QEMU on the real i686 image.
Design authority: `assets/mockup/mockup1.jpg` and `mockup3.jpg`, normalised to
the desktop's own 1024x768 before measuring (`/tmp` work files) so every number
below is in screen pixels, not mockup pixels.

---

## 1. XXRI window decorations

Tiny Core's flwm draws a rotated titlebar down the **left** edge of every window
with FLTK bevel buttons. That was the single most obvious "this is Tiny Core"
signal on the desktop. It is gone.

**What was built.** `work/flwm-xxri/` is flwm's own source
(`git.tinycorelinux.net/flwm`, the tree Tiny Core builds `flwm.tcz` from) with an
XXRI decoration patch compiled in as `-DTOPSIDE -DXXRI`. It is cross-built for
i686 against the target's own FLTK 1.3 (`work/flwm-xxri/build.sh`) and ships as
`rootfs-overrides/usr/local/bin/flwm`; the stock binary is kept beside it as
`flwm-stock` for recovery.

It keeps the name `flwm` deliberately: `flwm_restart` does
`kill -s USR2 $(pidof flwm)` and `desktop.sh` calls `"$DESKTOP"_makemenu`, so
renaming the window manager would have silently broken app menu registration.

**The decoration** (measured off the mockup):

| element | value |
|---|---|
| title bar height | 26 px, flat `#FAF9FE` active / `#EDEAF6` inactive |
| separator | 1 px hairline `#E3DFF2` under the bar |
| frame | single 1 px outline `#D9D4EC`, 3 px sides / 4 px bottom (still grabbable for resize) |
| title | Avenir Next Bold 13 px, `#1C1B24` active / `#8A87A0` inactive |
| controls | three at the left: ◀ triangle `#E624CE` (minimise), ■ rounded square `#7B22F0` (maximise/restore), ● circle `#2A22EE` (close) |
| control states | hover = lighter glyph + soft rounded halo; pressed = darker; close reveals a knocked-out ✕; disabled = `#C7C3D8` |

**Behaviour changes over stock flwm**

- One maximise control for **both** axes (stock flwm had separate width/height
  toggles and a window-shade), with a per-frame restore rectangle.
- Maximise and autoplacement both stop above the dock
  (`XXRI_DOCK_RESERVE`, default 78 px, exported by `.xsession`), so a maximised
  window never disappears behind the dock and a new window never opens under it.
- Dialogs (transient windows) get only the close control.
- The desktop menu is white with the XXRI face instead of FLTK's grey bevels
  (`Fl::background()` — setting `FL_BACKGROUND_COLOR` alone does nothing because
  FLTK boxes draw from the grey *ramp*), and "New xterm" is gone.
- `Ctrl + Alt + C` toggles the Control Center.

**Verified** with real X clicks on the host harness (`work/wm-test.sh`):
maximise → `1024x690` (screen minus dock reserve; with `XXRI_DOCK_RESERVE=200`
it becomes `1024x568`, proving the reserve is honoured), restore → original
`986x690`, minimise → `IsUnviewable`, close → client exits, and hover/press
states captured per control. In QEMU: every window in every scene carries the
XXRI bar — Settings, Store, the power menu, the FLTK dialog, aterm and the
editor.

## 2. XXRI Control Center

`rootfs/usr/local/src/xxri-control-center/` (GTK3, i686, ~26 KB) plus
`rootfs/usr/local/share/xxri-control-center/xxri-cc.css`.

Bottom-right, exactly where the mockups put it (right margin 18 px, bottom
margin 20 px). One override-redirect window with two states:

- **collapsed** — a 126x38 pill: network glyph, Bluetooth glyph, battery ring
  (only if a battery exists) and the clock. This is the always-available
  trigger, in the corner the brief asked for.
- **expanded** — a 264x150 panel: Wi-Fi/Ethernet and Bluetooth tiles, a volume
  row, a brightness row when the hardware supports it, then settings, power,
  battery and the clock/date.

Everything is read and written through the Phase 7 backends —
`xxri-network status`, `xxri-bluetooth status`, `xxri-audio volume|toggle`,
`xxri-power info|brightness`, and `xxri-settings`/`xxri-power-menu` for the
actions. No hardware logic is duplicated, and nothing is faked: with no
Bluetooth adapter the tile reads **Unavailable** in the disabled style, with no
sound card the volume row is replaced by **No audio device**, and with no
battery the ring is simply absent.

Mechanics that matter on this device:

- **Rounded corners without a compositor** — the desktop is not composited, so a
  transparent GTK background paints black. The window is clipped to a rounded
  rectangle with the X Shape extension (pill radius when collapsed, 16 px when
  expanded).
- **No focus theft, no duplicates** — `GTK_WINDOW_POPUP` + dock type hint +
  `accept_focus=false`; a second launch writes a command
  (`open|close|toggle|quit`) next to its pid file and signals the running
  instance instead of starting a second panel. `xxri-control-center --close`
  with nothing running exits immediately (it used to fall through and start a
  panel, which hung a QA hook).
- Opening re-reads hardware state; collapsed it updates the clock **in place**
  every 30 s and re-reads hardware every ~2 min, rebuilding only when something
  actually changed. (It used to rebuild the whole widget tree on every tick,
  which flickered - a screendump taken inside that window caught an empty
  corner and sent me looking for a crash that was not there.)
- It collapses 1.4 s after the pointer leaves, on a second click, or after an
  action. `XXRI_CC_DEBUG=1` appends a timeline to `~/xxri-cc.log`, which is how
  the flicker above was diagnosed without a terminal.
- The Wi-Fi/Ethernet tile shows the SSID on Wi-Fi and the IP address on a wired
  link; "eth0" told the user nothing they did not already know.
- The live installer hides it (`session_exclusive`) and restores it with the
  dock on "Continue Live".

## 3. Dock

Polished, not rebuilt. `dot.wbar` options are now
`-offset 15 -isize 40 -idist 7 -zoomf 1.25 -nanim 3 -jumpf 0.0`.

| | mockup | before | now |
|---|---|---|---|
| icon size | 40 | 44 | 40 |
| pill height | 48 | 52 | 52 |
| bottom margin | 26 | 11 | 26 |
| centred | yes | yes | yes (`338x63+343+690` on 1024, centre 512) |

`-offset` positions wbar's *window*, whose bottom 11 px are zoom headroom, so
the visible pill needed offset 15 to land on the mockup's 26 px margin. Hover
zoom is now a restrained 1.25 with no jump. The dock still grows correctly with
installed apps (`385x63+319+690` with Leafpad added) and stays centred.

## 4. Settings

- Sidebar now fits **all twelve** entries at 660 px without scrolling (row
  padding 6 px, group gaps 3 px), each with the mockup's dotted rule; the
  selected entry keeps its white pill and hides its own rule.
- **Help & Support is a real page.** It used to fire `xxri-open-url` and leave
  the content on the previous page — a dead click on a device with no browser.
  It now shows edition/architecture/software source, links to the website and
  support (with a hint that a browser must be installed first), and the
  desktop's keyboard shortcuts.
- **Volume bug fixed** (found by looking at the panel, not the code): the
  backends wrap payloads in an envelope whose key repeats inside it —
  `{"volume":{"volume":65,...}}` — and the flat `jget()` stopped at the
  envelope and parsed `{` as the number. Both Settings' Sounds page and the
  Control Center showed 0 % for a mixer sitting at 65 %. `jget()` now skips
  values that open an object or array.
- Scrollbars: no stepper buttons (the theme drew them as white pills at both
  ends), transparent trough, 6 px slider.

## 5. Store

Backend and remote repository untouched — still
`https://repo.xxri.flows.best/i686.json`.

- **Fits a 1024x768 panel.** The header's minimum width was 1097 px (a 34-char
  search entry), so the window opened wider than the screen with the search box
  hanging off the right edge. The entry is now 16 chars and expands into
  whatever room is left, and the status chip ellipsizes. Window: `944x648`.
- **No scroll furniture under the rails.** Every rail had a permanent grey
  scrollbar strip; the rails are `GTK_POLICY_EXTERNAL` now (wheel and drag still
  scroll, "See all" opens the full list).
- Metadata lines join only the fields that exist — an app with no license or
  size used to render `Graphics ·` with a dangling separator.
- Icons: the 57/57 repository icon work from the previous session is intact;
  app cards and detail pages show real logos.

## 6. Tiny Core artifacts removed

| leak | what happened |
|---|---|
| left-side flwm titlebars | replaced by the XXRI bar (§1) |
| "TC Exit Options" dialog | replaced by `xxri-power-menu` (shut down / restart / sleep when supported / log out), wired to the dock's Power icon |
| menu entries `Exit`, `RunProgram`, `Top`, `Xkill`, `Wifi` | dropped in both paths (`build-writable-image.sh` step 2d and `xxri-hw-init` for live) |
| `tinycore-editor`, `tinycore-mnttool`, `tinycore-screenshot` | replaced by `xxri-editor`, `xxri-disks`, `xxri-screenshot` entries, and the two FLTK apps are launched with XXRI colour switches |
| dock label "Files" for Tiny Core's mount tool | now **"Disks", opening Settings > Storage**. Tiny Core's `mnttool` exits immediately on this build - no process, no window, empty log - so the dock icon was dead. Storage already does mount/unmount/eject natively. |
| terminal titled "xxri Terminal", plain black | "XXRI Terminal" on the palette's `#16112B` panel (pseudo-transparency needed a root pixmap that never existed, so it fell back to black). Its rxvt scrollbar - a dithered grey trough this aterm build draws regardless of `scrollColor`/`scrollstyle` - is hidden; Shift+PageUp/PageDown still walk the 2000-line scrollback, and Settings' Help page says so. |
| `NAME="xxri OS Lite"`, `xxri-os.org` URLs | `XXRI OS Lite 2.0`, `xxri.flows.best`; `VERSION_ID` deliberately stays `16.0` because `xxri-functions` builds the package mirror path from it |

Desktop entries on the built image: 8, of which **0** are Tiny Core-named.

## 6b. Copy and empty states

- The Store's Installed and Downloads pages used a hint bar at the top of an
  otherwise blank page, which reads as an error; both now use the same centred
  empty state as Settings (badge with the page's own rail glyph, a bold line, a
  muted explanation).
- Settings' Apps page no longer points at "the upcoming Software Store" - the
  Store shipped two phases ago.
- Bluetooth's empty state said "Bluetooth is off" even with no radio present.
  The title now distinguishes no adapter / stack not installed / blocked, and
  the backend's lowercase reason is presented as a sentence (without doubling
  its full stop).
- About showed `Locale: C.` - `xxri-hardware` built the field as
  `LANG.TZ` and `sed` prints nothing while still succeeding, so the separator
  was emitted for missing files. The backend now joins only what exists, and
  Settings renders a bare `C` as `C \xc2\xb7 POSIX default`.
- The Store's metadata lines join only the fields that exist.

## 7. Typography and scaling

- The XXRI face is published to the X server as the `xxri` foundry. Xvesa got it
  via `-fp`; **X.Org never had it** — every FLTK surface (window titles,
  dialogs, the desktop menu) was silently falling back to a stock bitmap font.
  `.xsession` now runs `xset +fp /usr/local/share/fonts/xxri/` + `fp rehash`
  when the session is X.Org.
- GTK renders at Xft.dpi 96 (`.Xdefaults`) while X.Org runs at `-dpi 75` for
  core fonts. Both were verified visually at every tested resolution; the mixed
  setting is deliberate and documented here rather than changed, because the
  whole design system was measured against the current rendering.

## 8. Resolution matrix

| requested | actual | dock | Control Center | windows |
|---|---|---|---|---|
| 1024x768 | 1024x768 | `338x63+343+690`, centred | chip `126x38+880+710` | Settings `986x690`, Store `950x678` |
| 1280x720 | **1280x800** | centred | correct margins | fine |
| 1280x768 | 1280x768 | centred | correct margins | fine |
| 1360x768 | 1360x768 | centred | correct margins | two windows side by side, neither under the dock |

1280x720 is not in the virtual GPU's mode list (`1280x800 1920x1080 1600x1200
1680x1050 1400x1050 1280x1024 1440x900 1280x960 1360x768 1280x768 1024x768
800x600 640x480`), so `apply_xres` correctly falls back to the panel's preferred
mode. 1360x768 covers the 1366x768 class.

## 9. Regression results

Run on the built image in QEMU (`work/regress-hook.sh`):

| subsystem | result |
|---|---|
| window manager | running, `/usr/local/bin/flwm` (XXRI build) |
| dock | running, geometry correct, grows with installed apps |
| Control Center | running, one instance |
| X.Org + XInput2 | present, 1024x768 |
| desktop entries | 8, zero Tiny Core-named |
| Store install (`leafpad`, tcz) | 92 s → registry ✓, `INTEGRATED=1`, `.desktop` ✓, dock triplet ✓, flwm menu item ✓, icon 2374 B |
| `xxri-app verify-icons` | `OK` — registry/desktop/dock/store md5 all `4c5acce8` |
| persistence | after reboot the dock is still `385x63+319+690` with Leafpad |
| live ISO | boots to the installer welcome screen; Control Center correctly hidden during install |
| live ISO desktop | **the live session runs the XXRI window manager too.** This was nearly missed: the WM lived only in `rootfs-overrides/`, which `build-writable-image.sh` applies to the *installed* image, so a live boot would have kept Tiny Core's rotated titlebar. It is now in `rootfs/` as well (extensions load with `cp -ai`, so the initrd copy wins over `flwm.tcz`). Proven on a QA ISO that dismisses the installer: `/usr/local/bin/flwm` is a real 59520-byte file, not a tcloop symlink, `flwm` is running from it, and Settings comes up with the XXRI bar and the Control Center chip (`debug/phase10/live-session-decorated.png`) |
| Xvesa fallback | untouched — `.xsession` still falls back when X.Org has no input driver or no `/dev/dri/card0` |

## 10. Screenshots

25 captures in `debug/phase10/`, including: `1024x768-desktop`, `-cc`,
`-settings-about`, `-settings-help`, `-settings-sound`, `-store-home`,
`-store-app-gimp`, `-dialog`, `-power`, `-tools`, `-two`, `-installed`;
`1280x768-*`, `1280x720-*`, `1360x768-*`; `live-installer`, `wm-maximized`,
`wm-desktop-menu`, `controls-vs-mockup`.

## 11. QA harness (kept, not shipped)

| script | purpose |
|---|---|
| `work/wm-shoot.sh` | render the WM + any client under Xvfb/bwrap on the host, screenshot the root window (3 s per iteration) |
| `work/wm-test.sh` | drive the titlebar controls with real X clicks and report geometry/map state after each |
| `work/ui-shots.sh` | boot the image and screendump each scene when the guest announces `SHOTREADY` over the serial line; `IMG=` aims a run at a copy |
| `work/ui-qa-hook.sh` | the guest-side scene driver (desktop, cc, settings:*, store:*, dialog, power, tools) + a geometry report per scene |
| `work/regress-hook.sh` | the subsystem regression probe above |
| `work/inject-hook.sh` | install a hook into **both** `/etc/skel/.X.d` and `/home/xxri/.X.d` (forgetting the second silently runs the old hook) |
| `work/ui-pointer.sh` | move/click the guest pointer through the QEMU monitor. Kept for completeness, but see the limitation below: the injected motion never reaches this guest's X server. |

None of these are part of the release image; `output/xxri-disk.img` is rebuilt
from `rootfs/` and carries no QA hook.

## 12. Known limitations

- Interaction with the titlebar controls is proven on the host harness, which
  can synthesise clicks; the device has no `xdotool`, so on-device evidence for
  the controls is visual (rendering, states) plus the identical binary.
  QEMU-monitor pointer injection (`work/ui-pointer.sh`) was tried and does not
  work here: the Control Center's own trace recorded no click for any injected
  press, matching the earlier finding that this guest ignores synthetic
  pointer input. The Control Center's expand/collapse path *was* exercised on
  the device through its command interface, which is the same code a click
  runs.
- 1280x720 cannot be tested under QEMU's virtual GPU (not in its mode list).
- `editor` and `mnttool` are still Tiny Core FLTK apps, restyled with FLTK
  colour switches and renamed. Replacing them with XXRI-native apps is a later
  phase (explicitly out of scope here).
- Installed applications keep their published icon (no forced squircle frame),
  so a non-square upstream logo sits unframed among the squircle dock icons.
- The desktop menu uses FLTK's own popup metrics; only its palette, font and
  entries are XXRI.
- The Control Center has no animation. There is no compositor, and the brief
  ranks a clean instant transition above a broken animated one.
