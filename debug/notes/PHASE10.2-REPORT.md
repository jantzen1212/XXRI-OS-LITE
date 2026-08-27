# Phase 10.2 — XXRI Desktop Shell Stabilization

Scope: make the existing XXRI desktop shell stable, functional and polished.
No new applications, no architecture changes. Everything below was verified on
the built image running in QEMU, driven with **real pointer and keyboard input**
(QEMU `-device usb-tablet` + QMP `input-send-event`), not by inspecting files.

---

## 1. Headline: the three reported faults had two root causes, both in flwm

The user-visible complaints "the window manager disappears", "decorations
vanish" and "the dock sometimes does nothing" were not three problems. They
were two bugs in the window manager, and the second was **hidden by the first**.

### Root cause A — heap overflow in the desktop menu (`Menu.C`)

`ShowTabMenu()` counts the menu items, allocates an array of exactly that many
`Fl_Menu_Item`s, then fills it. The count and the fill disagreed:

```c
int n = num_other_items;
if (num_wmx) {
    n -= 1;          // "delete New xterm"   <-- unconditional
```

The subtraction removes the built-in "New xterm" entry — but XXRI sets
`XTERM_MENU_ITEM 0` (the dock owns the terminal), so that entry does not exist,
and the matching `memcpy` at the bottom of the function is `#if`'d out. The
array was therefore allocated **one `Fl_Menu_Item` too small** and the list
terminator was written past the end of the heap block, every single time the
desktop menu was opened with a populated `~/.wmx`.

This is exactly the class of bug the brief warned not to repeat, and it was
still live in the shipped binary.

Reproduced deterministically before the fix, with a real `~/.wmx` tree copied
from the installed image (8 launchers + `SystemTools`):

```
== wm=/tmp/claude-1000/flwm-base  rounds=3  wmx entries=10
  startup                            wm=up
-- round 1
  menu open/close (no client)        wm=DOWN
*** WINDOW MANAGER DIED
--- wm stderr ---
malloc(): invalid size (unsorted)
```

**One click on the desktop killed the window manager.** On-device evidence:
`debug/phase10.2/BEFORE-wm-crash-decorations-gone.png` — the XXRI titlebar is
gone and the window has jumped to y=0, undecorated.

Fix: tie the subtraction to `XTERM_MENU_ITEM`, collapse the two `memcpy`
branches into one clamped copy, and carry 8 spare slots so any future miscount
wastes a few hundred bytes instead of destroying the desktop.

### Root cause B — one Escape keypress disabled the whole desktop (`main.C`)

At the top of flwm's global event handler:

```c
if (Fl::event_key()==FL_Escape) { return 1; }
```

`Fl::event_key()` is **sticky** — it keeps reporting the last key seen. So after
any Escape press this handler claimed to have handled *every* event that
followed, including the raw X events it exists to process:

* `ButtonPress` — clicks stopped working desktop-wide
* **`MapRequest` — newly launched windows were never mapped**

That last one is the whole of "the dock sometimes does nothing": the click was
received, the application really started, and its window was silently never put
on screen. Isolated with a before/after probe:

```
== probe 1: fresh desktop, no menu has ever been opened
  probe 1-fresh: CLICK WORKED
== action: open the desktop menu, dismiss with ESC
  probe 2-after-esc: NO RESPONSE          <-- dock, titlebar, menu: all dead
== action: open the menu, dismiss by clicking away
  probe 3-after-click-away: CLICK WORKED
```

Fix: only swallow Escape when the event actually is a keyboard event, and let
ALT+Escape through (it is a documented hotkey).

**Bug A masked bug B.** Before the fix the WM died on the first menu, which
released everything; once the WM survived, the stuck-Escape state could persist
and the dock went dead. Fixing stability is what made this one observable.

---

## 2. Subsystems

Worked in the order the brief specifies, with build → boot → test → screenshot
between each. Nothing was batched.

| # | Subsystem | Result |
|---|---|---|
| 1 | flwm stability | **Fixed.** 8-round host soak: 98 liveness checks, 0 deaths. On-device: 6 menu open/close cycles, then a 15-click dock run, WM alive throughout |
| 2 | Window decoration | **Fixed.** Close control now visibly contains an X; titlebar font corrected |
| 3 | Minimize / restore | **Implemented.** New `xxri-wctl` + `xxri-dock-launch` |
| 4 | Dock running indicators | **Implemented.** macOS-style dash, running vs focused |
| 5 | Dock reliability | **Fixed** (root cause B). 15/15 correct clicks over 3 cycles |
| 6 | Control Center trigger | **Implemented.** Bottom-right hover, no click |
| 7 | Control Center polish | **Improved** (glass). Layout already matched the mockup |
| 8 | Transparency | **Implemented as pseudo-transparency.** True transparency proven impossible on this server |
| 9 | Animation | **Deliberately not added** — see §7 |
| 10 | Final QA | Screenshots + resolution matrix below |

