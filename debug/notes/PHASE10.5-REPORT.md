# Phase 10.5 — Window chrome, integrated toolbars

Baseline: Phase 10.4. The compositor, real alpha, △ □ X, dock behaviour,
Control Center and window management all still work; each was re-tested after
every change.

---

## 1. The complaint was right, and the mockup says why

The report was "a large white horizontal strip above the actual Settings
content… it makes the application feel like a generic GTK application placed
inside an XXRI window". That is exactly what we shipped, and reading the
mockup at full resolution shows the design has **no titlebar at all**:

`debug/phase10.5/MOCKUP-settings-chrome.png` — the △ □ ● controls sit **inside
the sidebar**, above the word "Settings", and the sidebar runs to the top edge
of the window. There is no full-width bar, so there is nothing to be white.

Our version put flwm's 26px titlebar across the whole window, above both panes
(`BEFORE-white-header-strip.png`). Making that strip transparent would not have
helped — the strip itself is the thing the design does not have.

**Two compositions, not one.** The mockups deliberately differ:

| | Settings | Store |
|---|---|---|
| navigation | **wide labelled sidebar** (250px) | **narrow icon rail** (~62px) |
| toolbar | none — the sidebar *is* the chrome | **horizontal header**: wordmark, search, actions |
| controls | top of the sidebar | top of the rail |

These were kept distinct rather than unified into one "XXRI header".

## 2. What changed

XXRI's own applications now draw their own window controls and are
undecorated; flwm honours `_MOTIF_WM_HINTS`, which is what
`gtk_window_set_decorated(FALSE)` sets, so no window-manager change was needed
to stop it drawing a bar.

* `xxri-chrome.h` (shared by Settings and Store) draws the **same** △ □ X
  glyphs flwm draws for every other window — magenta triangle, purple rounded
  square, blue circle with an X knocked out of it — with the same hover and
  pressed states. The close control is still the XXRI X, not a circle.
* Because the application owns the window now, it also owns what the titlebar
  used to do:
  * **minimise** → `gtk_window_iconify()`, which is ICCCM `WM_CHANGE_STATE`, which
    flwm already handles
  * **maximise/restore** → done by the window itself, stopping above the dock
    exactly where flwm's maximise stopped (flwm is ICCCM-only, so there is no
    `_NET_WM_STATE` to ask for)
  * **close** → `gtk_window_close()`
  * **drag** → the header row is the handle; double-click maximises
* **Non-XXRI applications are untouched.** The Terminal and Editor still get
  flwm's titlebar with △ □ X (`debug/phase10.5/term-deco.png`). Only XXRI's own
  applications, which have somewhere sensible to put the controls, go
  undecorated.

The sidebar's translucency now reaches the top of the window instead of being
cut off by an opaque strip, which is the visible payoff:
`AFTER-integrated-chrome.png`.

The Store's header is deliberately **opaque** — it carries the wordmark, the
status pill and the search field, and the mockup draws it as a clean light
toolbar. Only the rail beside it is translucent. (The first attempt left it
transparent and the wordmark ended up sitting on pink wallpaper.)

## 3. A regression I caused and caught

Making these windows undecorated made them `NO_BORDER`, and the dock-flight
animation from Phase 10.3 explicitly skipped `NO_BORDER` windows. That silently
removed the launch/minimise/restore flight from **exactly the two applications
it matters most for**. The filter is now "does this window match a dock slot?",
which is the real question — a window with no dock icon has nowhere to fly
from.

## 4. Verification

Every window operation moved from the window manager into the applications, so
all of it had to be proven again, with real pointer input:

| | Settings | Store |
|---|---|---|
| drag by header | ✅ window moved | ✅ |
| minimise (△) | ✅ `WM_STATE` → iconic | ✅ iconic |
| restore from dock | ✅ normal, focused, no second instance | ✅ |
| maximise (□) | ✅ grows to the dock line | ✅ |
| restore (□ again) | ✅ back to previous geometry | ✅ |
| close (X) | ✅ window gone | ✅ |

Two of my own test runs reported false failures before this table was right:
the controls **move with the window**, and clicking where they used to be
proves nothing. Both "failures" disappeared once the test re-located the
controls (or started from a known position).

| regression | result |
|---|---|
| dock: 3 cycles × 5 launchers, focus-vs-launch semantics | **15 correct, 0 wrong** |
| minimise → dock restore, no duplicate instance | ✅ |
| flwm host stress (menus/min/max/drag/close ×3) | 38 checks, **0 deaths** |
| Control Center: corner-open / dwell / auto-retract | ✅ all three |
| non-XXRI apps keep flwm decorations | ✅ |
| resolutions 1024x768 / 1280x768 / 1280x800 / 1360x768 | ✅ controls present at all four (checked by pixel) |
| real transparency (compositor, ARGB) | ✅ unchanged |

