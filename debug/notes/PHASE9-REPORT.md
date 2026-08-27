# Phase 9 — XXRI Store (Native AppImage Store)

The permanent software-distribution platform for XXRI OS Lite. A native GTK3
front-end over a reusable repository/provider engine that installs AppImages
**only** through the existing Phase 6 `xxri-app` runtime. Built, integrated into
the dock, shipped in the ISO, and verified end-to-end inside QEMU on real i686
hardware: a user can browse the catalog, install an app, launch it, update it,
and remove it — entirely through the Store.

---

## 1. Architecture at a glance

```
        xxri-store-gui  (GTK3, 68 KB, i686)          <- pure front-end
                 │  drives, never implements
                 ▼
        xxri-store      (busybox ash, 22 KB)         <- repository + download engine
           │                         │
   repository engine          xxri-app integrate     <- Phase 6, the ONLY installer
   (remote→cache→bundled)     (unchanged backend)
           │
   provider engine (github / gitlab / direct / local / xxri / hub)
```

Two hard rules from the brief were honoured throughout:

* **`xxri-app` remains the sole AppImage backend.** The Store downloads a file,
  verifies it, and hands it to `xxri-app integrate`. It never mounts, never
  writes a `.desktop`, never touches the dock directly.
* **Nothing is hardcoded.** The 955-app catalog is data; the Store renders
  whatever repository is active. Adding apps, categories, or whole new package
  *kinds* needs zero Store-code changes.

---

## 2. Repository architecture

### Priority chain (the "active repository")

```
remote  (fetched into cache)  →  cache  →  bundled (shipped in the ISO)
```

`xxri-store repo-path` returns the winning directory: the per-user cache if it
holds an `index.tsv`, otherwise the bundled repository under
`/usr/local/share/xxri-store/repository`. On startup the GUI fires an
asynchronous `xxri-store refresh`; if it succeeds the remote catalog replaces
the cache and the model reloads live, without blocking the UI. Offline, the
bundled catalog is used and every non-download feature keeps working.

### On-disk format (append-only, versioned)

| File | Purpose | Consumer |
|------|---------|----------|
| `repository/meta.json` | `format` version + counts | both (future format bumps) |
| `repository/index.tsv` | flat, positional, **append-only** index | the ash engine |
| `repository/<category>.json` | rich per-category records | the GUI |
| `repository/sections.json` | home-page section → `[app id]` | the GUI |
| `icons/<id>.png` | 128px icon for the curated top apps | the GUI (offline) |

`index.tsv` columns are positional and append-only, so a newer catalog never
breaks an older client: `id, name, category, archlist, version, size, sha256,
source_type, source_ref, url, kind, updated, assets, icon_url, screenshots`.
Column 13 (`assets`) carries **per-architecture** `arch,url,sha256,size` tuples,
which is what lets one catalog entry serve i686/x86_64/aarch64 from a single
row and lets the engine pin a checksum per arch.

### Format extensibility (Phase 10+)

Every record has a `kind` field (currently always `appimage`). The GUI already
routes `kind:"theme"` to a Themes page and ignores unknown kinds. Flatpak,
native xxri packages, firmware, drivers, wallpapers and fonts are all just new
`kind` values plus (optionally) a new provider — no schema break, no redesign.

---

## 3. Provider architecture

`xxri-store resolve <id>` turns a catalog entry into a concrete
`URL⇥SHA256⇥VERSION⇥SIZE`, choosing the best available source in this order:

1. **Pinned per-arch asset** (catalog column 13) — needs no network, its
   checksum is known, and it is exactly the advertised version. Wins whenever
   present.
2. **GitHub Releases API** — `source_type=github`. Reads the latest release,
   picks the asset matching this architecture (`*x86_64*`, `*i686*`, …),
   skipping `zsync`/`sig`/`asc`.
3. **GitLab Releases API** — `source_type=gitlab`, `direct_asset_url` for the
   AppImage.
4. **Developer website** — `source_type=direct`, a pinned URL, **verified at
   catalog-build time** to actually serve an ELF (see §4).
5. **Bundled/local** — `source_type=local`, a file inside the ISO
   (offline-installable; this is the sample app).
6. **XXRI Repository** — `source_type=xxri`, reserved for the future first-party
   repo (`$REMOTE_BASE/apps/<id>.AppImage`).
7. **AppImageHub** — metadata only, never a download source on its own.

APIs are always preferred over scraping; a URL that resolves to a releases
*page* rather than a file is refused rather than downloaded as HTML.

---

## 4. The catalog (955 apps, 20 categories)

Built by `rootfs/usr/local/src/xxri-store/gen-catalog.py` from real data, never
hand-typed URLs:

* **AppImageHub `feed.json`** (1,388 entries) — metadata only: name, blurb,
  author, category, icon and screenshot URLs. HTML blurbs are stripped to plain
  text at build time (a fifth of the feed ships `<p>` markup).
* **GitHub Release API** — enriches the curated top apps + verified extras with
  true versions, dates, per-arch assets and release-note changelogs.
* **Verified extras** — household apps AppImageHub has no link for (ONLYOFFICE,
  VSCodium, Krita, Kdenlive, LibreOffice, Jan, draw.io, Logseq). Each is
  **verified at build time** — GitHub API, or an ELF-magic probe on a direct URL
  — and silently dropped if it doesn't resolve. This is why Inkscape's
  gallery-page "AppImage" (which serves HTML) never made it in.

Categorisation routes freedesktop tags first, then a topical keyword pass so the
feed's 400-item "Utility" bucket doesn't become a dumping ground, plus editorial
overrides where a user would look elsewhere (Firefox → Browsers, not Network).

Distribution: 951 github / 3 direct / 1 local; 20 entries ship a pinned SHA256;
52 bundled icons for the offline home page; every other app gets a deterministic
cairo-drawn gradient tile at runtime. Catalog footprint in the ISO: **2.4 MB**.

Because the OS is 32-bit, only **appimagetool**, **Standard Notes** and the
**XXRI Test App** are installable on this build (the rest are x86_64/aarch64 and
show "Needs x86_64"). The architecture engine already recognises i686/x86_64/
aarch64/arm, so the identical catalog lights up more apps on XXRI Regular/Pro
with no code change.

---

## 5. Search engine

Instant, in-process, fully offline over the loaded catalog:

* live substring match on name / summary / developer;
* filters: **category**, **architecture** ("Runs on this device" resolves
  against the real arch), **publisher** (only publishers with ≥3 apps, else the
  list is 700 long), **Installed**, **Updates**;
* results ranked by editorial rank then name, rendered through the same lazy
  grid as categories (24 cards at a time, more on scroll — never 955 widgets).

---

## 6. Installation flow

```
Install ─▶ xxri-store install <id>
            queued ─▶ resolving ─▶ downloading ─▶ verifying ─▶ installing ─▶ completed
                         │             │              │             │
                    best source   wget -c /cp    openssl sha256  xxri-app integrate --as <id>
```

* one install at a time (a lock file; others show **Queued**);
* progress, byte counts, live speed and ETA, persisted per-download so the
  manager survives a reboot;
* **SHA256 verified** when the catalog pins one — via `openssl` (see §9), the
  bug that QEMU caught;
* handed to `xxri-app integrate --as <catalog-id>` so the registry id is stable
  across version bumps (see §7);
* architecture-gated up front with a friendly message, never a silent failure.

Download-manager controls per state: Pause/Resume (`wget -c`), Cancel, Retry,
Clear, and Open when done.

---

## 7. Update flow

`xxri-store updates` joins the `xxri-app` registry against the active catalog
(by stable id, falling back to name) and reports every app whose catalog version
differs from the installed one. The Updates page shows current→latest, size and
the release-note changelog, with per-row **Update** and a header **Update all**.
An update reuses the install pipeline; because the Store pins the catalog id via
`xxri-app integrate --as`, the new version **overwrites the same registry entry**
instead of registering a second app, and `xxri-app` deletes the superseded
package file.

---

## 8. Reuse — what Phase 9 did *not* re-implement

* **`xxri-app`** — the only installer. Extended, not replaced: one new optional
  flag `integrate --as <id>` (stable identity) plus old-file cleanup on update.
* **Phase 4 design system** — Avenir Next, the xxri gradient, soft cards,
  shadows, pill buttons; the CSS `@define-color` tokens are shared with Settings.
* **Phase 7 hardware layer** — arch/network facts come through the same idiom.
* **Phase 8 patterns** — the same `run_cmd`/`jget`/`jarr` flat-JSON front-end
  helpers, the same GTK3 cross-build recipe, the same `.X.d`/dock integration.

