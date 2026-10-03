# 2.6 final: the look, dialogs, installer, pointer, Super key

What changed last before the 2.6 release, why, and how each part
was checked. Build and QA results are in `qa-ptxdist.md` and
`gpu-cpu-support.md`; the harnesses are in `tools/qa/`.

## Palette

The lavender tint is gone from every white, and the dark appearance is
charcoal rather than dark purple. Hierarchy comes from brightness, not from
coloured rules.

| | Light | Dark |
|---|---|---|
| window / content | `#FFFFFF` | `#242124` |
| surface (cards, grouped rows) | `#F5F5F7` | `#2A2B33` |
| raised (secondary buttons, hover) | `#ECECEF` | `#34353E` |
| sunken (troughs, switches off) | `#E2E2E6` | `#3C3D47` |
| sidebar, without a compositor | `#F2F2F4` | `#1D1B1D` |
| hairline | black at 7 % | white at 8 % |
| ink / muted | `#1C1C1E` / `#6B6B73` | `#F2F2F4` / `#A0A1AA` |
| accent (focus, selection, links) | `#2E7CF6` | `#4C93F8` |

Primary buttons are one gradient everywhere, `#1E4FC2 → #4A9AF5`; secondary
buttons are neutral. Switches, sliders and progress use the same blue.

Where it lives: the GTK theme (`themes/xxri/gtk-3.0/gtk.css`, `gtk-dark.css`,
`gtk-4.0`, `gtk-2.0/gtkrc`), each application's own sheet (Settings, Store,
Files, Media, Control Center and power menu, launcher), the browser (Qt style
sheet, new-tab page), the Qt palette files, `.Xdefaults` (FLTK programs, the
terminal is `#242124`), the window manager's title bars and menu, and the dock's
running indicators.

Left as they were, on purpose — they are artwork, not surfaces: the sidebar and
dock icons, the △ □ ✕ window controls, the Files filter chips and location text
(sampled from the supplied mock-ups), the browser's wordmark and fallback site
icon, the Store's two brand banners and its letter tiles.

## Dark mode in every app

Until now only Settings followed Settings > Wallpaper & Style > Mode; every
other XXRI app pinned GTK's `prefer-dark` to FALSE. Now the whole desktop
follows it, and open windows switch while they are open.

* `xxri-desktop appearance set dark|light` writes `~/.config/xxri/appearance`
  (whole, by rename) besides GTK's `settings.ini`, sets the FLTK and Xaw
  colours in `~/.Xdefaults` (the terminal stays the dark panel in both
  modes), and rebuilds the dock.