## 5. Not done, and why

* **The dock flight still clips rather than scales.** It was re-enabled for the
  undecorated windows and verified functionally (minimise → iconic → restore),
  but I did not manage to capture a mid-flight frame of a CSD window: the
  screendump takes seconds and the flight is 170ms, and the slowed-build trick
  that worked in Phase 10.3 kept catching the wrong window. The flight itself
  was captured mid-air in Phase 10.3 (`debug/phase10.3/livability/`); what is
  unproven here is only the picture, not the behaviour.
  A compositor-assisted *scale* (`XRenderSetPictureTransform` on the redirected
  surface, as the brief suggests) remains the right next step and is still not
  done — it needs flwm and the compositor to agree on a source rectangle, and I
  would rather build that deliberately than bolt it on at the end of this pass.
* **The Settings sidebar has no search field.** The mockup shows a magnifier
  beside "Settings"; the implementation has no search, and adding one is a
  feature rather than chrome work.
* Window frames remain 24-bit, so flwm's own titlebar (still used by every
  non-XXRI application) cannot be translucent.

## 6. Files

```
rootfs/usr/local/src/xxri-settings-gtk/xxri-chrome.h     NEW (shared)
rootfs/usr/local/src/xxri-store-gui/xxri-chrome.h        NEW (copy)
rootfs/usr/local/src/xxri-settings-gtk/xxri-settings.c
rootfs/usr/local/src/xxri-store-gui/xxri-store-gui.c
rootfs/usr/local/share/xxri-settings/xxri.css
rootfs/usr/local/share/xxri-store/xxri-store.css
work/flwm-xxri/Frame.C                    (flight no longer skips NO_BORDER)
```

Artifacts rebuilt. 20 screenshots in `debug/phase10.5/`. Phase 11 not started.


---

## Addendum — the "floating inside a white canvas" fix

**Report:** the Settings UI still looked like it was floating inside a larger
white window.

**Cause, one line.** The page content was wrapped in a box with
`GTK_ALIGN_CENTER`:

```c
GtkWidget* center = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);
gtk_widget_set_halign(center, GTK_ALIGN_CENTER);   /* <- this */
```

A centred box takes only its child's natural width, so the 600px content column
sat in the middle of a ~730px pane, leaving a band of bare surface colour down
both sides. Verified against the mockup, whose content pane is filled to within
about 17px of its edges. Changed to `GTK_ALIGN_FILL`; the column's existing
32px margins still provide the page's breathing room and the 600px size request
became a minimum.

Nothing else changed: same sidebar, same transparency, same icons, same
typography, same internal spacing, same △ □ X in the same place, same
undecorated behaviour.

The window geometry was **not** the problem, and was checked before changing
anything: `xwininfo` reports the client as `980x660+0+0`, depth 32, border 0,
with the application painting to x=976 of 978 — it already filled its window.
(An earlier screenshot that appeared to show a white surround turned out to be
a contaminated capture with the Store open behind Settings.)

Before / after: `debug/phase10.5/FIX-side-by-side-1024x768.png`.

### Verified after the fix

| at 1024x768 | result |
|---|---|
| open | ✅ |
| drag by header | ✅ window moved |
| maximise | ✅ fills to the dock line |
| restore | ✅ |
| minimise | ✅ iconic |
| restore from dock | ✅ normal, focused |
| close | ✅ |
| reopen from dock | ✅ |
| **resize** | ❌ see below |

At 1280x800 the content fills the pane identically
(`debug/phase10.5/FIX-AFTER-1280x800.png`).

### One thing this pass did not fix: resize

Dragging the window's bottom-right corner does nothing. This is **not** caused
by the change above — it is a consequence of Phase 10.5 making these windows
undecorated: with no window-manager frame there is no border to grab, and
GTK adds no resize handles when `decorated` is false.

It was left alone deliberately, because every fix available changes the window
boundary this pass was asked to make clean:

* Motif decorations of `BORDER|RESIZEH` (no `TITLE`) would put flwm into
  `THIN_BORDER`, which gives `top=0` — no titlebar — but a 3px side and 4px
  bottom border. That restores resize, at the cost of the sidebar no longer
  reaching the window's left edge.
* An invisible resize grip in the application (a transparent event box calling
  `gtk_window_begin_resize_drag`) keeps the look exactly as it is, but means
  wrapping the Settings layout in a `GtkOverlay`.

Both are small; neither is "this exact issue", so they are flagged rather than
chosen.
