# Display layouts

With a second display connected, Settings > Display offers **Extend**,
**Mirror**, **_Built-in_ Only** and **_External_ Only**. The display names
come from what RandR reports. The choice applies at once, asks to be kept,
and is restored at every session start and whenever a display is plugged in
or out.

## Why the tester saw a mirrored desktop

The 2.7 fix for the HP g6 (tester-reports-2.7.md, "Two monitors") added
`mirror_others()` to `~/.xsession`. At every session start it put every extra
output `--same-as` the built-in panel, or switched it off when it lacked the
panel's mode. Nothing read a setting, and `xxri-display` only knew one output,
so Settings had no control to offer.

Mirroring had been chosen because an extended screen broke the shell. The
root window spans every display, and everything sized itself from it:

* wbar (Tiny Core's 2.3.0 binary) centres on `WidthOfScreen()`.
* The Control Center, launcher, login screen and the apps' own maximise
  (`xxri-chrome.h`) use `gdk_screen_get_width/height`.
* flwm maximises, auto-places and clamps to `Root->w()/h()`.

So the dock and Quick Control sat in the gap between the displays, and
maximise covered both. Two parts also measured the screen only once. flwm
kept clamping to the old root size, and xxri-compositor kept a back buffer of
the old size: growing the root left the whole screen black (reproduced in
Xephyr).

## How it works now

**Backend** (`xxri-display`). One `xrandr --query` pass reads every output.
The roles come from it, never from fixed names: "internal" is the first
connected eDP/LVDS/DSI panel, or else the first connected output; "external"
is the first other connected output.

| Layout | Screen |
|---|---|
| extend | the internal display at 0,0 as the primary; the others to its right, each at its own size (`MODE_<output>`, else its preferred mode) |
| mirror | every display at 0,0 in one mode: the desktop size if all offer it, else the internal display's preferred mode, else the largest shared one |
| internal / external | that display alone, the others off |

The primary display always sits at 0,0. Each layout is set with one `xrandr`
call and then checked. If the driver refuses half way, the previous screen
comes back. If two CRTCs can't drive three displays, the layout is retried
with the first external display only. If everything fails, the internal
display is lit at the desktop size, then at `--auto`. `set-layout` is a
trial: a watchdog (20 s) reverts it unless `confirm-layout` arrives, so a
display that shows nothing cannot strand the desktop even if Settings dies.

**Persistence.** `LAYOUT` (and `MODE_<output>`) is saved in `display.conf`;
nothing saved means mirror. `xxri-display apply` re-fits the saved layout to
whatever is connected and never leaves every display dark. It is run from
three places:

* `~/.xsession`, before anything is laid out (`--session`). This replaces
  `apply_xres`/`mirror_others`; the precedence xres= > saved MODE > 1024x768
  moved into the backend.
* `/usr/local/etc/X.d/xxri-display`, a system hook, so homes that keep a 2.7
  `.xsession` (skel is copied only into a new home) also get the saved
  layout. With a current `.xsession` it changes nothing.
* `xxri-wctl watch-displays`, started by that hook. When the set of connected
  outputs changes it runs `apply --hotplug`. Unplugging the only lit display
  brings the internal one back.

**Shell on the primary display.**

* wbar: `xxri-primary-screen.so`, loaded by `wbar.sh`, sets the default
  Screen's size to the primary RandR monitor's once wbar opens its display.
  LD_PRELOAD is dropped at load, so apps started from the dock see the real
  size.
* Control Center: chip and hot corner at the primary display's bottom right;
  it re-places itself on `monitors-changed`.
* Launcher drawer, login screen and installer: sized and centred on the
  primary display.
* `xxri-chrome.h` maximise: fills the window's own display, keeping the dock
  band only on the primary.
* flwm (patch 0004): reads the RandR 1.5 monitors. Maximise fills the
  display under the window, new windows open on the pointer's display, and
  the dock band is kept only on the primary. On a RandR screen change it
  takes the new root size; a maximised window refills its display, and a
  window less than a quarter visible comes back to the primary.
* xxri-compositor: rebuilds its back buffer when the root window resizes.
* After a live change, `xxri-display` re-renders the wallpaper (per display)
  and restarts the dock.

**Settings > Display.** With two displays connected, the picture shows each
display at its relative size: a laptop for the panel, a monitor otherwise,
dimmed when off. Below it come the Arrangement list (rows the hardware can't
do are greyed out with the reason), the main display's resolutions, and in
Extend a resolution menu for the second display that applies at once. After
a change a dialog counts down 17 s with Keep and Revert. The page rebuilds
when a cable is plugged in or out while it is open.

## Limits

* External displays always go to the right of the internal one. wbar
  snapshots the wallpaper behind it at the position it computes, which
  assumes the primary is at 0,0.
* Mirror needs a mode both displays offer. There is no scaling
  (`--scale-from`), because transforms without glamor are untested here.
* The primary display's resolution still restarts the session, as before.

## Tested

Nothing here was tried on real dual-monitor hardware.

* **Backend logic (simulated):** the device's own busybox against a stateful
  fake `xrandr`, 62 checks (`~/.xxri-testenv/displaytest/tests.sh [--slow]`).
  They cover one display, two displays, every layout, unplug and replug,
  driver refusal half way, CRTC limits, the trial timeout, and the
  per-output resolution.
* **Settings (simulated):** rendered and driven in the GTK harness. Clicking
  Extend, then Keep, saves it; letting the countdown run out restores Mirror.
* **flwm and compositor:** checked in Xephyr with RandR root resizes. A
  window stranded by a shrink comes back with the new flwm and stays
  off-screen with the old one. After growth the old compositor shows black
  and the new one paints.
* **Release ISO in QEMU, single display:** the 2026-10-18 ISO with the
  changed files overlaid, on XLibre. The session starts through
  `apply --session`. Every layout command is refused cleanly with one
  display, and `internal` is saved. A Settings maximise fills 1024x690,
  stopping above the dock. The layout survives `restart-session`, with both
  the new `.xsession` and the 2.7 one (X.d hook). The dock, via the shim,
  the launcher button and the Control Center sit where they should.
* **Not verified on a real X server:** two outputs. This host's QEMU has no
  virtio-gpu multi-head module. With a second `secondary-vga` card the
  kernel saw both connectors, but XLibre did not bind the second GPU into
  RandR.
