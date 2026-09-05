#!/bin/bash
# package.sh - build xxri-browser.tcz from the native i686 buildroot.
#
# The browser is the only Qt application in XXRI, and Qt WebEngine carries a
# whole Chromium with it, so it ships as a squashfs extension rather than
# inside core.gz: an extension is mounted from the image, while core.gz is
# unpacked into RAM and 190MB of engine would not fit there.
#
# Only what the browser actually loads is staged.  Everything is stripped, and
# the 105 locale catalogues Chromium ships are reduced to the one the system
# runs in - that alone is 70MB.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../.." && pwd)
B="$ROOT/work/xxri-file/buildroot"
OUT="$ROOT/work/iso/cde/optional"
STAGE=$(mktemp -d)
trap 'rm -rf "$STAGE"' EXIT

L="$STAGE/usr/local/lib"
# The locale catalogue goes under the Qt *translations* directory: that is
# where QLibraryInfo::TranslationsPath points and the only place the engine
# looks.  Staged next to resources/ instead, it was never found - the booted
# browser logged "locales directory not found ... Translations MAY NOT be
# correct" and every Chromium-supplied string (context menus, error pages,
# form controls) fell back to nothing.
mkdir -p "$L/qt5/plugins" "$STAGE/usr/local/bin" "$STAGE/usr/local/libexec" \
         "$STAGE/usr/local/share/qt5/resources" \
         "$STAGE/usr/local/share/qt5/translations/qtwebengine_locales"

say() { printf '   %-46s %s\n' "$1" "$2"; }

echo ">> 1 executables"
install -m755 "$B/usr/local/bin/xxri-browser"        "$STAGE/usr/local/bin/"
install -m755 "$B/usr/local/libexec/QtWebEngineProcess" "$STAGE/usr/local/libexec/"

echo ">> 2 Qt libraries"
# Exactly the modules the browser and the engine name in DT_NEEDED.
QTLIBS="Core Gui Widgets Network Qml Quick QmlModels WebChannel Positioning
        PrintSupport DBus XcbQpa WebEngineCore WebEngineWidgets Svg QuickWidgets
        X11Extras"
for q in $QTLIBS; do
    src=$(ls "$B/usr/local/lib/libQt5$q.so."*.*.* 2>/dev/null | head -1) || true
    [ -n "$src" ] || { echo "   !! missing libQt5$q" >&2; continue; }
    cp -a "$src" "$L/"
    base=$(basename "$src")
    ( cd "$L" && ln -sf "$base" "libQt5$q.so.5" )
done

echo ">> 3 third-party libraries the image does not already carry"
for l in libicuuc.so.70 libicui18n.so.70 libicudata.so.70 \
         libdouble-conversion.so.3 libopus.so.0 \
         libssl.so.1.1 libcrypto.so.1.1; do
    src=$(ls "$B/usr/local/lib/$l"* 2>/dev/null | grep -v '\.so$' | \
          xargs -r -n1 readlink -f 2>/dev/null | sort -u | head -1)
    [ -n "$src" ] || { echo "   !! missing $l" >&2; continue; }
    cp -a "$src" "$L/"
    [ "$(basename "$src")" = "$l" ] || ( cd "$L" && ln -sf "$(basename "$src")" "$l" )
done

echo ">> 4 Qt plugins (only the ones a windowed browser loads)"
for p in platforms imageformats iconengines platformthemes xcbglintegrations \
         platforminputcontexts; do
    [ -d "$B/usr/local/lib/qt5/plugins/$p" ] || continue
    cp -a "$B/usr/local/lib/qt5/plugins/$p" "$L/qt5/plugins/"
done
# The browser runs on xcb and draws PNG/SVG; the rest of Qt's plugin set only
# pulls in libraries the image does not carry (webp, mng, jasper, websockets,
# EGLFS) for formats and platforms XXRI never uses.
for drop in platforms/libqminimal.so platforms/libqoffscreen.so \
            platforms/libqvnc.so platforms/libqlinuxfb.so \
            platforms/libqwebgl.so platforms/libqeglfs.so \
            imageformats/libqwebp.so imageformats/libqmng.so \
            imageformats/libqjp2.so imageformats/libqtiff.so; do
    rm -f "$L/qt5/plugins/$drop"
done

echo ">> 5 engine resources"
cp -a "$B/usr/local/share/qt5/resources/." "$STAGE/usr/local/share/qt5/resources/"
# DevTools is a 2.2MB debugging front-end; the shipped browser does not open it.
rm -f "$STAGE/usr/local/share/qt5/resources/qtwebengine_devtools_resources.pak"
# One locale, not 105.
src_locale=$(ls "$B/usr/local/share/qt5/translations/qtwebengine_locales/en-US.pak" \
                "$B/usr/local/share/qt5/qtwebengine_locales/en-US.pak" 2>/dev/null | head -1)
