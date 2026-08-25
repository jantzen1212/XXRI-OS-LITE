# Phase 10.4 — Real transparency, and where the earlier answer was wrong

Continues from the Phase 10.3 baseline. Everything listed as working there —
△ □ X decorations, drag, minimize/restore, dock launch/restore/minimize-to-icon,
Control Center corner trigger and auto-retract, focus state — still works; each
was re-tested after every change below.

---

## 1. I was wrong about transparency, and this is what changed my mind

Phase 10.3 concluded that real alpha was unavailable and used wallpaper
sampling instead. **Two of the three legs of that conclusion were wrong**, and I
want to be precise about which:

| Phase 10.3 claim | Reality |
|---|---|
| "0 ARGB visuals" | **False.** That number came from counting `depth: 32` lines in `xdpyinfo` output — the wrong thing to count. A proper probe (`XGetVisualInfo` + `XRenderFindVisualFormat`, checking `direct.alphaMask`) reports **10 ARGB visuals**. |
| "no compositing manager, so ARGB paints black" | True, and still true — but it was a missing *program*, not a missing *capability*. `XCompositeGetOverlayWindow` is **granted** on this target. |
| "a compositor is too expensive for Pentium-III-class hardware" | **Too pessimistic.** That reasoning assumed continuous full-screen compositing. A damage-driven compositor does nothing while nothing changes. Measured below: **~0.1% idle CPU, ~2 MB**. |

The probe that settled it (`work/` → built from `/tmp/.../xprobe.c`):

```
root depth        : 24
visuals total     : 100
depth-32 visuals  : 20
ARGB (alpha mask) : 10
Composite         : yes 0.4      Damage : yes 1.1
XFixes            : yes 6.0      Render : yes 0.11
overlay window    : granted
```

## 2. `xxri-compositor` — small on purpose

No compositor exists in the Tiny Core 16.x repository (2694 packages
enumerated; the four client libraries are there, no manager). Rather than port
`picom`, I wrote one that does only what XXRI needs:

* manual redirection + an overlay window, XRender compositing
* **damage-driven** — repaints only the changed region, so an idle desktop costs nothing
* honours each window's **bounding shape**, so flwm's rounded frames survive
* honours `_NET_WM_WINDOW_OPACITY`, looking *inside* the frame for it (an
  application sets it on its own window, which lives one level in)
* claims `_NET_WM_CM_S0`, which is how GTK's `gdk_screen_is_composited()`
  answers — without it applications never learn they may use an alpha channel
* ~600 lines, no shadows/blur/fading/vsync: those belong to the surfaces
  themselves, not to a general-purpose engine

**Safety.** Redirection belongs to this client, so if the compositor dies the X
server unredirects and the desktop goes back to painting itself — a crash costs
translucency, not the session. `nocomposite` on the kernel command line skips
it (verified). Every surface keeps its Phase 10.3 fallback path.

### Measured on the target

| | idle CPU | RAM used | compositor RSS |
|---|---|---|---|
| without compositor | 1.2% | 60,008 KB | — |
| with compositor | 1.3% | 59,508 KB | 1,964 KB |
| ARGB window repainting 10×/s | 3.0% | | |

## 3. Three bugs found on the way to a working blend

Getting from "compositor runs" to "alpha actually blends" took four wrong
answers, each found by measuring rather than reasoning:

1. **New windows were invisible.** `add_win` inserted at the *bottom* of the
   stack. That is right for the initial `XQueryTree` walk (bottom-to-top) but
   wrong for `CreateNotify`, since X creates windows on **top** — so every new
   window was buried under everything else.
2. **My test client was lying.** It looped 200 × 100 ms = 20 s and had already
   exited before every screendump. Nothing was wrong with the compositor at all
   for two of the debugging rounds.
3. **The frame flattened the client's alpha.** flwm reparents each client into
   a frame it creates with the default **24-bit** visual, and a 24-bit pixmap
   has nowhere to keep an alpha channel — a translucent application came out
   mixed with flat grey. Fixed by redirecting the ARGB *client* as well and
   compositing it separately from its frame. Only depth-32 clients are
   redirected, so nothing that never asked for alpha is affected.
4. **The frame was then painted underneath it.** With the client composited
   separately, the opaque frame still sat behind it and became the backdrop the
   client blended against. The client's rectangle is now punched out of the
   frame's clip, so it blends against the desktop.

