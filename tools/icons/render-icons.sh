#!/bin/sh
# render-icons.sh - regenerate XXRI's runtime icon rasters from the SVG masters.
#
# The masters are assets/icons/xxri-icons/*.svg.  Rasters exist only where the
# runtime cannot use an SVG: the dock (wbar/imlib2), the Settings sidebar and
# the xxri icon theme's 64x64 size (the device's gdk-pixbuf has no working SVG
# loader, so every XXRI app loads PNG through cairo).  The scalable icon theme
# ships the masters themselves.
#
# Rendering is `rsvg-convert -w N -h N` - that reproduces every existing raster
# pixel for pixel, so re-running this changes nothing but the icons whose
# master changed.  A raster is rewritten only when its pixels differ, so an
# unchanged icon keeps its exact bytes (no churn in the image checksums).
#
# Needs rsvg-convert and ImageMagick (magick) on the build host; the outputs
# are committed, the PTXdist build itself does not run this.
#
#   tools/icons/render-icons.sh            # from the repository root
set -e
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
M="$ROOT/assets/icons/xxri-icons"
PR="$ROOT/ptxdist/projectroot/usr/local/share"
SI="$ROOT/src/xxri-settings/root/usr/local/share/xxri-settings/icons"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
changed=0

render() { # master size out
	[ -f "$M/$1.svg" ] || { echo "MISSING MASTER: $1.svg" >&2; exit 1; }
	rsvg-convert -w "$2" -h "$2" "$M/$1.svg" -o "$TMP/r.png"
	if [ -f "$3" ] && [ "$(magick compare -metric AE "$TMP/r.png" "$3" null: 2>&1 | cut -d' ' -f1)" = 0 ]; then
		return 0
	fi
	mkdir -p "$(dirname "$3")"
	magick "$TMP/r.png" -strip "$3"
	echo "rendered ${3#$ROOT/}"
	changed=$((changed + 1))
}

# dock / .desktop icons (64x64, /usr/local/share/pixmaps)
while read -r px master; do
	render "$master" 64 "$PR/pixmaps/$px.png"
done <<EOF
apps xxri-store
aterm terminal
editor notes
exittc power
flrun all-apps
gear general
mnttool storage
xxri-app-generic all-apps
xxri-browser xxri-browser
xxri-installer storage
xxri-settings settings
xxri-store xxri-store
EOF
# (core.png is the logo; xxri-file, xxri-launcher and xxri-media have their
#  own artwork, not a master from this set)

# Settings sidebar: <id>-22.png and <id>-40.png
while read -r id master; do
	render "$master" 22 "$SI/$id-22.png"
	render "$master" 40 "$SI/$id-40.png"
done <<EOF
about about-devices
apps all-apps
battery battery
bluetooth bluetooth
camera camera
devices connected-devices
display display
files files
general general
settings settings
sound music
storage storage
terminal terminal
update downloads
wallpaper photos
wifi wifi-network
EOF
# (help-22/40 are the XXRI logo, used by the sidebar's user chip)

# the xxri icon theme: every master, scalable + 64x64
for f in "$PR"/icons/xxri/scalable/apps/*.svg; do
	n=$(basename "$f" .svg)
	cmp -s "$M/$n.svg" "$f" || { cp "$M/$n.svg" "$f"; echo "copied ${f#$ROOT/}"; changed=$((changed + 1)); }
	render "$n" 64 "$PR/icons/xxri/64x64/apps/$n.png"
done
echo "$changed file(s) updated"