### Subsystem 2 — the close control

The mockup draws the three controls as △ □ ● (triangle, rounded square,
**circle**). The brief overrides the mockup and requires △ □ **X**. The circle
is kept — it is the XXRI visual language — with an X knocked out of it in the
bar colour, present at rest and thickening on hover:

`debug/phase10.2/titlebar-close-is-X.png`

Also fixed here: **the titlebar was drawing in a bitmap fallback font, not
Avenir Next.** `.xsession` added the XXRI font path with `xset +fp` — but
**`xset` does not exist anywhere in the image**, so `command -v xset` failed and
the line silently did nothing. Under X.Org the server had never heard of the
`xxri` foundry, so flwm's `-xxri-helvetica-bold-r-normal--*` request fell back.
(GTK apps were unaffected: they use fontconfig.) The font path is now set on the
X.Org **command line** (`-fp`), which removes the ordering question entirely;
`xset` is also now shipped.

### Subsystems 3 & 4 — giving the dock window awareness

wbar is a launcher and nothing else: no window tracking, no per-icon state. Per
the brief it was **not** rewritten. Instead a small helper supplies what it
lacks, and the dock's own command lines route through it:

* **`xxri-wctl`** (new, 320 lines C, `rootfs/usr/local/src/xxri-wctl/`)
  `list` / `running KEY` / `activate KEY` / `indicators`.
  flwm is ICCCM-only and publishes no `_NET_*` properties, so everything is
  plain ICCCM: a window is "managed" exactly when it carries `WM_STATE`, which
  also neatly excludes the dock and Control Center (override-redirect).
  `activate` sends `WM_CHANGE_STATE`/`NormalState`, which flwm answers with
  `Frame::raise()`. Mapping the client directly does **not** work — flwm
  reparents it into a frame it owns, so while iconic the frame is unmapped and
  a map of the client inside it is invisible and unnoticed.
* **`xxri-dock-launch KEY CMD…`** (new, shell) — focus the existing window if
  there is one, otherwise launch. `+KEY` = always open a new window but still
  indicate (terminal, editor); `-` = not an application (power menu).
* **Indicators** — one tiny override-redirect window per slot, its *background
  colour* being the indicator, so there is no drawing code, no expose handling
  and nothing to flicker. Position is derived from wbar's real window geometry
  each poll, not hard-coded.

Verified on-device, from the guest's own window list:

```
1. clean desktop            windows: (none)
2. launch Settings          windows: xxri-settings(normal,focused)
3. launch Terminal          windows: xxri-settings(normal) aterm(normal,focused)
4. minimise Settings        windows: xxri-settings(iconic) aterm(normal,focused)
5. click Settings dock icon windows: aterm(normal) xxri-settings(normal,focused)
6. click it again           windows: aterm(normal) xxri-settings(normal,focused)
```

Step 5 restores rather than launching a second copy; step 6 does not duplicate.
Indicator states across launch / focus / minimise / restore:
`debug/phase10.2/dock-indicator-states.png`.

### Subsystem 5 — dock reliability, and a wbar trap

Fixed by root cause B. Two further findings:

* **Turning the icon zoom off breaks the dock.** The zoom (`-zoomf 1.25`) slides
  icons sideways as the pointer approaches, which both moves the click target
  and desynchronises the indicators — so the obvious fix was `-zoomf 1.0
  -nanim 0`. That **kills wbar's hit-testing completely**: the dock goes
  entirely dead. Settled on `-zoomf 1.06 -nanim 3`: clicks work, icons barely
  move, indicators stay aligned. This is precisely why the brief says not to
  rewrite wbar blindly.
* **The Editor threw a modal error dialog on every launch.** Its dock command
  passed `-scheme gtk+ -bg … -bg2 … -fg …`, which this `editor` build does not
  parse as options — it tried to *open* `-scheme` as a file:
  *"Error reading from file '-scheme': No such file or directory"*. A modal
  dialog appearing on every launch is itself an input-blocking hazard. The
  unsupported arguments were removed.

Final run — 3 cycles × 5 launchers, asserting the *correct* semantics (a
single-instance app clicked while already open must FOCUS, not duplicate):

