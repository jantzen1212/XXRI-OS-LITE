# Phase 10.5.2 — Store transparency regression, and what Settings actually does

You were right that the state was wrong, and right not to trust the previous
report. Here is what the diagnosis actually found — including two things I had
believed that were false.

---

## 1. The Store regression: I caused it, in 10.5

`.xxri-header` (the Store's toolbar) has carried
`background-color: alpha(#FFFFFF, 0.72)` all along — that is its glass. In
Phase 10.5 I wrapped the toolbar in an event box so it could act as a drag
handle, and gave that box an **opaque** background:

```css
.xxri-headbox { background-color: @xxri_surface; }   /* 10.5 */
```

An opaque box *behind* a 72%-white toolbar cancels the toolbar's alpha exactly.
That is the "Store transparency disappeared" you saw. I did it deliberately
because the wordmark had been sitting on raw pink wallpaper — but the pink tint
*was* the intended glass, and removing it was the wrong call.

Fixed: the box behind the toolbar now paints nothing when composited, so
`.xxri-header`'s own alpha does the work again.

## 2. The Store rail was never translucent — including in 10.4

The composited rule I added in Phase 10.4 targeted `.xxri-sidebar`:

```css
.xxri-root.composited .xxri-sidebar { background-image: …alpha(…0.85)… }
```

But **the Store's rail is `.xxri-rail-col`**, which that selector never matched.
The rail kept its fully opaque gradient the whole time; what looked translucent
in 10.4 was the *window background* behind it, not the rail. So this was a gap
in 10.4, not a 10.5 regression — the 10.4 report claiming "Store rail
translucent" was wrong, and I have corrected that claim.

Fixed: `.xxri-rail-col` added to the rule. Also removed a `.xxri-sidebar` class
I had put on the rail's new wrapper in 10.5, which stacked a second gradient on
top of the rail's own.

## 3. Settings is NOT floating — measured, not assumed

The client window is `980x660+0+0`, depth 32, border 0 (`xwininfo`), and the
application paints to x=976 of 978. It already fills its window; the sidebar
reaches the top, left and bottom edges.

**The white area around Settings in your screenshot is the Store's own opaque
content pane, behind it.** With the Store's rail and toolbar now translucent
that expanse is smaller, but the Store's *content* pane stays opaque — which
your brief explicitly asks for ("the content area can remain appropriately
opaque"). Two overlapping windows will always show the lower one around the
upper one.

Settings' sidebar translucency was never lost. It tracks the wallpaper from warm
to cool down its height, which only real compositing produces:

```
y=120  sidebar #E7C9F0   wallpaper behind #FDC596   (warm)
y=550  sidebar #D4CFFA   wallpaper behind #3895FC   (cool)
```

## 4. Proof of genuine compositing — over wallpaper AND over an application

The test you asked for. One pixel column through the Store's rail, with the
window unmoved; only what is *behind* it differs:

```
Store rail, x=110
  over the dark Terminal (y<390):  #FCFCFD  #535167  #C8C3D2
  over the wallpaper     (y>390):  #F2D5F9  #DBDBFA  #D3CFFA
```

A painted-on background cannot change when a different window moves behind it.
`debug/phase10.5.2/R2-store-raised.png` shows it: the rail's icons sit on dark
where the Terminal is behind, and on light lavender below it.

## 5. Three of my own measurements were invalid before this one

Worth recording, because it is why the earlier "identical pixels" readings were
misleading: `xwininfo -tree` prints a window's geometry **relative to its
frame** first and its absolute position second. I read the first as absolute,
so three comparisons sampled points that were not inside the surfaces I thought
they were, and produced identical values for the trivial reason that nothing
had changed at those coordinates. The measurements above come from geometry
read off the screen and confirmed visually.

## 6. Regression

| test | result |
|---|---|
| dock: 3 cycles × 5 launchers | **15 correct, 0 wrong** |
| minimise → dock restore, no duplicate | ✅ |
| Settings maximise / restore | ✅ fills to the dock line, returns |
| Settings drag | ✅ |
| Settings minimise / dock restore | ✅ iconic → normal, focused |
| △ □ X | ✅ unchanged, X still the XXRI X |
| no titlebar reappeared | ✅ |
| Store layout, toolbar, search, rail | ✅ unchanged |
| Store content pane | ✅ still opaque |
| compositor architecture | ✅ untouched |

Four compositions captured in `debug/phase10.5.2/`: Settings alone, Store alone,
Settings over Store, Store over Settings.

## 7. What I did not change

No redesign, no colour/gradient/icon/typography changes, no new cards or glass,
no titlebar, no dock or Control Center changes, no compositor or X.Org changes.
The diff is 30 CSS lines and one class name in the Store.

Still open, unchanged from 10.5: **resize** does not work on the undecorated
windows (no frame edge to grab). The two candidate fixes are in the 10.5 report;
neither was applied here because this pass was scoped to the regression.