* `xxri-theme.h` (identical copies in each GTK app, like `xxri-dialog.h`)
  reads that file, watches it with a GFileMonitor, sets `prefer-dark` (the
  xxri theme's `gtk-dark.css` for stock widgets) and puts the app's
  `*-dark.css` above its own sheet. The dark sheets redefine the app's named
  colours and restyle the few surfaces the light sheet writes as plain
  white. `xxri_theme_dark()` is for what the apps paint with cairo: rail
  and tile glyphs, glass panels, progress rings and the like. Light mode is
  unchanged rule for rule.
* Apps: Settings, Store, Files, Media, Control Center and the power menu, the
  All Apps drawer and dock key, the installer, the `xxri-dialog` helper
  (which already read GTK's setting).
* Files' three near-black glyphs (menu, refresh, the "File" of the title)
  have `-dark` twins made by `tools/icons/files-dark.sh`; the dock pill has
  `wbar-bg-dark.png` (`tools/icons/dock-bg-dark.sh`), which `xxri-dock-pin`
  picks by the mode.
* Window frames: flwm patch `0003-XXRI-dark-palette.patch` makes the
  palette runtime values, chosen from the same file. flwm looks at the file
  every two seconds and repaints the frames on a change.
* Browser: the palette is loaded at start and a QFileSystemWatcher on
  `~/.config/xxri` re-applies it: the style sheet gets a dark block appended,
  the painted glass and glyphs follow, and the start/history pages get a dark
  style block.

## Compositor readiness, window fade-in

* `.xsession` used to wait for the compositor with `pidof` plus a fixed
  `sleep 0.5`. Anything override-redirect that painted before the redirect
  stayed blank all session. Now `xxri-compositor` writes `$XXRI_COMP_READY`
  (by rename) once it owns `_NET_WM_CM_S0`, has redirected, and its first
  frame is synced. `.xsession` removes the file before (re)starting it and
  waits for it while the compositor process lives. A compositor that exits
  (no Composite, `nocomposite`) ends the wait at once. On a session restart
  the old compositor's exit is awaited instead of `sleep 0.2`, because it
  holds the selection until then.
* Managed windows (flwm frames with a `WM_STATE` client) fade in over 120 ms,
  ease-out, through the compositor's existing opacity mask. Menus, the dock,
  Control Center and the launcher have no client and are not faded; flwm's
  frames are themselves override-redirect, so that flag could not tell them
  apart. While nothing fades the compositor sleeps in `XNextEvent` as
  before. The `noanim` boot code exports `XXRI_NO_ANIM=1` before the window
  manager starts, which turns off this fade and flwm's minimise flight
  (flwm read the variable, but nothing set it).
* Checked on Xvfb with the real i686 flwm and compositor: ready marker 13 ms
  after start; fade opacity 0 → 0.23 → 0.53 → 0.74 → 0.87 → 0.95 → 0.99 → 1;
  none with `XXRI_NO_ANIM`. The same run found that the compositor's debug
  mode (`XXRI_COMP_DEBUG`) asked for ButtonPress on the root, which only one
  client may hold. Under a window manager that voided the compositor's
  whole event mask, so the button bits are now a separate request.

## Dialogs and menus

`xxri-dialog.h` (the same file in Settings, Store, Media, Files and the
installer) turns every `GtkDialog` of a process into a rounded card with a
hairline and a soft shadow, and gives menus real corners:

* `xxri_dialogs_init()` replaces `GtkDialogClass->realize`, so it also reaches
  dialogs the application never sees — libfm's rename, delete and properties
  dialogs in Files, the file chooser.
* With the compositor the dialog is an ARGB window: the card's corners and
  shadow are real alpha over whatever is behind it. Without one the card is cut
  out with the X Shape extension instead (no shadow).
* One primary action per dialog (what the application marked, otherwise
  OK/Yes/Accept), every other button neutral. A dialog with a window title
  shows it as a heading, since no XXRI window has a title bar.
* `xxri-dialog`, the helper shell tools use, is now a GTK program built on the
  same header (it was FLTK, a square window with painted corners). The power
  menu is the same card.
* "Log out" in the power menu ends the session (`xxri-desktop
  restart-session`); it used to kill only the window manager.

## Translucency, measured

`tools/qa/glass-measure.py DESKTOP.png SHOT.png name=x,y,w,h …` regresses a
region of a screenshot against the same region of the bare desktop. An opaque
surface has slope 0 whatever is behind it; a translucent one follows the
backdrop with slope 1 − alpha.

| Surface | Designed alpha | Measured in the session |
|---|---|---|
| Settings sidebar | 0.74 | 0.743 |
| Store rail | 0.74 | 0.741 |
| Files rail | 0.72 → 0.55 | 0.62 (mean) |
| Browser sidebar | 0.84 | 0.839 |
| Media rail | 0.83 | 0.830 |
| Control Center panel | 0.62 → 0.72, over its shadow | 0.77 |
| All Apps drawer | 0.93 → 0.90, edge strip | 0.85 |
| Installer cards | 0.80 | 0.802 |
| Dialog margin | shadow only | backdrop unchanged but for the shadow |

The dock is the one surface that is not an ARGB window: wbar blends its glass
against the root pixmap. That is exact here, because ordinary windows stack
above the dock — nothing but the wallpaper is ever behind it.

## Installer

`xxri-installer` is a GTK 3 program now (it was FLTK with hand-drawn, aliased
widgets and its own pink-to-purple palette). The backend, its protocol and the
configuration file it is handed are unchanged.

* The official wallpaper is the desktop's own: the installer's window is
  transparent and an opaque backdrop window under it shows the root wallpaper,
  so the glass cards are blended by the compositor. Without a compositor one
  window paints the wallpaper and the cards blend against that.
* Account page: each field says what it is for, and says what to change, under
  the field it is about, the moment something is off. Capitals in the username
  become lowercase; the computer name follows the username until it is edited;
  the password line says whether the two match. Continue is disabled until the
  page is valid.
* Destination page: "Erase disk" / "Choose partition", the disks as selectable
  rows, a plain warning of what will be erased, Back and Install.
* Progress: a ring that eases to each new value, the percentage in it, the
  current operation, and the backend's phases as a check list. The 40 ms timer
  runs only while an installation is on screen.
* Back buttons on the account and destination pages; Enter does what the
  page's primary button does.
* The backend's extension phase reported 86 → 90 and the next phase 88, so the
  old bar went backwards; it now runs 86 → 88.

`XXRI_INSTALL_BACKEND` points the installer at another backend; it exists for
QA (a scripted fake backend drives every page without a disk).

## Pointer at the screen edge

On the framebuffer server (Xfbdev) the pointer comes from `/dev/input/mice`,
the kernel's mousedev. mousedev turns an absolute device — a virtual machine's
tablet, a touchscreen — into relative motion by scaling it to a screen of its
own, 1024×768 unless told otherwise. VirtualBox's VMSVGA framebuffer is
1280×800, so the pointer ran out of travel 256 px before the right edge.

`start_xfbdev` in `~/.xsession` now writes the framebuffer's size to
`/sys/module/mousedev/parameters/{xres,yres}` before it starts the server.
There is nothing VM-specific in it. Xorg (KMS or VESA) reads evdev through
libinput and was always exact.

VirtualBox 7.2.14, VMSVGA, USB tablet, BIOS, the guest forced onto Xfbdev at
1280×800, host pointer moved over the 1:1 view:

| Host pointer | Guest, mousedev = framebuffer | Guest, kernel default |
|---|---|---|
| 1279, 799 | 1279, 799 | 1023, 799 |
| 1279, 0 | 1279, 0 | 1023, 32 |
| 0, 799 | 0, 799 | 0, 799 |
| 640, 799 | 640, 799 | 512, 799 |
| 300, 200 | 299, 200 | 240, 224 |

The same VM on its normal path (Xorg on the vmwgfx KMS device) was exact at
every corner. In QEMU with a USB tablet: exact at 800×600 and 1280×1024 with
the fix; with the default a left-to-right sweep at 1280×1024 ends at x = 1024.
On Xfbdev an absolute device is still delivered as relative motion, so the
guest pointer lines up with the host's only after it has touched an edge once.

## Super key

Pressing Super on its own opens All Apps; pressing it again, or Esc, closes it.

* The resident `xxri-launcher --button` process grabs the key on the root
  window and toggles the drawer on the release, if nothing else was pressed in
  between and the key was not held (0.7 s). Super+something and Ctrl/Shift/Alt
  +Super do nothing.
* Which keycode is Super depends on the keyboard: the core map is only the map
  of the keyboard used last (133 before the first key press under Xfbdev, 115
  after). Every keyboard device's own map is read, each keycode that is Super
  is grabbed, and `MappingNotify` repeats it.
* While the installer has the screen there is no dock, and no drawer.
* The drawer gives the keyboard back to the window that had it.
* `flwm` patch `0002`: when the last window closed, the X focus ended at
  `None` — all keyboard input discarded, so no hotkey at all (the window
  manager's own included) until a window was opened with the mouse. The focus
  now falls back to `PointerRoot`.

## Defects found on the way

* Dock icons blank on a first boot: the icon framer ran a moment before
  gdk-pixbuf's `loaders.cache` existed and cached eight empty containers. PNG
  icons are read through cairo now, and a failed frame is never cached.
* `xxri-dock-pin` redirected the framer's stderr to a log it might not be able
  to append to; a failed redirection makes the shell skip the command.
* The dock's running indicators stayed on screen when the dock was stopped.
* The live session had no window manager (`/etc/sysconfig/desktop` missing)
  and the root window kept X's default ✕ cursor; both fixed before the
  release, the cursor is the theme's arrow everywhere.
