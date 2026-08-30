# XXRI File

The XXRI OS Lite file manager.  It is a **fork of PCManFM 1.3.2**, modified at
source level to be an XXRI application rather than PCManFM with a theme.

    upstream PCManFM 1.3.2  ->  src/xxri-file/pcmanfm  ->  /usr/local/bin/xxri-file

Upstream is GPL-2.0+ by Hon Jen Yee (PCMan); that notice and the About-dialog
credit stay.  `pcmanfm/` keeps upstream's tree layout so the fork can be
diffed and rebased against a future release.

## Why a fork and not the packaged pcmanfm

Tiny Core's `pcmanfm.tcz` is built against **GTK2** (`libfm-gtk.so.4` ->
`libgtk-x11-2.0.so.0`).  XXRI's design system is GTK3 CSS - `xxri.css`, the
shared `△ □ X` client-side chrome, and the compositor's real per-window alpha.
None of that can apply to a GTK2 binary, so XXRI File is built from source
with `--with-gtk=3`.

## Building

    ./build.sh              # build and stage into ../../rootfs
    ./build.sh --clean      # discard the buildroot first

The build is **native i686, not cross-compiled**.  The x86_64 host kernel runs
i686 binaries directly, so `build.sh` assembles a Tiny Core i686 root from the
image's own `work/iso/boot/core.gz`, adds Tiny Core's `compiletc` and
`gtk3-dev`, and builds inside it under bubblewrap.  The result links against
exactly the libraries the device ships (GTK 3.24.39, glibc with the correct
32-bit `glibconfig.h`).

Two traps this script already handles:

* GCC 14 turned incompatible-pointer-types into a hard error; this 2021 source
  needs the permissive `CFLAGS` the script sets.
* Never `make distclean` the libfm tarball: it deletes the pre-generated
  marshallers, and regenerating them needs `glib-genmarshal`, a python script -
  and Tiny Core's `python3.9.tcz` ships libraries with **no interpreter**.

## What the fork changes

The XXRI shell lives in `pcmanfm/src/xxri-ui.{c,h}` so upstream's `main-win.c`
stays recognisable; `main-win.c` calls into it.

| Area | Change |
|---|---|
| Window layout | `rail \| content(header, list)` replaces `menubar/toolbar/notebook/statusbar`.  The menubar and toolbar are still built (GtkUIManager keeps its accelerators and every `/menubar/...` action lookup) but never packed. |
| Places | A translucent icon rail replaces libfm's `FmSidePane`: Home, Desktop, Documents, Downloads, Pictures, Music, Videos, Computer, with dotted group rules and a selected chip.  An entry whose directory does not exist is hidden rather than silently aliased to `$HOME`. |
| Window chrome | Undecorated + the shared `xxri-chrome.h` controls at the top of the rail, identical to Settings and Store, plus invisible resize edges. |
| Header | "XXRI File" wordmark (pango+cairo gradient - GTK CSS cannot clip a gradient to glyphs), a location pill carrying the folder name and its total size, and filter chips. |
| File view | List rows, no column headers, transparent over the XXRI surface. Columns are `name;size;mtime`. |
| Hidden locations | `/proc`, `/sys`, `/dev`, `/run` and friends are filtered on the **folder model**, so the rule covers the view, navigation and Computer alike. |
| Translucency | ARGB visual + `.composited` class, re-evaluated on `composited-changed`; falls back to opaque when no compositor is running. |
| Identity | Binary `xxri-file`, `g_set_application_name("XXRI File")`, config in `~/.config/xxri-file/`, own icon and desktop entry, dock launcher keyed `xxri-file`. |
| Defaults | Opens `$HOME` (the dock launches with cwd `/`, which is both useless and hidden), 940x620. |

## Artwork

Every glyph in the XXRI File window comes from the supplied asset set in
`assets/icons/xxri-icons/xxri-files/`.  Nothing is looked up in an icon theme:
`src/xxri-file/stage-icons.sh` rasterises the SVGs into
`/usr/local/share/xxri-file/icons/` at build time (the device has no librsvg
or SVG gdk-pixbuf loader), and `xxri_asset_image()` loads them by filename.
A missing asset logs a warning and leaves the image blank on purpose - it is
never quietly replaced with a themed stand-in.

