#!/bin/bash
# build-caches.sh - generate every desktop runtime cache at BUILD time.
#
# XXRI ships as a release image: a freshly installed system must already have a
# fully initialised desktop runtime, with nothing left for a first-boot script
# to "finish configuring".
#
# The caches cannot be produced by the host's own tools (they would describe the
# host's paths and ABI), so this assembles the exact /usr/local the image will
# have - every onboot extension unsquashed into one tree - and runs the TARGET's
# own i686 binaries against it inside bwrap, with that tree bound at /usr/local
# so every path recorded in the caches is the path the device will see.
#
# The results land in rootfs-overrides/, which build-writable-image.sh copies
# over the stage AFTER the extensions (step 2b), so they cannot be clobbered.
#
# Why this matters: gdk-pixbuf 2.42 does NOT fall back to its built-in loaders
# when loaders.cache is absent - the loader table stays empty and EVERY image
# format, even XPM, fails with "Couldn't recognize the image file format".
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
CDE="$HERE/work/iso/cde"
OVR="$HERE/rootfs-overrides"
TREE=$(mktemp -d)
trap 'rm -rf "$TREE"' EXIT

echo ">> 1 assemble the image's /usr/local from the onboot extensions"
n=0
while read -r ext; do
    [ -n "$ext" ] || continue
    [ -f "$CDE/optional/$ext" ] || continue
    unsquashfs -n -f -d "$TREE" "$CDE/optional/$ext" >/dev/null 2>&1 || true
    n=$((n+1))
done < "$CDE/onboot.lst"
# the base rootfs contributes files too (its own schemas, icons, desktop files)
cp -a "$HERE/rootfs/usr/local/." "$TREE/usr/local/" 2>/dev/null || true
echo "   unsquashed $n extensions -> $(du -sh "$TREE" | cut -f1)"

L="$TREE/usr/local"
[ -d "$L/lib" ] || { echo "!! no $L/lib - nothing to do" >&2; exit 1; }

# Run a target binary with the assembled tree bound at /usr/local, exactly
# where the device will have it.
tgt() {
    bwrap --ro-bind / / --dev /dev --proc /proc --bind "$TREE" "$TREE" \
          --bind "$L" /usr/local \
          --setenv LD_LIBRARY_PATH /usr/local/lib \
          --setenv PATH /usr/local/bin:/usr/local/sbin:/usr/bin:/bin \
          --setenv HOME /tmp --setenv XDG_DATA_DIRS /usr/local/share:/usr/share \
          "$@" 2>&1
}
have() { [ -x "$L/bin/$1" ] || [ -x "$L/sbin/$1" ]; }

echo ">> 2 gdk-pixbuf loaders.cache"
PB=lib/gdk-pixbuf-2.0/2.10.0
if have gdk-pixbuf-query-loaders && [ -d "$L/$PB/loaders" ]; then
    tgt sh -c "gdk-pixbuf-query-loaders > /usr/local/$PB/loaders.cache" >/dev/null || true
    c="$L/$PB/loaders.cache"
    echo "   $(grep -cE '^"[a-z0-9]+" [0-9]+ "gdk-pixbuf"' "$c" 2>/dev/null || echo 0) formats, $(wc -c < "$c" 2>/dev/null || echo 0) bytes"
else echo "   (query-loaders or loaders dir missing - skipped)"; fi

echo ">> 3 glib schema cache"
if have glib-compile-schemas && [ -d "$L/share/glib-2.0/schemas" ]; then
    tgt glib-compile-schemas /usr/local/share/glib-2.0/schemas >/dev/null || true
    echo "   $(ls -l "$L/share/glib-2.0/schemas/gschemas.compiled" 2>/dev/null | awk '{print $5}' || echo 0) bytes"
else echo "   (no schemas dir - skipped)"; fi

echo ">> 4 shared-mime-info database"
if have update-mime-database && [ -d "$L/share/mime" ]; then
    tgt update-mime-database /usr/local/share/mime >/dev/null || true
    echo "   mime.cache $(wc -c < "$L/share/mime/mime.cache" 2>/dev/null || echo 0) bytes"
else echo "   (update-mime-database or share/mime missing - skipped)"; fi

echo ">> 5 icon theme caches"
if have gtk-update-icon-cache; then
    for th in "$L"/share/icons/*/; do
        [ -f "$th/index.theme" ] || continue
        tgt gtk-update-icon-cache -q -f -t "/usr/local/share/icons/$(basename "$th")" >/dev/null || true
        echo "   $(basename "$th"): $(wc -c < "$th/icon-theme.cache" 2>/dev/null || echo 0) bytes"
    done
else echo "   (gtk-update-icon-cache missing - skipped)"; fi

echo ">> 6 desktop database"
if have update-desktop-database && [ -d "$L/share/applications" ]; then
    tgt update-desktop-database /usr/local/share/applications >/dev/null || true
    echo "   mimeinfo.cache $(wc -c < "$L/share/applications/mimeinfo.cache" 2>/dev/null || echo 0) bytes"
else echo "   (update-desktop-database missing - skipped)"; fi

echo ">> 7 gio modules cache"
if have gio-querymodules && [ -d "$L/lib/gio/modules" ]; then
    tgt gio-querymodules /usr/local/lib/gio/modules >/dev/null || true
    echo "   giomodule.cache $(wc -c < "$L/lib/gio/modules/giomodule.cache" 2>/dev/null || echo 0) bytes"
else echo "   (gio-querymodules missing - skipped)"; fi

echo ">> 8 font cache"
if have fc-cache && [ -d "$L/share/fonts" ]; then
    tgt fc-cache -f >/dev/null || true
    echo "   $(find "$L/share/fonts" -name '*.cache-*' 2>/dev/null | wc -l) cache file(s)"
else echo "   (fc-cache missing - skipped)"; fi

echo ">> 9 copy the generated caches into rootfs-overrides/"
copy_out() { # relative path under /usr/local
    src="$L/$1"; [ -e "$src" ] || return 0
    dst="$OVR/usr/local/$1"
    mkdir -p "$(dirname "$dst")"
    cp -a "$src" "$dst"
    echo "   + usr/local/$1"
}
copy_out "$PB/loaders.cache"
copy_out share/glib-2.0/schemas/gschemas.compiled
copy_out share/mime/mime.cache
copy_out share/applications/mimeinfo.cache
copy_out lib/gio/modules/giomodule.cache
for th in "$L"/share/icons/*/; do
    [ -f "$th/icon-theme.cache" ] && copy_out "share/icons/$(basename "$th")/icon-theme.cache"
done
for f in "$L"/share/fonts/*.cache-*; do
    [ -e "$f" ] && copy_out "share/fonts/$(basename "$f")"
done
# the mime database is a directory of generated files, not just mime.cache
for m in aliases generic-icons globs globs2 icons magic mime.cache subclasses treemagic types version XMLnamespaces; do
    copy_out "share/mime/$m"
done

echo ">> Done: $(find "$OVR" -type f | wc -l) files staged in rootfs-overrides/"