```
===== dock clicks: 15 correct, 0 wrong
  Store    : OK (launched) / (focused existing) / (focused existing)
  Settings : OK (launched) / (focused existing) / (focused existing)
  Disks    : OK (focused existing) ×3
  Terminal : OK +new window ×3
  Editor   : OK +new window ×3
```

### Subsystem 6 — Control Center opens on hover

Two independent triggers, both verified on-device:

* **Hovering the chip** now expands the panel. The existing `on_enter` handler
  only cancelled the collapse timer; it never opened anything.
* **The extreme corner.** The chip is inset by `EDGE_X`/`EDGE_Y`, so shoving the
  pointer into the corner — which is what people do, a corner needs no aim —
  missed it. An input-only X window was tried first and never received its
  crossing events; the shipping implementation polls the root pointer every
  150 ms, which reports the position regardless of which window is on top,
  cannot swallow a click and cannot be stacked underneath anything.
* The explicit chip click still toggles. Collapse keeps its 1400 ms grace period.

`debug/phase10.2/1024x768-cc-hover.png`, `…-cc-click.png`.

Panel contents come from the Phase 7 backends unchanged — Ethernet 10.0.2.15
read live, Bluetooth shown as **Unavailable** rather than faked.

### Subsystem 8 — transparency

Measured on the target before writing any code:

```
extensions : Composite, DAMAGE, RENDER, SHAPE, XFIXES, Present … (26)
depth of root window : 24
32-bit ARGB visuals  : 0
compositing manager  : none (_NET_SUPPORTING_WM_CHECK not found)
_XROOTPMAP_ID        : pixmap id # 0x600001
```

So **true transparency is impossible here** — no ARGB visual and no compositor;
an ARGB window would paint black. But the wallpaper *is* published as a root
pixmap, so the panel is made translucent the way X applications did it before
compositors existed: the region behind the panel is read out of `_XROOTPMAP_ID`,
blurred by scaling down and back up, tinted with the XXRI surface colour, and
used as the panel background, with the CSS gradient now semi-transparent on top.
Real frosted glass, no compositor, no new dependency, and a flat-colour fallback
if the property is missing. `debug/phase10.2/control-center-glass.png`.

Honest limitation: the source is the wallpaper, not the screen, so a window
passing *behind* the panel is not seen through it.

---

## 3. Other defects found and fixed along the way

| Defect | Fix |
|---|---|
| `xset` absent from the image → XXRI font path never applied under X.Org | ship `xset`; set the font path on the `Xorg -fp` command line |
| Desktop menu ran Tiny Core's `exittc` | now runs `xxri-power-menu`; item renamed "Power" |
| Desktop menu offered "New desktop" | removed under `-DXXRI` (not an XXRI concept) |
| `logout_cb` forked without `_exit()` after a failed `execlp` | double-fork + `_exit`, so a failed exec cannot leave a second copy of the window manager running |
| Resolution flapped to 1280x800 mid-session | `apply_xres` asked RandR once and fell back to `--preferred` if the mode was not ready; it now retries before settling |
| Editor launcher error dialog | unsupported FLTK args removed |

---

## 4. Files changed

```
rootfs/etc/skel/.xsession                                 (font path, apply_xres retry, indicator daemon)
rootfs/usr/local/bin/flwm            + rootfs-overrides/  (rebuilt WM — both trees)
rootfs/usr/local/bin/xset                                 NEW (from Xlibs.tcz)
rootfs/usr/local/bin/xxri-wctl                            NEW
rootfs/usr/local/bin/xxri-dock-launch                     NEW
rootfs/usr/local/bin/xxri-control-center                  (rebuilt)
rootfs/usr/local/share/wbar/dot.wbar                      (launch wrapper, zoom, editor args)
rootfs/usr/local/share/applications/xxri-editor.desktop   (editor args)
rootfs/usr/local/share/xxri-control-center/xxri-cc.css    (semi-transparent panel)
rootfs/usr/local/src/xxri-wctl/{xxri-wctl.c,build.sh}     NEW
rootfs/usr/local/src/xxri-control-center/xxri-control-center.c, build.sh
work/flwm-xxri/{Menu.C, main.C, Frame.C}
```

Source diff: **+501 / −66** across 8 source files. The window manager change is
20 lines of drawing, 15 lines of event handling and 77 lines of menu allocation
— deliberately small, because it is the process everything else depends on.