| Mockup element | Asset |
|---|---|
| rail: menu | `menu.svg` |
| rail: refresh | `refresh.svg` |
| rail: Downloads | `slice30.svg` |
| rail: Pictures | `Photos.svg` |
| rail: Music | `music.svg` |
| rail: Videos | `Videos.svg` |
| rail: Documents | `Documents.svg` |
| rail: Internal Storage (opens `$HOME`) | `Internal-storage.svg` |
| rail: Storage (opens `/`) | `Storage-usage.svg` |
| Applications folder | `../all-apps.svg` |
| every other folder | `folders.svg` |
| "XXRI Files" wordmark | `slice31.svg` |
| "All" chip | `slice28.svg` |
| rail selected chip | `slice29.svg` fill (`#D0D0D0` @ 63%) |
| location pill | `slice32.svg` fill (`#6E6969` @ 25%) |
| location text gradient | `slice33.svg` (`#DB00FF` -> `#1D00FF`) |

The same folder artwork is used in the file listing itself, via a cell data
function on libfm's icon column, so Downloads/Pictures/Music/Videos/Documents
carry their XXRI icon in the list and not an Adwaita folder.

No folder in the listing falls back to a theme icon: anything without its own
XXRI artwork uses `folders.svg`.

**Assets that do not exist in the supplied set:** there is no Home glyph, and
the mockup's rail has none - the rail's Internal Storage entry opens `$HOME`,
which is also where the window opens.  `~/Desktop` is not shown at all: XXRI
has no desktop-icon layer, so the folder is neither created nor listed.

## Geometry

The mockup was scanned, not estimated: window 944x724 with a **102px rail**
(10.8%), content surface `#F9F9F9`, pill 54px tall with 27/25px margins.

Those *proportions* are what is reproduced, not the pixel sizes - the mockup
is drawn at a larger scale than a 1024x768 desktop wants.  XXRI File opens at
**720x552**, a normal desktop window that leaves obvious wallpaper around it,
with the rail at 78px (the same 10.8%) and the rest scaled to match.

Row proportions follow the same reasoning.  In the mockup a 64px thumbnail
sits beside a **three-line** text block, so the glyph tracks the height of the
text next to it.  These rows carry one line, so the glyph is **24px** - a 64px
icon simply dwarfs the filename and pads the list with dead space.

## Window behaviour

The window is undecorated, so moving and resizing are the application's job:

* The wordmark strip and the rail are drag handles.  `xxri_chrome_drag_area()`
  refuses any widget without a `GdkWindow`, and both are plain `GtkBox`es, so
  nothing was wired up and the window could not be moved at all.  A
  `GtkEventBox` with `visible_window = FALSE` paints nothing but owns an
  input-only window, so it can take the press and call
  `gtk_window_begin_move_drag()` on the real top-level window.
* The rail's glyph column used to impose a ~500px minimum height on the whole
  window, so it could not be shrunk vertically.  The column now lives in a
  scroller (no visible scrollbar, natural width propagated) which removes the
  floor without changing the layout at normal sizes.

## List behaviour

* **Time grouping** is computed from each item's mtime (`Today`, `Yesterday`,
  `This week`, `Earlier`).  The heading is **not** part of any item: a group's
  first row is allocated an extra 24px band and its cells are bottom-aligned,
  so the item keeps exactly the geometry every other item has, and the heading
  and its dotted leader are painted into the empty band by a `draw` handler.
  (Real heading *rows* are not possible here - libfm resolves selection and
  activation against its own `FmFolderModel`, so a proxy model that inserted
  rows would desynchronise every path and open the wrong file.)  Sorted by date
  **descending with `FM_SORT_NO_FOLDER_FIRST`** - libfm's default hoists every
  directory above the files, which splits a date group in two and makes the
  same heading appear twice.
* **Filter chips** are real: the active chip drives `xxri_filter_mode`, which
  the folder-model filter honours, and clicking one calls
  `fm_folder_model_apply_filters()` so the visible list actually changes.
  "All" wears the supplied `slice28` artwork whenever it is active (the state
  the mockup depicts); the inactive chip is outlined, and a text chip that
  becomes active wears the same gradient reproduced in CSS from that asset.
* **Folder rows carry no date.**  A directory's mtime is noise to a user, so a
  cell data function blanks it for directories; files keep their size and date.
  This is presentation-only - the model still holds the metadata.
* **Storage Usage** in the rail is not a location: it launches Settings' own
  storage page (`xxri-settings storage`, through `xxri-dock-launch` so an
  already-open Settings is focused). There is no second implementation.

## Known deltas from the mockup

The mockup's file rows show a large thumbnail with name / source / date, and
group them under time headings ("This week") with a dotted leader.  XXRI File
currently renders name / size / date rows without time grouping: that grouping
needs a custom tree model in libfm's view and is not yet implemented.