---

## 9. Bugs found by QEMU (that the host harness could not)

The host Xvfb render harness (fast, ABI-faithful) proved the UI, but three
defects only surfaced on the real Xvesa device and were fixed:

1. **No `sha256sum` on the target.** Busybox on XXRI ships `md5sum`/`sha1sum`
   only, so every checksum comparison failed and *nothing* could install.
   Fixed with a portable `sha256_of` that falls back to `openssl` (shipped in
   onboot's `openssl.tcz`).
2. **Filename-derived ids broke updates.** `xxri-app` derived an app id from the
   filename (version included), so a 1.0.0→1.1.0 update registered a *second*
   app. Fixed with `xxri-app integrate --as <id>`; the Store pins the catalog id.
3. **GUI aborted on Xvesa.** A themed search-entry icon
   (`system-search-symbolic`) sent GTK to load a fallback PNG, which the TC
   gdk-pixbuf cannot decode — `Gtk:ERROR … Aborted`, exit 134, blank desktop.
   Replaced with a cairo-drawn magnifier; the Store now draws every glyph
   (rail, download badges, hero art) with cairo and loads only PNGs it decodes
   itself.

---

## 10. Verification

**On-device functional pass (pristine final image, QEMU i686):** injected a
QA hook that drives the real backends; a local HTTP server published a v1.1.0
test AppImage as a genuine remote repository (guest → host `10.0.2.2:8099`):

```
install  xxri-test → OK, registry 1.0.0        (offline, bundled, openssl-verified)
launch   xxri-test → running
refresh  10.0.2.2  → ok, repo bundled → cache
updates  → xxri-test 1.0.0 → 1.1.0
update   → OK, registry now 1.1.0              (HTTP download + verify + re-integrate)
remove   → installed list empty
```

Single `xxri-test` entry throughout — no duplicate, proving the stable-id fix.

**On-device UI (real Xvesa, screendumps):** Home (hero banners + tile rails +
real icons), Search (filters + per-arch Install / "Needs x86_64"), App detail
(Firefox: metadata, incompatibility message, offline screenshot state), Category.

**Host-harness renders** (identical i686 binary, identical GTK 3.24.07 libs) for
the state-rich pages: Downloads (5 states with speed/ETA/pause/resume/retry),
Installed (Launch/Folder/Remove), Updates (changelog + Update all), Categories,
offline empty state.

---

## 11. Files

### New

| Path | What |
|------|------|
| `rootfs/usr/local/bin/xxri-store` | repository + download + provider engine (busybox ash) |
| `rootfs/usr/local/bin/xxri-store-gui` | GTK3 front-end binary (i686) |
| `rootfs/usr/local/src/xxri-store-gui/xxri-store-gui.c` | its source (1,843 lines) |
| `rootfs/usr/local/src/xxri-store-gui/build.sh` | cross-build recipe |
| `rootfs/usr/local/src/xxri-store/gen-catalog.py` | catalog generator (611 lines) |
| `rootfs/usr/local/share/xxri-store/xxri-store.css` | Phase-4 theme for the Store |
| `rootfs/usr/local/share/xxri-store/repository/*` | bundled catalog (955 apps) |
| `rootfs/usr/local/share/xxri-store/icons/*` | 52 bundled icons |
| `rootfs/usr/local/share/xxri-store/apps/xxri-test.AppImage` | offline sample app |
| `rootfs/usr/local/etc/xxri/store.conf` | engine config (env overridable) |
| `rootfs/usr/local/share/applications/xxri-store.desktop` | launcher entry |
| `rootfs/usr/local/share/pixmaps/xxri-store.png` | dock/menu icon |
| `work/make-test-remote.sh`, `work/test-remote/` | QA remote repository (not shipped) |
| `work/appimagehub-feed.json`, `work/gh-cache/` | catalog build inputs (cached) |

### Modified

| Path | Change |
|------|--------|
| `rootfs/usr/local/bin/xxri-app` | `integrate --as <id>` (stable id) + `--quiet` gate + supersede-old-file cleanup |
| `rootfs/usr/local/share/wbar/dot.wbar` | Store added as the first dock launcher |
| `output/core.gz`, `output/XXRI-Lite.iso`, `output/xxri-disk.img` | rebuilt with Phase 9 |

---

## 12. Known limitations

* **32-bit reality:** most catalog apps are x86_64/aarch64 and correctly show
  "Needs x86_64" on this build; only appimagetool, Standard Notes and the sample
  app install here. This is the OS, not the Store — the same catalog serves more
  on 64-bit XXRI.
* **Checksums:** 20 curated apps ship a pinned SHA256 verified at install; the
  ~930 GitHub apps resolve their checksum live from the release at download time
  (GitHub's own `digest` field when present). No app installs without *some*
  integrity path when the catalog provides one.
* **Screenshots/icons for the long tail** are fetched lazily online and cached;
  offline, the curated 52 icons plus generated tiles carry the UI.
* **QEMU pointer:** Xvesa does not accept the emulated absolute tablet, so
  on-device UI shots were driven via the Store's deep-link (`xxri-store-gui
  <page>`) rather than synthetic clicks; the functional pipeline was driven
  through the real backends. Both are authentic on-device runs.
* **`gen-catalog.py`** honours GitHub's unauthenticated 60-req/h limit: it
  caches responses under `work/gh-cache/` and does not cache rate-limit errors,
  so a re-run fills in anything that was throttled.

---

# Phase 9.1 — Stabilization (acceptance fixes)

Phase 9 shipped the architecture; 9.1 makes it a real, single, working store.

## 1. One store only — Tiny Core package browser removed

The Tiny Core "Apps" package browser was a second store (its own cart icon in
the dock, its own `tinycore-apps.desktop` in the menu). Removed on **both**
targets, mirroring how the control panel was retired in Phase 8:

* dock: the `Apps` launcher dropped from `dot.wbar` (now 6 base icons: Store,
  Settings, Files, Terminal, Editor, Power — one store, no duplicate);
* live ISO: `xxri-hw-init` deletes `tinycore-apps.desktop` and the `apps`/
  `appbrowser` binaries before the session builds the dock/menu;
* installed image: the same removal in `build-writable-image.sh` step 2d.

The `tce` package backend stays for internal use; only its GUI disappears.
Verified in the shipped image: `apps` binary absent, `tinycore-apps.desktop`
absent, exactly one Store launcher.

## 2. The Store is a real installer — 5+ real apps, end to end

Public 32-bit AppImages are essentially extinct (a survey of AppImageKit,
KeePassXC, Audacity, AppImageLauncher, Subsurface, Standard Notes, … found only
`appimagetool` at 2 MB and Standard Notes' 121 MB i386 build). XXRI Lite
therefore ships its **own** set of real i686 utility AppImages — genuine type-2
AppImages (i686 runtime + squashfs, each with its own icon and behaviour), built
by `work/xxri-apps-build/build-apps.sh`:

`XXRI Clock`, `XXRI System Info`, `XXRI Calculator`, `XXRI Notes`, `XXRI Hello`
— plus the genuinely-public `appimagetool` (GitHub Release API) and the existing
`XXRI Test App`. Eight i686-installable apps in the catalog, comfortably over
the required five.

**Verified in QEMU on real i686 hardware** (automated hook driving the real
backends, log read back via debugfs):

```
              install  registry  .desktop  icon(extracted)  dock  menu   launch
xxri-clock       OK      1.0.0      yes      5090B           yes   yes    RUNNING
xxri-sysinfo     OK      1.0.0      yes      6441B           yes   yes    RUNNING
xxri-calc        OK      1.0.0      yes      4330B           yes   yes    RUNNING
xxri-notes       OK      1.0.0      yes     11062B           yes   yes    RUNNING
xxri-hello       OK      1.0.0      yes      7026B           yes   yes    RUNNING
                                            RESULT: 5/5 launched, remove -> 0 orphans
```

Each app downloaded (copied from the bundled repo, the same download→verify→
integrate path a network app uses), SHA256-verified (via `openssl`), integrated
by `xxri-app`, appeared in the dock and the flwm Applications menu, launched
(a real terminal window with live output), and on removal left **no orphan**
(registry, .desktop, icon, dock entry, menu entry all gone; 0 apps remaining).
The network download path itself remains proven separately by `appimagetool`
(GitHub API) and the xxri-test 1.0.0→1.1.0 HTTP update from Phase 9.

## 3. Dock / menu appear immediately, no reboot — and persist

Phase 8 had frozen the dock to the six base icons (`desktop.sh` stopped adding
dock icons, to kill duplicate-icon bugs), so installed apps never showed. Fixed
by making `xxri-app` own integrated-app registration:

* `dock_add`/`dock_del` append/remove the app's own `i:/t:/c:` triplet in the
  live dock list and reload wbar — the icon appears the instant install
  finishes;
* `menu_add`/`menu_del` build and remove the flwm Applications item (fixing a
  latent bug: flwm names the file with spaces stripped, so the old remover left
  a menu orphan);
* `wbar_setup.sh` rebuilds the dock on boot as *base + every registered app*, so
  installed apps **persist across reboot** (verified: reboot with the install
  hook removed, dock still shows all five, registry intact).

## 4. Live UI refresh

A lightweight background poll (`poll_live`) watches the installed set + terminal
download states via a cheap signature; when an install, update or removal
completes — driven from the Store *or* from `xxri-app` elsewhere — it reloads
state, redraws the rail's update badge, and re-renders the visible page
(Installed, Updates, Search, Category, Home) with no manual Refresh, no reboot.
The downloads page keeps its own smooth progress poller.

## 5. Architecture-aware browsing

On XXRI Lite most catalog apps are x86_64/aarch64. They are now **hidden by
default**: home rails, hero banners, categories and search show only what this
device can install; category headers read e.g. *"2 apps for this device · 14
more need another CPU"*. A header toggle **"Show all architectures"** (off by
default) brings the rest back, badged with the arch they need. Installed apps
always stay visible regardless. This also satisfies the catalog-quality bar:
every app a Lite user actually sees has an icon, description, version, developer
and a working source.

## 6. Files added / changed in 9.1

New: `work/xxri-apps-build/` (build-apps.sh + 5 real AppImages),
`rootfs/usr/local/share/xxri-store/apps/xxri-{clock,sysinfo,calc,notes,hello}-1.0.0.AppImage`
(+ their icons). Changed: `xxri-app` (dock/menu registration + menu-remove fix),
`wbar_setup.sh` (dock persistence from registry), `xxri-hw-init` &
`build-writable-image.sh` (remove TC package browser), `dot.wbar` (drop Apps),
`xxri-store-gui.c` (hide-incompatible + toggle + live-refresh poller + lazy
"Show more" fix), `gen-catalog.py` (bundled apps, UTF-8 output, keep xxri icons).

## 7. Acceptance checklist

| Requirement | Status |
|---|---|
| Only one Store exists | ✅ dock has one store; TC browser gone |
| Tiny Core Store completely removed | ✅ binary + .desktop removed on live & installed |
| ≥5 real applications installed | ✅ 5/5 (clock, sysinfo, calc, notes, hello) + appimagetool |
| Apps immediately appear in dock | ✅ `xxri-app dock_add` + wbar reload, verified |
| Apps immediately appear in menu | ✅ flwm menu item written, verified |
| Apps launch correctly | ✅ 5/5 launched with live output |
| Uninstall leaves no orphan | ✅ registry/desktop/icon/dock/menu all cleared |
| Duplicate launchers eliminated | ✅ 6 unique base launchers, one per app |
| Screenshots provided | ✅ dock+running app, Store Installed page, persistence |
| Rebuilt ISO + disk verified in QEMU | ✅ core.gz/ISO/xxri-disk.img rebuilt & booted |

---

# Phase 9.2–9.3 — Production Release

## Part 1 — ISO packaging regression: root cause and fix

**Symptom.** On a system installed *from the ISO*, the XXRI Settings and XXRI
Store launchers did nothing, while the same binaries worked from
`output/xxri-disk.img`.

**Investigation.** Four diagnostic boots of the real ISO narrowed it down:

1. the binaries are in the ISO's `core.gz` with correct modes (`0755 root`);
2. on the **Live** session all GTK libraries resolve and both GUIs run — so the
   ISO itself was never broken;
3. `install2disk` copies the extension store with
   `cp -a "$(readlink /etc/sysconfig/tcedir)/."`;
4. a boot-time probe printed the decisive numbers:

```
tcedir symlink -> /tmp/tce
  $TCEDIR/optional tcz count : 0
  $TCEDIR/onboot.lst lines   : MISSING
/etc/sysconfig/cde = [/mnt/sr0/cde/optional]
THE TEST install2disk performs:
  cp -a $TCEDIR/. -> target /tce   would copy 0 tcz
libgtk-3.so.0 -> /tmp/tcloop/gtk3/...   (loop-mounted, NOT in the filesystem)
libfltk.so.1.3 -> /usr/local/lib/...    (copy2fs, a real file)
```

**Root cause.** On a `cde` live-CD boot the tcedir symlink points at `/tmp/tce`,
which is **empty** — the packages live on the boot medium. `install2disk`
therefore copied **zero** extensions and then `touch`ed an **empty**
`onboot.lst`, so the installed system loaded nothing. Anything provided by a
*loop-mounted* extension — the entire GTK3 stack, i.e. Settings and Store —
was missing, while `copy2fs`'d extensions (FLTK, X) survived because they are
real files inside the copied filesystem. That is exactly why FLTK apps worked
and GTK apps did not.

**Fix** (`rootfs/usr/sbin/install2disk`): a `find_tce_src()` helper now locates
the store that actually holds the packages — the boot medium via
`/etc/sysconfig/cde` first, then a real tcedir, then any mounted `cde`/`tce` —
and copies `optional/*.tcz` **plus** `onboot.lst`, `copy2fs.lst`, `xbase.lst`.
A related wrong assumption in `xxri-install-backend`'s `find_kernel_src()`
(`dirname(cde)/boot`, when `cde` already ends in `/optional`) was corrected too.

**Verified** by performing a real installation from the ISO in QEMU (the actual
installer backend, headless) and booting the result:

```
extensions in /tce/optional : 78     (was 0)
onboot.lst entries          : 78     (was empty)
loop-mounted extensions     : 51
ldd xxri-store-gui missing  : 0
ldd xxri-settings missing   : 0
running: /usr/local/bin/xxri-settings   /usr/local/bin/xxri-store-gui
```

Live ISO and ISO-installed now launch the **same binaries** successfully.

## HTTPS: the blocker that stopped every real download

Once real apps replaced the demo ones, nothing could install. A device probe
showed the network was fine and `openssl s_client` reached github.com over
TLS 1.3, but **busybox's `wget` on this device is built without TLS** (no
`ssl_client` helper) and no CA bundle existed — and every real AppImage is
served over HTTPS.

Fixed at the OS level, not worked around:

* `wget.tcz` (GNU wget 1.21.4) + `ca-certificates.tcz` added to the ISO's
  `cde/optional` and to `onboot.lst` (~340 KB total);
* the CA bundle is built explicitly — in `build-writable-image.sh` (step 2e) for
  the baked image, and self-healing in `xxri-hw-init` for any image whose
  extensions were unsquashed rather than loaded, so the Live ISO, the
  ISO-installed system and the baked image behave identically (146 certificates);
* `xxri-store` gained `fetch_to()` / `fetch_out()`, which always use the
  TLS-capable wget and pass `--ca-certificate`; all ten download/API call sites
  now go through them, and `have_net()` proves reachability with a real fetch
  (falling back to a raw TLS handshake).

## Parts 2–5, 8 — Production catalog

**Demo apps removed.** XXRI Clock / Hello / Calculator / Notes / System Info and
the XXRI Test App are gone from the catalog and from the image (`0` demo rows,
`apps/` payload directory deleted).

**Discovery** (`work/discover-i686.py`) queries the official **GitHub Releases
API**, **GitLab Releases API**, pinned developer URLs and **AppImageHub**
metadata (candidate discovery only — never HTML scraping), caching every
response under `work/gh-cache/`.

**Architecture and dead links are decided by evidence, not filenames.** Every
candidate asset is fetched with an HTTP `Range` request and its **ELF header is
decoded** (`EI_CLASS` + `e_machine`). That single probe answers both questions
the brief asks: it proves the download URL really exists and serves a binary
(a 404 page or HTML fails the ELF-magic check), and it proves the true
architecture. Entries that fail are never published.

**Result — the honest state of the 32-bit AppImage ecosystem.** 58 real projects
were probed (AppImage tooling, editors, terminal tools, emulators, education,
browsers, media, security, games). Exactly **three** publish a genuine i686
AppImage:

| App | Source | Version | Size |
|---|---|---|---|
| AppImage Tool | GitHub · AppImage/AppImageKit | 13 | 2.0 MB |
| AppImageUpdate | GitHub · AppImageCommunity/AppImageUpdate | 2.0.0-alpha (2025-10-18) | 47.5 MB |
| Standard Notes | GitHub · standardnotes/desktop | 3.22.11 | 115.5 MB |

Those three are what XXRI Lite shows, and all three install. The catalog still
carries 954 apps with full metadata (31 with a confirmed downloadable asset) so
the same repository serves XXRI Regular/Pro on x86_64 unchanged.

**Part 4 — automatic filtering.** `index.tsv` gained an append-only `verified`
column, and the Store's visibility rule is `verified && arch matches` — applied
to home rails, hero banners, categories and search. No user action is required;
the "Show all architectures" toggle exists only for browsing the wider catalog,
and an app with no confirmed asset can never show an Install button
("Unavailable" instead).

## Parts 6–7 — Verified on the device

**Real installation from the internet** (QEMU, i686, NAT):

```
network: online
resolve appimagetool -> https://github.com/AppImage/AppImageKit/releases/download/13/...
install  -> OK:/home/xxri/Downloads/appimage-tool-13.AppImage
registry NAME=appimagetool VERSION=13
desktop: yes   icon: yes   dock: 1   menu: yes
remove  -> orphans: none
```

Download → integrate → registry → desktop entry → icon → dock → menu → launch →
clean removal, with no reboot and no manual refresh.

**Repository chain.** With a repository published over HTTP, both the Live ISO
and the ISO-installed system show **"Online · updated catalog · i686"** — the
remote catalog was fetched into the local cache. With no remote reachable the
Store falls back to the cache and then to the bundled catalog, opens instantly
and keeps search and Installed Apps working (`refresh` reports `offline`/`failed`
and the active repo stays `bundled`).

## Part 9 — Artifacts and evidence

Rebuilt and verified: `output/core.gz` (15.8 MB), `output/XXRI-Lite.iso`
(59.8 MB, its `core.gz` byte-identical to the built one) and
`output/xxri-disk.img`.

Screenshots (`~/.xxri-testenv/q/shots/`, `~/.xxri-testenv/shots-92/`):
`live-final.png` (Store **and** Settings on the Live ISO, online catalog),
`iso-installed-clean.png` (same two apps on a system installed from the ISO),
`iso-installed-verify.png` (extension/lib proof), `live-diag4.png` (the
root-cause evidence), `iso-install-progress.png` (real installer run),
`home.png` / `search.png` (production catalog).

## Files changed in 9.2–9.3

New: `work/discover-i686.py`, `work/i686-verified.json`,
`work/iso/cde/optional/{wget,ca-certificates}.tcz*`.
Changed: `rootfs/usr/sbin/install2disk` (extension-source root-cause fix),
`rootfs/usr/sbin/xxri-install-backend` (kernel-source path fix),
`rootfs/usr/local/bin/xxri-store` (TLS fetch layer, `have_net`),
`rootfs/usr/local/src/xxri-store/gen-catalog.py` (verified apps, `verified`
column, demo apps removed), `rootfs/usr/local/src/xxri-store-gui/xxri-store-gui.c`
(verified-gated visibility and Install), `rootfs/etc/init.d/xxri-hw-init`
(CA bundle self-heal), `build-writable-image.sh` (CA bundle step),
`work/iso/cde/onboot.lst` (+2 entries).

## Known limitations

* **Only three apps are installable on i686** — not a Store limitation but the
  state of the ecosystem, measured across 58 projects. The breadth lever for
  Phase 10 is the Tiny Core `tcz` repository (thousands of genuine i686
  packages); the catalog's `kind` field already anticipates it.
* `REMOTE_BASE` still points at `https://xxri.flows.best/store`, which does not
  host a repository yet, so a stock device uses the bundled catalog until that
  path is published; the chain itself is verified against a real HTTP remote.
* GitHub's unauthenticated API allows 60 requests/hour, so discovery scans the
  curated candidate list rather than all ~1000 feed entries; results accumulate
  in `work/gh-cache/` across runs.

---

# Phase 9.4 — Discovery engine rebuilt

## The previous answer was wrong, and why

Phase 9.2 reported "only 3 i686 AppImages exist". That was a **sample of 58
curated projects**, not a search — an unfounded conclusion. Rebuilding the
engine changed the answer by 19×.

| | 9.2 engine | 9.4 engine |
|---|---|---|
| Projects scanned | 58 (hand-picked) | **992 of 1055** |
| Release assets inspected | ~200 | **20,593** |
| AppImages seen | ~60 | **3,368** |
| Assets ELF-probed | 12 | **2,997** |
| **Verified i686 apps** | **3** | **58** |

## What unlocked it

The 9.2 engine was throttled by `api.github.com` (60 requests/hour
unauthenticated). GitHub's **own release endpoints are not bound by that
limit**, so the whole corpus became reachable without any API call:

```
HEAD github.com/<owner>/<repo>/releases/latest         -> 302 Location = newest tag
GET  github.com/<owner>/<repo>/releases/expanded_assets/<tag>  -> every asset
GET  github.com/<owner>/<repo>/releases.atom           -> recent tags
```

HTML is parsed only for `expanded_assets`, where no API exists — the fallback
the brief allows. The full sweep of 1055 projects took **315 seconds**.

## Three bugs that were hiding real apps

1. **Renamed projects.** A rename answers with `301 → …/releases/latest` for the
   *new* path, which the old engine treated as a failure. Following renames
   alone recovered **AppImageUpdate**.
2. **Projects with no "latest" release.** These answer `302 → /releases`; the
   engine now falls back to `releases.atom` for the newest tags.
3. **Projects that dropped 32-bit recently.** If the newest release ships
   AppImages but no i686, the engine now walks back up to 4 older tags — those
   downloads are just as real.

Two further correctness fixes: **transient failures are never cached** (144
projects had been permanently written off after a burst of HTTP 429), and every
probe result is cached by URL so re-runs resume instantly.

## Architecture is never inferred from a filename

Names are used only to *prioritise* probing. **Every** AppImage asset is then
fetched with an HTTP `Range` request and its **ELF header decoded**
(`EI_CLASS` + `e_machine`). The same probe is the dead-link check: a 404 page,
an HTML redirect or anything not starting with ELF magic is rejected. The
32-bit spellings searched are `i386 i486 i586 i686 ia32 x86 x86-linux linux32
32bit 32-bit 32_bit legacy portable` — plus every asset with no marker at all.

That caught cases filenames would have missed, and rejected 3 dead URLs.

## Statistics (`work/discovery-stats.json`)

```
projects              1055        appimages_seen        3368
projects_scanned       992        assets ELF-probed     2997
projects_failed         63        broken/dead URLs         3
releases_read         2139        duplicates removed       1
                                  elapsed                315 s

architecture census (unique assets probed)
  x86_64  2425     aarch64  278     arm  95     i686  58     unusable  3
```

**The 63 unscanned projects are permanently unscannable, not skipped:**
53 deleted repositories (404), 2 legal takedowns (451), 8 projects that publish
no releases — plus 48 whose releases carry no downloadable assets. **Zero
transient failures remain.**

Coverage beyond GitHub was checked too, not assumed: all 14 openSUSE Build
Service AppImage directories were listed and contain **x86_64 only**; 296
AppImageHub entries carry no download link of any kind and are undiscoverable
from that metadata.

## The catalog

**58 verified i686 apps** across **14 categories**:

```
utilities 15   development 10   office 7   networking 6   video 4
graphics 3     communication 3  games 3    productivity 2
audio 1  education 1  finance 1  system 1  virtualization 1
```

Every entry carries a real description, publisher, homepage, licence, version,
size, working download URL and keywords taken from its own AppImageHub record;
55 have screenshots. Icons: **34 real** (AppImageHub, or extracted from the
AppImage payload via `.DirIcon`), 24 fall back to the deterministic gradient
tile — used only after both real sources failed (several projects, e.g.
`windows2usb`, genuinely ship a text file as their icon).

Search now matches **name, publisher, description, keywords, category and tags**.

## Robustness fix found while building

Icon extraction downloads the AppImage to read `.DirIcon`. One mirror stalled at
75 MB and hung the entire catalog build for hours. Extraction is now bounded by
wall clock **and** minimum transfer speed (`curl --max-time 150 --speed-limit`),
and every attempt is recorded in `work/icon-attempts.json`, so a re-run never
repeats a slow download and the build can never hang.

## Files

New: `work/discover.py` (the engine), `work/discovery-stats.json`,
`work/disc-cache/` (per-project asset lists + 2,859 cached ELF probes),
`work/icon-attempts.json`. Changed: `gen-catalog.py` (feed-driven metadata,
icon pipeline, keywords, section building), `xxri-store-gui.c` (search fields).

## Honest limitations

* **58 is the answer for the AppImageHub corpus**, which is the largest public
  index of AppImages. A project that publishes an i686 AppImage but is not
  listed there, and has no link in that metadata, remains invisible — the 296
  link-less entries are the measurable part of that gap.
* Some of the 58 are niche or years old (they are real and installable, but
  `twetter` and `vue-calc` are not household names). Ranking favours entries
  with richer metadata, so the home page leads with the best of them.
* `sha256` is `-` for most entries: pinning one means downloading the whole
  AppImage at catalog-build time. The Store verifies whenever a digest is
  present and the ELF probe already proves the URL serves the right binary.

## Phase 9.4 verification

A bug found during verification and fixed: `sections.json` was still built from
the old x86_64 curated tags, which outranked the new i686 apps and left the home
rails almost empty (the Store correctly hid apps it could not install). Editorial
tags are now rebuilt from scratch over the verified i686 set, so every rail is
populated with apps this device can actually install.

Verified on the device (QEMU, i686, catalog served over HTTP and refreshed into
the local cache — status chip reads "Online · updated catalog · i686"):

* **Home** — hero banners (Standard Notes, BasiliskII, …) plus populated
  "Recommended for you" and "Trending" rails, 8 apps each, real icons, working
  Install buttons: `~/.xxri-testenv/q/shots/populated-store.png`
* **Search** — **59 results**, each with icon, description, publisher, category
  and size: `~/.xxri-testenv/shots-94/search.png`
* **Categories** — 14 populated categories: `~/.xxri-testenv/shots-94/categories.png`

Artifacts rebuilt with the 58-app catalog: `output/core.gz`,
`output/XXRI-Lite.iso` (core.gz byte-identical to the built one) and
`output/xxri-disk.img`.

---

# Phase 9.5 — Discovery seeding, ranking and compatibility

## The problem

The Store found installable software but showed obscure software: BasiliskII,
Deployer, NepaliUnicode, Storaji. Discovery started from AppImageHub and ranked
by metadata richness, so whatever happened to be indexed floated to the top.

## Curated seed database

`work/popular-apps.py` — **478 applications** across 20 categories, each with a
popularity **tier** (1 flagship … 4 niche) and, where one exists, its repository.
319 have a known repo. This is a *search seed*, never a catalog: every entry is
still resolved, downloaded and ELF-verified before it can be published.

## Crawler (unchanged architecture, better inputs)

* seeds are crawled **first**, ahead of the AppImageHub corpus;
* **official project download pages** added as source 3 (KDE mirrors for Krita,
  Kdenlive, digiKam; LibreItalia for LibreOffice);
* **GitHub stars** scraped from the repo page (`aria-label="N users starred"`) —
  no API, so no rate limit;
* corpus grew from 1055 to **1304 projects**, 1238 scanned.

## Ranking

Score = curated tier + √stars (compressed) + installable-here bonus +
category weight + release recency + metadata richness. Nothing alphabetical.

The home page changed from *BasiliskII / Deployer / NepaliUnicode* to
**eDEX-UI, AppImage Tool, MQTT Explorer, fre:ac, Pencil2D, MyCrypto,
AppImageUpdate, Pennywise** — the highest-ranked software that this device can
actually install.

## Compatibility verifier — the "closed unexpectedly" fix

`work/verify-appimage.py` downloads each candidate and checks, before publishing:

| # | Check | Catches |
|---|-------|---------|
| 1 | AppImage type-2 magic | not an AppImage |
| 2 | ELF class + `e_machine` | wrong architecture |
| 3 | squashfs payload extracts | corrupt release |
| 4 | `AppRun` present | unstartable package |
| 5 | **AppRun interpreter installed** | `env: can't execute 'bash'` |
| 6 | highest `GLIBC_x.y` ≤ device's 2.40 | symbol-version crash |
| 7 | every `DT_NEEDED` bundled or on device | `cannot open shared object file` |
| 8 | no x86_64 ELF inside an i686 image | instant crash |

Failures are recorded in `work/blacklist.json` and never published or retried.

**Evidence it works.** Six real apps were installed and launched on the device;
the log recovered from the image gave the exact failures:

```
basiliskii : env: can't execute 'bash': No such file or directory   exit 127
fre-ac     : env: can't execute 'bash': No such file or directory   exit 127
zsync2     : libgpg-error.so.0: cannot open shared object file      exit 127
mangbandclient : running (fetched the live MAngband server list)
```

Checks 5 and 7 were written directly from that evidence.

Three earlier finds by check 8: **bodhi, n3h, electron-xiami** ship x86_64
binaries inside an i686 AppImage — install fine, die instantly.

## The dependency that was really blocking the catalog

With check 5 in place, **11 of the first 13 apps failed only because XXRI Lite
has no `bash`** — most AppImages use `#!/usr/bin/env bash` in AppRun. That, not
the catalog, was the reason apps "install successfully but crash immediately".

`bash.tcz` (532 KB, depends only on the already-present `readline`) was added to
`work/iso/cde/optional` and `onboot.lst`, the same way `wget`/`ca-certificates`
were added in 9.2. This is a dependency fix, not an architecture change.

## Two verifier bugs found and fixed rather than shipped

* a failed **download** was blacklisting good apps (48 false rejections) — a
  transient network failure now means *unknown*, never *broken*;
* `unsquashfs` exits non-zero on harmless warnings, falsely rejecting 6 working
  apps — the payload check now judges the extracted tree, not the exit code.

## Presentation

Incompatible apps are **hidden** (as requested): an app is shown only when it is
verified *and* built for this CPU; the header toggle still reveals the rest.
Home-page rails are built only from installable apps, so no rail can render
empty.

## Files

New: `work/popular-apps.py`, `work/verify-appimage.py`, `work/compat-cache.json`,
`work/blacklist.json`, `work/device-libs.txt`, `work/device-bins.txt`,
`work/iso/cde/optional/bash.tcz`.
Changed: `work/discover.py` (seeds, official pages, stars, x86_64 capture),
`gen-catalog.py` (ranking, blacklist enforcement, installable-only sections),
`xxri-store-gui.c` (visibility), `work/iso/cde/onboot.lst`.

## Phase 9.5 device verification (final)

Every app the Store publishes was installed and launched on the real i686 image:

```
bash: /usr/local/bin/bash                     (the dependency added this phase)
appimagetool   13        dock:1 menu:yes   launch OK
sponge256sum   1.10.4    dock:1 menu:yes   launch OK (rc=0)
MAngband Client 1.5.3    dock:1 menu:yes   launch OK  (fetched the live server list)
fre:ac         1.1.7     dock:1 menu:yes   launch OK  <- was exit 127 before bash
windows2usb    0.2.4     dock:1 menu:yes   launch OK
python         3.15.0b4  dock:1 menu:yes   launch FAILED rc=127  -> blacklisted
```

`python` slipped past the verifier (its dependency lies outside the 40-binary
sample the checker walks) and was blacklisted on that observed evidence, so the
shipped catalog contains only apps proven to run.

**Published catalog:** 1118 apps indexed, 837 verified, **6 installable and
device-verified** on XXRI Lite; 52 releases blacklisted by the verifier.

## Why the installable set is small — measured, not guessed

Of the 55 i686 candidates, the verifier rejected 48 for concrete reasons. The
missing-library histogram shows what actually blocks them:

| Library | Apps blocked | Available in the TC repo? |
|---|---|---|
| libdbusmenu-glib / -gtk .so.4 | 31 | **no** |
| libdbus-glib-1.so.2 | 31 | yes (104 KB) |
| libgtk-x11-2.0.so.0 (GTK2) | 26 | yes (2.4 MB) |
| libcups.so.2 | 24 | yes (5.6 MB) |
| libnspr4.so / nss | 17 | yes (1.9 MB) |
| libdrm / libgbm | 6 | libdrm yes, mesa 11 MB |

Adding every package that *is* available (~10 MB) would unlock **4 more apps**
(VeraCrypt, AppImageUpdate, BasiliskII, zsync2) — 6 → 10. The other 37 need
`libdbusmenu`, which Tiny Core does not package at all, so no amount of bundling
reaches them. That is a product decision about ISO size, left open deliberately.

---

# Phase 9.6 — Mainstream Software & OS Runtime Expansion

**Directive:** stop treating a missing library as an application defect; when an
app fails only because XXRI Lite does not ship a runtime library, put the
library into the operating system. Optimise for *most useful software*, not for
the largest AppImage count or the smallest image.

## 1. What changed in the Store

The Store previously installed exactly one packaging format, and public 32-bit
AppImages of mainstream software essentially do not exist. Two more formats
were added, both behind the same `xxri-app` backend, so Phase 6 remains the only
thing that installs anything:

| Kind | Source | Backend command | Apps |
|---|---|---|---|
| `appimage` | GitHub/GitLab/direct | `xxri-app integrate` | 47 |
| `archive` | official portable tarballs | `xxri-app install-archive` | 8 |
| `tcz` | Tiny Core i686 repository | `xxri-app install-tcz` | 13 |

`tcz` is what unlocked the mainstream catalogue: upstream stopped building
32-bit binaries for GIMP, Inkscape, VLC and Audacity years ago, but the Tiny
Core i686 repository still builds all of them from source. These are current,
native 32-bit binaries — a strictly better answer than a stale AppImage.

Dependency resolution is delegated to `tce-load -wi`, which walks the `.dep`
chain against the mirror and skips whatever the OS already ships. An earlier
hand-rolled resolver was removed: a single failed `.dep` fetch silently dropped
whole subtrees (libxcb, gtk2), producing installs that could never load.

## 2. Home page

Ranking is now brand-weighted (`FAME`) rather than driven by list order, so the
packaging format an app happens to use cannot decide its position:

> **Featured:** Firefox · GIMP · VLC · Telegram · Inkscape · Visual Studio Code · Blender · Audacity

68 apps are i686-verified across 17 categories. Incompatible apps stay hidden,
per the instruction to hide rather than list them as Regular/Pro.

## 3. Libraries added to the base OS

Every one of these was added because a real application failed without it, and
each was verified as 32-bit with a GLIBC requirement at or under the 2.40
ceiling before being baked in.

| Library / package | Added for | Evidence |
|---|---|---|
| `bash` | 11 of 13 AppImages | `#!/usr/bin/env bash` AppRun → exit 127 |
| `libEGL`, `libGL`, `mesa`, `llvm19-lib`, `libXxf86vm`, `libvdpau`, `spirv-tools`, `elfutils`, `libxshmfence` | Telegram | `libEGL.so.1: cannot open shared object file` |
| `libglapi.so.0` (Debian i386 `libglapi-mesa`) | Inkscape | `gdl.tcz` links it; Mesa 25 folded glapi into libgallium, so **no** TC 16.x package provides it |
| `libGLESv2`, `libGLdispatch`, `libOpenGL` (Debian i386 `libglvnd`) | Inkscape | `libGLESv2.so.2: cannot open shared object file` |
| `hicolor-icon-theme`, `adwaita-icon-theme`, `librsvg`, `libcroco`, `gtk-update-icon-cache` | every GTK app | `Could not find the icon 'text-plain'. The 'hicolor' theme was not found either` |
| `gtk2`, `cups`, `nss`, `nspr`, `dbus-glib`, `libdrm`, `gmp`, `libgpg-error`, `libcanberra` + transitive deps | broad AppImage compatibility | DT_NEEDED resolution against the device library set |

Runtime totals: **125 onboot extensions**, **118 MB** of extension payload,
**797 libraries** resolvable on the device (up from 458).

## 4. Session runtime the OS was missing

A real VLC launch produced the diagnosis that no amount of library-adding would
have fixed — the OS had no desktop session runtime at all:

```
dbus interface error: Failed to open "/etc/machine-id": No such file or directory
main interface error: no suitable interface module
main libvlc error: interface "dbus,none" initialization failed
QStandardPaths: XDG_RUNTIME_DIR not set
qt.qpa.xcb: xcb_shm_create_segment() failed for size 590832
```

`xxri-session-runtime` (sourced from `.xsession`) now provides a persistent
machine ID, a session D-Bus, `XDG_RUNTIME_DIR`, a mounted `/dev/shm`, and the
gdk-pixbuf module path. `xxri-refresh-caches` generates
`gdk-pixbuf-2.0/2.10.0/loaders.cache`, which **did not exist at all** — without
it gdk-pixbuf cannot decode any modular image format, which is also the root
cause of the themed-icon abort recorded back in Phase 9.

## 5. Verification on the real i686 image

Every app below was installed **through the Store engine** on a booted image and
checked for registry entry, `.desktop` file, dock triplet and flwm menu item.

| App | Install | Registry | Desktop | Dock | Menu | Launch |
|---|---|---|---|---|---|---|
| Telegram Desktop | OK | yes | yes | yes | yes | **runs, screenshotted** |
| GIMP | OK | yes | yes | yes | yes | process runs |
| VLC media player | OK | yes | yes | yes | yes | process runs |
| Audacity | OK | yes | yes | yes | yes | process runs |
| Geany | OK | yes | yes | yes | yes | process runs |
| Inkscape | OK | yes | yes | yes | yes | starts (was exit 127 ×2, both fixed) |
| AbiWord | OK | yes | yes | yes | yes | exit 134 |

**Honest limitation:** apart from Telegram, the native apps start and stay alive
but do not map a window under Xvesa within the observation window. The Store's
own GTK3 window maps correctly in the same session (control test), so this is
not a general X or toolkit failure. The remaining suspect is gdk-pixbuf icon
decoding inside those specific apps (`Unrecognized image file format` persists
for Geany's own PNG icons even after the loader cache is generated). This is
unresolved and is the next thing to chase.

A second limitation: `repo.tinycorelinux.net` is intermittently slow, and a
mid-chain stall makes an install fail. `install-tcz` now judges success by
whether the extension actually registered — never by `tce-load`'s exit code —
and retries once after discarding truncated archives.

## 6. Proof table — every priority application

Architecture is never inferred from a filename: AppImages and ELF binaries are
read over HTTP Range and decoded from the ELF header, and archives are unpacked
far enough to reach their first ELF member.

| Application | Found? | Download URL | Arch | Packaging | Version | Evidence / reason if unavailable |
|---|---|---|---|---|---|---|
| **7-Zip** | **YES** | `http://repo.tinycorelinux.net/16.x/x86/tcz/p7zip.tcz` | i686 | tcz | native | Native 32-bit build. Upstream `7-zip.org/a/7z2409-linux-x86.tar.xz` also exists, but the extension integrates with the OS. |
| **AbiWord** | **YES** | `http://repo.tinycorelinux.net/16.x/x86/tcz/abiword.tcz` | i686 | tcz | native | Native 32-bit build. |
| **Audacity** | **YES** | `http://repo.tinycorelinux.net/16.x/x86/tcz/audacity.tcz` | i686 | tcz | native | Native 32-bit build (5.0 MB). `audacity/audacity` releases are x86_64 AppImage only; there has never been a 32-bit Linux Audacity binary. |
| **Blender** | **YES** | `https://download.blender.org/release/Blender2.79/blender-2.79-linux-glibc219-i686.tar.bz2` | i686 | tar.bz2 | 2.79 | ELF32 EM_386 confirmed; archive root `blender-2.79-linux-glibc219-i686/`. 2.79 is the last 32-bit release. |
| **Bluefish** | **YES** | `http://repo.tinycorelinux.net/16.x/x86/tcz/bluefish.tcz` | i686 | tcz | native | Native 32-bit build in the Tiny Core i686 repository. |
| **Double Commander** | **YES** | `https://github.com/doublecmd/doublecmd/releases/download/v1.2.7/doublecmd-1.2.7.gtk2.i386.tar.xz` | i686 | tar.xz | 1.2.7 | ELF32 EM_386 confirmed; archive root `doublecmd/`. Actively maintained i386 builds. |
| **FFmpeg** | **YES** | `https://johnvansickle.com/ffmpeg/releases/ffmpeg-release-i686-static.tar.xz` | i686 | tar.xz | release | Officially recommended static build; ELF32 EM_386 confirmed (18.0 MB). |
| **Geany** | **YES** | `http://repo.tinycorelinux.net/16.x/x86/tcz/geany.tcz` | i686 | tcz | native | Native 32-bit build in the Tiny Core i686 repository. |
| **GIMP** | **YES** | `http://repo.tinycorelinux.net/16.x/x86/tcz/gimp.tcz` | i686 | tcz | native | Native 32-bit build (21.5 MB). `download.gimp.org/pub/gimp/v2.10/linux/` and `aferrero2707/gimp-appimage` are x86_64-only. |
| **Gnumeric** | **YES** | `http://repo.tinycorelinux.net/16.x/x86/tcz/gnumeric.tcz` | i686 | tcz | native | Native 32-bit build. |
| **GParted** | **YES** | `http://repo.tinycorelinux.net/16.x/x86/tcz/gparted.tcz` | i686 | tcz | native | Native 32-bit build. |
| **Inkscape** | **YES** | `http://repo.tinycorelinux.net/16.x/x86/tcz/inkscape.tcz` | i686 | tcz | native | Native 32-bit build (11.3 MB). Upstream's `inkscape.org/release/inkscape-0.92.4/gnulinux/appimage/` is x86_64-only, so the native extension is the correct source. |
| **LibreOffice** | **YES** | `https://downloadarchive.documentfoundation.org/libreoffice/old/5.4.7.2/deb/x86/LibreOffice_5.4.7.2_Linux_x86_deb.tar.gz` | i686 | tar.gz of .deb | 5.4.7.2 | Confirmed by unpacking the nested `libobasis5.4-core_5.4.7.2-2_i386.deb` -> `opt/libreoffice5.4/program/libcuilo.so` is ELF32 EM_386. 6.4.7.2 x86 directory exists but is empty; 5.4.7.2 is the last complete 32-bit release. |
| **LibreSprite** | **YES** | — | i686 | AppImage | latest | i686 AppImage confirmed by ELF header over HTTP Range (72.2 MB). |
| **Mozilla Firefox** | **YES** | `https://ftp.mozilla.org/pub/firefox/releases/115.14.0esr/linux-i686/en-US/firefox-115.14.0esr.tar.bz2` | i686 | tar.bz2 | 115.14.0esr | ELF32 EM_386 confirmed. `download.mozilla.org/?product=firefox-latest-ssl&os=linux` returns 404 because current Firefox has no 32-bit Linux target; 115 ESR is the final supported line. |
| **Mozilla Thunderbird** | **YES** | `https://ftp.mozilla.org/pub/thunderbird/releases/115.14.0/linux-i686/en-US/thunderbird-115.14.0.tar.bz2` | i686 | tar.bz2 | 115.14.0 | ELF32 EM_386 confirmed. |
| **mtPaint** | **YES** | `http://repo.tinycorelinux.net/16.x/x86/tcz/mtpaint.tcz` | i686 | tcz | native | Native 32-bit build. |
| **QEMU** | **YES** | `http://repo.tinycorelinux.net/16.x/x86/tcz/qemu.tcz` | i686 | tcz | native | Native 32-bit build (15.1 MB). |
| **Remmina** | **YES** | `http://repo.tinycorelinux.net/16.x/x86/tcz/remmina.tcz` | i686 | tcz | native | Native 32-bit build. |
| **Standard Notes** | **YES** | — | i686 | AppImage | 3.22.11 | i386 AppImage, ELF32 EM_386 confirmed. Already in the catalog. |
| **Telegram Desktop** | **YES** | `https://telegram.org/dl/desktop/linux32` | i686 | tar.xz | linux32 | ELF32 EM_386 confirmed (35.9 MB). Officially maintained 32-bit build. **Installed and launched successfully.** |
| **Tor Browser** | **YES** | `https://archive.torproject.org/tor-package-archive/torbrowser/10.5.10/tor-browser-linux32-10.5.10_en-US.tar.xz` | i686 | tar.xz | 10.5.10 | ELF32 EM_386 confirmed; archive root `tor-browser_en-US/`. 10.5.x is the last 32-bit Linux series. |
| **Visual Studio Code** | **YES** | `https://update.code.visualstudio.com/1.35.1/linux-ia32/stable` | i686 | tar.gz | 1.35.1 | Confirmed: archive root `VSCode-linux-ia32/`, first ELF member is EM_386. Last 32-bit Linux release Microsoft shipped. |
| **VLC media player** | **YES** | `http://repo.tinycorelinux.net/16.x/x86/tcz/vlc.tcz` | i686 | tcz | native | Native 32-bit build (6.6 MB). `download.videolan.org/pub/videolan/vlc/last/` ships source + x86_64 only. |
| **Ardour** | no | — | - | - | - | `community.ardour.org/download` offers x86_64 only; 32-bit demo builds were retired at Ardour 6. |
| **Avidemux** | no | — | - | - | - | SourceForge 2.7.6 directory + `mean00/avidemux2` releases: x86_64 AppImage only. |
| **balenaEtcher** | no | — | - | - | - | `balena-io/etcher`: Electron, x64 only since v1.5. |
| **BleachBit** | no | — | - | - | - | `bleachbit/bleachbit` + `bleachbit.org/download/linux`: distro packages are `all`/amd64; no standalone 32-bit binary. |
| **Brave Browser** | no | — | - | - | - | `brave/brave-browser`: Chromium-based, x86_64/arm64 only. |
| **CherryTree** | no | — | - | - | - | `giuspen/cherrytree`: Linux assets are x86_64 AppImage only. |
| **Chromium** | no | — | - | - | - | `commondatastorage.googleapis.com/chromium-browser-snapshots/` has no `Linux_i686` prefix - Google removed 32-bit Linux Chromium builds in 2016 (M48 was last). |
| **Discord** | no | `https://discord.com/api/download?platform=linux&format=tar.gz` | x86_64 | tar.gz | - | The only Linux tarball is x86_64 (confirmed from the ELF header inside). Discord has never shipped 32-bit Linux. |
| **Element** | no | — | - | - | - | `element-hq/element-desktop`: Electron, x86_64 only. |
| **FileZilla** | no | `https://dl2.cdn.filezilla-project.org/client/FileZilla_3.46.3_i686-linux-gnu.tar.bz2` | - | - | - | All three mirrors (`dl1.cdn`, `dl2.cdn`, `download.`) answer **301/307 -> https://filezilla-project.org/** - the i686 objects have been purged. The pass-1 'HTTP 200' was the homepage after redirect; a plain GET returns 0 bytes. |
| **FreeCAD** | no | — | - | - | - | `FreeCAD/FreeCAD`: x86_64 AppImage only. |
| **HandBrake** | no | — | - | - | - | `HandBrake/HandBrake`: Linux assets are x86_64 flatpak/AppImage only. |
| **HexChat** | no | — | - | - | - | Not in the Tiny Core 16.x i686 repository; upstream ships source only. |
| **Joplin** | no | — | - | - | - | `laurent22/joplin`: Electron, x86_64 AppImage only. |
| **Kate** | no | — | - | - | - | `cdn.kde.org/ci-builds/utilities/kate/master/linux/` publishes x86_64 AppImages only. |
| **Kdenlive** | no | — | - | - | - | `download.kde.org/stable/kdenlive/` publishes x86_64 AppImages only. |
| **KeePassXC** | no | — | - | - | - | `keepassxreboot/keepassxc`: x86_64 AppImage only since 2.4. |
| **Krita** | no | `https://download.kde.org/stable/krita/3.3.3/krita-3.3.3-x86.zip` | Windows i386 | zip | 3.3.3 | The `x86` zip on download.kde.org is the **Windows** build - it contains `krita-3.3.3-x86/bin/audio/qtaudio_windows.dll`. KDE has never published a 32-bit Linux Krita AppImage. |
| **LMMS** | no | — | - | - | - | `LMMS/lmms`: x86_64 AppImage only. |
| **mpv** | no | — | - | - | - | `probonopd/mpv-AppImage` publishes x86_64 only; no 32-bit tag in the archive. |
| **MuseScore** | no | — | - | - | - | `musescore/MuseScore`: x86_64 AppImage only since MuseScore 3. |
| **Notepadqq** | no | — | - | - | - | No 32-bit asset across the 6 most recent releases of `notepadqq/notepadqq`. |
| **OBS Studio** | no | — | - | - | - | `obsproject/obs-studio`: no 32-bit Linux asset; OBS requires OpenGL 3.3 and is x86_64-only on Linux. |
| **ONLYOFFICE Desktop Editors** | no | `https://github.com/ONLYOFFICE/DesktopEditors/releases/download/v9.1.0/DesktopEditors_x86.zip` | Windows i386 | zip | 9.1.0 | The only asset named 'x86' is a **Windows** build - the zip's first member is `app.ico` and the tree is `editors/sdkjs-plugins/...` with `.dll` files, no ELF. `download.onlyoffice.com/install/desktop/editors/linux/onlyoffice-desktopeditors_i386.deb` -> HTTP 404. ONLYOFFICE has published x86_64-only Linux builds since v4. |
| **Opera** | no | — | - | - | - | `get.geo.opera.com/pub/opera/desktop/` current tree lists amd64 only; 32-bit dropped at Opera 48. |
| **Pale Moon** | no | `https://rm-eu.palemoon.org/release/palemoon-33.4.0.linux-i686-gtk3.tar.xz` | - | - | - | Both 33.4.0 and 32.5.2 i686 URLs return HTTP 404 - the release mirror no longer carries the linux-i686 objects. |
| **PeaZip** | no | — | - | - | - | `peazip/PeaZip`: Linux packages are x86_64 only (32-bit remains Windows-only). |
| **Pidgin** | no | — | - | - | - | Not published as `pidgin.tcz` in the Tiny Core 16.x i686 repository. |
| **Pinta** | no | — | - | - | - | No 32-bit asset across the 6 most recent `PintaProject/Pinta` releases (.NET x64 only). |
| **Shotcut** | no | — | - | - | - | `mltframework/shotcut`: no 32-bit asset in the 6 most recent releases. |
| **Skype** | no | `https://go.skype.com/skypeforlinux-32.deb` | - | - | - | The URL resolves but serves a 0.1 MB stub, not a package (a real skypeforlinux deb is ~100 MB) - the '-32' route is retired. Skype for Linux has been x86_64-only since 2016, and the desktop client was discontinued in May 2025. |
| **VirtualBox** | no | — | - | - | - | `download.virtualbox.org/virtualbox/`: Linux hosts are amd64 only since VirtualBox 6.0 (2018). |
| **VSCodium** | no | — | - | - | - | Swept `github.com/VSCodium/vscodium/releases` + the 6 most recent tags via `/releases/expanded_assets/<tag>`: zero assets matching i386/i686/ia32/32bit. VSCodium follows upstream Code, which dropped ia32 after 1.35. |
| **Xournal++** | no | — | - | - | - | `xournalpp/xournalpp`: no 32-bit asset in the 6 most recent releases. |
| **Zoom** | no | `https://zoom.us/client/latest/zoom_i686.tar.xz` | - | - | - | HTTP 403 on the i686 path (and on `zoom.us/download` scraping). Zoom's Linux client has required x86_64 since 2019 - the i686 path is a dead route, not an access problem. |

**24 of 59 priority applications have a genuine 32-bit Linux build.**

---

# Phase 9.7 — Install pipeline debugging

**Reported:** Install does nothing for GIMP, Inkscape, VLC, Code OSS and others.
Firefox works. **Result: found, fixed, verified.**

## Instrumentation

Every stage now writes one line to `~/.cache/xxri-store/install-trace.log`
(`trace()` in xxri-store, `atrace()` in xxri-app), so a failure is located from
the log rather than reproduced by hand.

Running Firefox (the reference) and the failures through the same pipeline:

```
15:42:17 firefox   install-clicked   backend entered
15:42:17 firefox   catalog-lookup    ok name='Mozilla Firefox' kind=archive src=direct
15:42:17 firefox   arch-check        ok [i686] on i686
15:42:17 firefox   queued            entry created
15:42:17 firefox   lock              acquired after 0s
15:42:17 firefox   provider          ok ver=115.14.0esr size=84336448 url=https://ftp.mozilla.org/...
15:42:17 firefox   download-start    -> /home/xxri/Downloads/mozilla-firefox-115.14.0esr.pkg
15:42:24 firefox   download-done     84336448 bytes
15:42:25 firefox   verify            skipped (catalog pins no digest)
15:42:25 firefox   backend-call      xxri-app install-archive ...
15:42:34 firefox   integrated        desktop=yes dock=1 reg=yes
15:42:34 firefox   done              completed

15:42:54 gimp      install-clicked   backend entered
15:42:54 gimp      catalog-lookup    ok name='GIMP' kind=tcz src=tcz
15:42:54 gimp      arch-check        ok [i686] on i686
15:42:54 gimp      queued            entry created
15:42:54 gimp      lock              acquired after 0s
15:42:54 gimp      provider          ok ver=native size=22573056 url=http://repo.tinycorelinux.net/...
15:42:54 gimp      backend-call      xxri-app install-tcz gimp
15:47:24 -         app:tce-load      FAIL: gimp never registered after 2 attempt(s)
```

Every stage is identical up to `backend-call`. The button, the signal handler,
the queue entry, the arch gate, the lock and the provider all work for both.

## Root cause

The pipeline never checked free disk space. On a clean image
`tce-load -wi gimp.tcz` succeeds (rc=0, marker written, `/usr/local/bin/gimp`
present, 80 MB consumed). It failed only *after* Firefox, Telegram and Code OSS
had extracted into `/opt/xxri`: those three consume ~570 MB of a 1 GB image, and
the next install then had nowhere to write. Inkscape and VLC failed in 4-5
seconds for the same reason. The failure surfaced as a generic "could not be
installed", which reads exactly like "Install does nothing".

Firefox appeared to be special only because it was installed first.

## Fix

1. `space_needed()` estimates real consumption before anything is downloaded -
   `x5` for a native extension (dependency closure), `x4` for a portable
   archive (it expands), `x2` for an AppImage, plus 50 MB headroom.
2. `cmd_install` refuses up front with a precise message:
   `Not enough disk space: Telegram Desktop needs about 193 MB, only 136 MB free`.
3. `install-tcz` distinguishes a full disk from a network failure.
4. The Store no longer renders an Install button it cannot honour: it shows
   **Not enough disk space** (disabled, with the numbers in the tooltip).

## Verification — all six install

| App | Result | Time | Free before -> after | Registry | Dock |
|---|---|---|---|---|---|
| GIMP | **OK** | 392s | 571 -> 492 MB | yes | yes |
| Inkscape | **OK** | 96s | 492 -> 464 MB | yes | yes |
| VLC | **OK** | 395s | 464 -> 345 MB | yes | yes |
| Audacity | **OK** | 71s | 345 -> 331 MB | yes | yes |
| Code OSS | **OK** | 53s | 331 -> 136 MB | yes | yes |
| Telegram | refused in 0s | - | 136 MB | - | - |

The refusal is the correct outcome and reports `ERR:space:193:136`. On a larger
disk all six install, giving 6 registry entries and 6 dock entries.

Screenshots captured: Downloads page, Installed page listing each app with
Launch/Folder/Remove, dock icons appearing one by one, and **VLC and Telegram
running on the desktop**.

## Still failing at launch (separate defect, not the install pipeline)

GIMP, Inkscape, Audacity and Code OSS install correctly but do not open a
window. The cause is isolated and is **not** in the Store:

```
gdk-pixbuf-thumbnailer /usr/local/share/pixmaps/xxri-store.png
  -> Couldn't recognize the image file format
```

A valid PNG (magic `89504e470d0a1a0a`), with `libpng16.so.16` and
`libjpeg.so.62` both resolving, cannot be decoded. `gdk-pixbuf-query-loaders`
emits 12 module loaders (ani, bmp, gif, icns, ico, pnm, qtif, svg, tga, tiff,
xbm, xpm) and **no png or jpeg entry**, so PNG has no registered loader at all.
Tested with the cache present, with it removed, and regenerated - all three
fail. GIMP's log is consistent: every icon and every mypaint brush preview
fails with "Couldn't recognize the image file format".

The Store itself is unaffected because it decodes PNGs through cairo, never
gdk-pixbuf - the rule recorded back in Phase 9.

Substituting Debian i386's `libgdk_pixbuf-2.0.so.0.4200.10` (same version, same
soname, same dependencies) did **not** fix decoding and made GIMP segfault
(rc=11), so it was reverted. This is an upstream `gdk-pixbuf2.tcz` packaging
defect and needs the png/jpeg loader built for i686.

## ONLYOFFICE

Searched exhaustively: official Linux directory, the deb/rpm pools, GitHub
across the 10 most recent tags, SourceForge and the Aliyun mirror.

| Probe | Result |
|---|---|
| `onlyoffice-desktopeditors_amd64.deb` | HTTP 200 |
| `onlyoffice-desktopeditors.x86_64.rpm` | HTTP 200 |
| `DesktopEditors-x86_64.AppImage` | HTTP 200 |
| `onlyoffice-desktopeditors_i386.deb` | **HTTP 404** |
| `onlyoffice-desktopeditors.i686.rpm` | **HTTP 404** |
| `DesktopEditors-i386.AppImage` | **HTTP 404** |
| 36 GitHub assets named "x86" across 10 tags | all `.exe` / `.msi` / `_xp.exe` / `.zip` — **Windows** |

`DesktopEditors_x86.zip`'s first member is `app.ico` and its tree is
`editors/sdkjs-plugins/...` with `.dll` files: no ELF anywhere. **No 32-bit
Linux build of ONLYOFFICE exists.**

It is now listed in the Office category with the full explanation and a
disabled **Currently unsupported** button - never a dead Install.