Proof that alpha genuinely blends — a test window drawn opaque on the left,
50% on the right, over the wallpaper (`debug/phase10.4/argb-blend-proof.png`):

```
opaque half : #FF20FF   (exactly the colour asked for)
alpha half  : #F588FE   (that colour blended with the wallpaper behind)
```

## 4. What is translucent now, and what is not

Real alpha, deliberately limited to the surfaces the mockups draw that way:

* **Control Center** — the sampled-wallpaper glass is gone. Because the server
  does the blending, this also retired every workaround that version needed:
  no wallpaper snapshot, no "is a window behind me?" test, no one-bit shape
  mask. The corners are now **antialiased** and the shadow is a genuine soft
  gradient. It correctly shows a window *and* wallpaper through it at once
  (`debug/phase10.4/cc-over-win.png`) — the case that produced a bright halo
  twice in Phase 10.3.
* **Settings sidebar** and **Store rail** — translucent layers of their window;
  > **CORRECTION (10.5.2): the Store rail was NOT translucent.** The rule added
  > here matched `.xxri-sidebar`, but the Store's rail is `.xxri-rail-col`, so it
  > kept its opaque gradient; only the window background behind it was
  > transparent. Fixed in 10.5.2.
  the content pane stays fully opaque so text never competes with the wallpaper.
  Verified by measurement, not eyeballing: the sidebar's colour now tracks the
  wallpaper behind it (`#E6BFED` where the wallpaper is warm, `#C7BCFB` where it
  is blue), which it did not before.

Everything else stays opaque on purpose. "Make every window transparent" would
be worse, not better.

## 5. Regression

All on the built image, real pointer input, compositor running.

| test | result |
|---|---|
| dock: 3 cycles × 5 launchers, focus-vs-launch semantics | **15 correct, 0 wrong** |
| minimize → dock click restores, no second instance | ✅ |
| window management suite (drag/min/max/restore/close, 2 windows, menus) | ✅ |
| Control Center: corner-open / dwell / auto-retract | ✅ all three |
| resolutions 1024x768, 1280x768, 1280x800, 1360x768 | ✅ all four, Control Center present at each |
| live ISO | ✅ boots to the installer |
| `nocomposite` boot code | ✅ compositor off, fallbacks render correctly |

A 1360x768 scare — the Control Center appeared to vanish — turned out to be a
stale build in the sweep, not a defect; the current build renders it at every
resolution, confirmed by pixel sampling at all four.

## 6. Still not done

* **The launch/minimize/restore flight clips rather than scales.** The window
  travels between its dock icon and its final rectangle, which reads as origin,
  but the content inside is clipped during the flight rather than scaled. Now
  that a compositor exists this could be a true scale via
  `XRenderSetPictureTransform`; it would couple flwm and the compositor, so it
  is deliberately left for a later pass rather than rushed in at the end of this
  one.
* **Window frames are still 24-bit**, so flwm's own titlebar cannot be
  translucent and its rounded corners remain one-bit (aliased) while the
  surfaces inside them are antialiased. Making the frame ARGB means making FLTK
  draw on a 32-bit visual, which is a change to the most stability-critical
  component in the system.
* The compositor has no vsync, so a fast drag can tear. Adding sync would cost
  more than the tearing does on this hardware.

## 7. Files

```
rootfs/usr/local/src/xxri-compositor/{xxri-compositor.c,build.sh}   NEW
rootfs/usr/local/bin/xxri-compositor                                NEW
rootfs/etc/skel/.xsession                        (start it; `nocomposite` opt-out)
rootfs/usr/local/src/xxri-control-center/xxri-control-center.c   (real-alpha path)
rootfs/usr/local/src/xxri-settings-gtk/{xxri-settings.c,build.sh}
rootfs/usr/local/src/xxri-store-gui/xxri-store-gui.c
rootfs/usr/local/share/xxri-settings/xxri.css
rootfs/usr/local/share/xxri-store/xxri-store.css
```

`build.sh` for Settings was also repaired: it pointed at sysroot GTK headers
that do not exist (the sysroot carries runtime libraries only), so it could not
build at all. It now uses the host-headers/target-libs recipe the Store and
Control Center already used.

Artifacts rebuilt. Screenshots in `debug/phase10.4/`. Phase 11 not started.