QA harnesses added under `work/` (not shipped): `wm-stress.sh`, `wctl-test.sh`,
`cc-hover-test.sh`, `vm-grab.sh`, `vm-dockcycle.sh`, `vm-dock34.sh`,
`vm-final-qa.sh`, `vm-res-sweep.sh`, `vm-wait.sh`, and the guest hooks
`dockwatch-hook.sh`, `wlist-hook.sh`, `xcaps-hook.sh`, `font-diag-hook.sh`.

---

## 5. Interaction results (real pointer input)

**QEMU-monitor pointer injection was listed as impossible in Phase 10. It is
not** — `-device usb-tablet` plus QMP `input-send-event` reaches this guest.
Every row below was performed by injecting real events and reading back either a
screenshot or the guest's own window list.

| Interaction | Result |
|---|---|
| Drag a window by its titlebar | ✅ window follows, decoration stays attached |
| Minimize (△) | ✅ `WM_STATE` → iconic, window unmapped |
| Maximize (□) | ✅ fills screen, stops above the dock |
| Restore (□ again) | ✅ back to previous geometry |
| Close (X) | ✅ client exits (verified by pixel + window list) |
| Dock click → launch | ✅ 15/15 over 3 cycles |
| Dock click → focus running app | ✅ no second instance |
| Dock click → restore minimized app | ✅ |
| Control Center by corner hover | ✅ no click needed |
| Control Center by chip click | ✅ |
| Multiple windows | ✅ Settings + Store + Terminal, all decorated |
| Desktop menu open/close ×6, ×8 | ✅ WM alive, decorations intact |

---

## 6. Resolution matrix

| Requested | Actual | Result |
|---|---|---|
| 1024x768 | 1024x768 | ✅ full scene set |
| 1280x768 | 1280x768 | ✅ |
| 1280x800 | 1280x800 | ✅ |
| 1366x768 | 1360x768 | ✅ (QEMU's virtual GPU offers 1360x768, not 1366x768) |
| 1280x720 | **1280x800** | ❌ not in QEMU's mode list — falls back. Untestable here, as in Phase 10 |

36 screenshots in `debug/phase10.2/`.

---

## 7. Deliberate non-implementation: animation (subsystem 9)

No new animation was added, and this is a decision rather than an omission.
There is no compositor. Animating the Control Center's expansion would mean
re-running `gdk_window_shape_combine_region()` and re-deriving the frosted
backdrop on every frame, on a desktop with no compositing to hide the seams —
the likely result is tearing and flicker. The brief is explicit that "a stable
static transition is preferable to a broken animation" and that stability
outranks effects.

What *is* animated, and is stable: the dock's icon hover zoom (retained,
retuned to 1.06) and the titlebar controls' hover/pressed states.

---

## 8. Remaining limitations — stated plainly

* **Qt applications were not tested.** The acceptance list asks for it, but the
  image carries no Qt application to test with, and installing one was out of
  scope. GTK (Settings, Store), FLTK (editor, dialogs) and plain-X (aterm, wbar)
  clients were all exercised.
* **The Xvesa fallback path was not re-exercised this phase.** It is untouched
  by these changes and `.xsession` still falls back to it, but every test above
  ran under X.Org.
* **The live ISO was verified only as far as the installer welcome screen**
  (`debug/phase10.2/90-live-iso.png`) — it boots, the fonts are correct, and the
  new binaries are confirmed present in the initrd. The live *desktop* behind
  the installer was not re-driven this phase; the installed image was.
* **Disks and Settings share one identity.** They are the same program, so the
  Disks icon focuses an open Settings window and only one indicator lights.
* **The Editor identifies itself as `WM_CLASS` "FLTK"**, not "editor", so its
  indicator is keyed on a class other FLTK surfaces share. It is marked
  multi-instance so it can never focus a dialog instead of opening.
* `editor` and `mnttool` remain Tiny Core FLTK applications; with the broken
  style arguments removed the Editor now renders in stock FLTK grey rather than
  the XXRI palette. Replacing them is a later phase.
* Pseudo-transparency samples the wallpaper, not windows behind the panel.
* `.xsession` still exports `XXRI_STORE_DEBUG=1`, marked TEMPORARY since Phase 9.

---

## 9. Artifacts rebuilt

```
output/core.gz          15.6 MB
output/XXRI-Lite.iso
output/xxri-disk.img
```

All three rebuilt from the final source state; `flwm` is byte-identical in
`rootfs/` and `rootfs-overrides/`, and `flwm`, `xset`, `xxri-wctl`,
`xxri-dock-launch` and `xxri-control-center` are all confirmed present in the
live initrd.

Phase 11 not started. No new applications created.
