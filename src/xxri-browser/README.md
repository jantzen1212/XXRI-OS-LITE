# XXRI Browser

XXRI OS Lite's web browser: a source-level fork of Qt WebEngine's
`simplebrowser` example, rebuilt as an XXRI application.

Upstream supplies the engine integration - `WebView`, `WebPage`, `TabWidget`,
the download manager and the certificate/authentication dialogs - and those
files keep The Qt Company's copyright headers and are modified as little as
possible.  Everything that makes it XXRI is new code in this tree:

| File | What it is |
|---|---|
| `browser/xxriui.{h,cpp}` | The shell: window controls, wordmark, painted line icons, tab counter, dotted rule, resize edges, favicon fetching, and the whole style sheet |
| `browser/xxridata.{h,cpp}` | Persistent shortcuts, bookmarks and history (INI under the user's config directory) |
| `browser/xxripages.{h,cpp}` | The pages the browser draws itself: the XXRI start page and the History page |
| `browser/browserwindow.{h,cpp}` | Reworked: the native menu bar, tool bar and title bar are gone; the window is the mockup's sidebar + slim tool bar + content column |
| `browser/main.cpp` | XXRI identity, and no network fetch before the user asks for one |

## The layout

`assets/mockup/mockup1.jpg` is the source of truth, and the geometry is
measured from it rather than estimated.  In the artwork the browser window is
1325x898 with a 236px sidebar, a 37px tool bar and a 26px address pill, sitting
on a 4:3 desktop 2210px wide.

XXRI's desktop is 1024x768, so the shell is drawn at **kS = 158/236 = 0.669** of
the artwork and the window keeps the artwork's 1.476 aspect ratio.  Every
sidebar offset in `browserwindow.cpp` is the mockup's own y-coordinate scaled by
kS and measured from the window's top edge:

| Element | Mockup y | Shell y |
|---|---|---|
| window controls | 117 | 3 |
| wordmark | 144 | 21 |
| shortcut tiles | 169 | 38 |
| "Bookmark" | 292 | 120 |
| bookmark slots | 327 | 144 |
| dotted rule | 454 | 229 |
| shortcut rows | 461 | 233 |

The tool bar is the one place the artwork cannot be scaled literally: 37px and
26px at kS are 25px and 17px, below what a text field can be read or clicked
at.  They are held at 32px and 24px - the smallest usable sizes - and the ratio
between them is preserved.

## Web compatibility

Chromium 87 is the last branch that builds for i686, and it is a complete
engine - but it predates a few JavaScript standard-library additions from 2021
and 2022 that modern sites call without a guard.  One missing function throws
during start-up and leaves the page unbuilt: GitHub rendered nothing at all
because `crypto.randomUUID` and `Array.prototype.at` were absent.

`browser/xxricompat.cpp` supplies them - `at`, `findLast`, `findLastIndex`,
`Object.hasOwn`, `crypto.randomUUID` (backed by the engine's own
`getRandomValues`), `structuredClone`, error `cause`, `AbortSignal.timeout` -
as a script injected before every page's own scripts, each polyfill guarded so
nothing already present is replaced.  They are library functions, not engine
features, so a script can provide them.

The same file sets the user agent.  Google and others serve a cut-down page to
browser versions they consider old, although this engine renders the modern
page fine; the request header reports a newer Chrome, and the polyfills let
that page run.

## Icons

XXRI bundles no third-party logos.  The XXRI mark is the project's own
`assets/logo.png`; every other site icon in the sidebar is that site's own
favicon, fetched once over HTTPS and cached under the user's data directory.
Until a site's favicon is known, its tile
shows the XXRI generic site mark - a drawn globe, never invented initials.

The order is: the icon the engine decoded for the page, then the on-disk cache,
then one request to the site's `/favicon.ico` for a shortcut that has never
been opened, then the generic mark.  A site whose icon is only declared in its
HTML (XDA, for instance, has no `/favicon.ico`) shows the generic mark until it
is first visited.

The line icons the mockup draws - the chevrons, the house, the plus, the clock,
the stacked tab sheets, the three dots, the down arrow - are painted in
`xxriui.cpp` rather than shipped as files, so their stroke weight tracks the
text colour and the requested size.

## Window behaviour

The window is frameless and draws its own chrome, like XXRI Settings, Store and
File.  Dragging the sidebar or the tool bar background moves it; five invisible
grips on the east, west and south edges and the two southern corners resize it,
at the same 5px/14px thicknesses `xxri-chrome.h` uses for the GTK applications.

Both are driven directly rather than through `startSystemMove()` /
`startSystemResize()`: on XCB those report success as soon as a
`_NET_WM_MOVERESIZE` message is sent, whether or not any window manager acts on
it, and flwm does not - so the "supported" path silently does nothing.

## Keyboard

Ctrl+T, Ctrl+W, Ctrl+Shift+T, Ctrl+D, Ctrl+L, Ctrl+R / F5 and Alt+Left/Right
are bound as `Qt::ApplicationShortcut`, not as window actions.  Qt WebEngine
gives the render process its own input path: when focus is inside the page -
which, with a start page that focuses its own search field, is immediately -
neither Qt's shortcut map nor an application event filter sees the key at all,
and a window-context shortcut simply never fires.  An application-context
shortcut is matched before focus is consulted.  The menu entries keep the same
sequences for display, with `Qt::WidgetShortcut` so they cannot compete.

## Building

Built by PTXdist: `ptxdist/rules/xxri-browser.make` (qmake against the
private Qt) and `xxri-qt5.make` (Qt 5.15.17 + Qt WebEngine, installed under
`/usr/local/lib/xxri-browser` with only the `en-US` locale catalogue).
`image-xxri-browser-tcz` squashes both into `xxri-browser.tcz`;
`tools/qa/elf-closure.py` checks that every library resolves in the image.