[ -n "$src_locale" ] || { echo "   !! en-US.pak not found in the buildroot" >&2; exit 1; }
cp -a "$src_locale" "$STAGE/usr/local/share/qt5/translations/qtwebengine_locales/"

echo ">> 6 desktop integration"
mkdir -p "$STAGE/usr/local/share/applications" "$STAGE/usr/local/share/pixmaps"
cat > "$STAGE/usr/local/share/applications/xxri-browser.desktop" <<'DESK'
[Desktop Entry]
Type=Application
Name=XXRI Browser
GenericName=Web Browser
Comment=Browse the web
Exec=/usr/local/bin/xxri-browser %U
Icon=xxri-browser
X-FullPathIcon=/usr/local/share/pixmaps/xxri-browser.png
Terminal=false
Categories=Network;WebBrowser;
MimeType=text/html;x-scheme-handler/http;x-scheme-handler/https;
StartupNotify=false
DESK
magick "$ROOT/assets/icons/xxri-icons/xxri-browser.svg" -background none \
       -resize 64x64 -gravity center -extent 64x64 \
       "$STAGE/usr/local/share/pixmaps/xxri-browser.png"

echo ">> 7 strip"
before=$(du -sm "$STAGE" | cut -f1)
find "$STAGE" -type f \( -name '*.so*' -o -perm -u+x \) -print0 |
    xargs -0 -r -n1 strip --strip-unneeded 2>/dev/null || true
after=$(du -sm "$STAGE" | cut -f1)
say "stripped" "${before}M -> ${after}M"

echo ">> 7b closure check"
# Every DT_NEEDED of everything staged must resolve either inside this package
# or in the image the package will be mounted into.  Shipping an extension that
# is one library short costs a whole boot to discover, so it is checked here.
IMGTREE="${XXRI_IMGTREE:-}"
if [ -z "$IMGTREE" ]; then
    IMGTREE=$(mktemp -d); MADE_TREE=1
    while read -r ext; do
        [ -n "$ext" ] || continue
        [ "$ext" = "xxri-browser.tcz" ] && continue
        [ -f "$ROOT/work/iso/cde/optional/$ext" ] || continue
        unsquashfs -n -f -d "$IMGTREE" "$ROOT/work/iso/cde/optional/$ext" \
            >/dev/null 2>&1 || true
    done < "$ROOT/work/iso/cde/onboot.lst"
    cp -a "$ROOT/rootfs/lib" "$ROOT/rootfs/usr" "$IMGTREE/" 2>/dev/null || true
fi
missing=0
for f in $(find "$STAGE" -type f \( -name '*.so*' -o -perm -u+x \)); do
    for n in $(readelf -d "$f" 2>/dev/null | sed -n 's/.*NEEDED.*\[\(.*\)\]/\1/p'); do
        case "$n" in ld-linux*|linux-gate*) continue;; esac
        find "$STAGE" -name "$n" | grep -q . && continue
        find "$IMGTREE" -name "$n" | grep -q . && continue
        echo "   !! $(basename "$f") needs $n - NOT in this package or the image"
        missing=$((missing+1))
    done
done
[ -n "${MADE_TREE:-}" ] && rm -rf "$IMGTREE"
if [ "$missing" -gt 0 ]; then
    echo "   $missing unresolved dependencies - refusing to build a package that cannot run" >&2
    exit 1
fi
say "closure" "every DT_NEEDED resolves"

echo ">> 8 squashfs"
mkdir -p "$OUT"
rm -f "$OUT/xxri-browser.tcz"
# $STAGE comes from mktemp -d, which is mode 0700: without normalizing that,
# mksquashfs bakes the restrictive owner-only root inode into the image, and
# every extension loads as the unprivileged user, not root, so it cannot see
# past that root and the loader treats the mount as empty (bootlooks like the
# extension fails to load, when it silently loads unreadable instead).
chmod -R a+rX "$STAGE"
mksquashfs "$STAGE" "$OUT/xxri-browser.tcz" -b 16384 -no-xattrs -noappend \
    -all-root -no-progress >/dev/null
md5sum "$OUT/xxri-browser.tcz" | sed "s#$OUT/##" > "$OUT/xxri-browser.tcz.md5.txt"

# Extensions this one needs that the image already carries.  Listing them keeps
# the load order honest even though they are all on onboot.lst already.
cat > "$OUT/xxri-browser.tcz.dep" <<'DEP'
nss.tcz
alsa.tcz
dbus.tcz
libxkbcommon.tcz
fontconfig.tcz
harfbuzz.tcz
DEP

say "package" "$(du -h "$OUT/xxri-browser.tcz" | cut -f1)"
