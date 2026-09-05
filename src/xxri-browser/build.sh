#!/bin/bash
# build.sh - build XXRI Browser for i686 against the native buildroot.
#
# The engine (Qt WebEngine 5.15.10 / Chromium 87) is built separately and once;
# this only rebuilds the application, which takes about a minute.  Everything
# runs against work/xxri-file/buildroot, the real i686 target root, using
# Chromium's own pinned clang - see work/xxri-browser/buildenv.sh.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../.." && pwd)
BUILD="$ROOT/work/xxri-browser/build/simplebrowser"
B="$ROOT/work/xxri-file/buildroot"

[ -d "$BUILD" ] || { echo "no engine build at $BUILD" >&2; exit 1; }
# shellcheck disable=SC1091
. "$ROOT/work/xxri-browser/buildenv.sh"

cd "$BUILD"
qmake -o Makefile "$HERE/browser/xxri-browser.pro" -spec linux-clang-i686
make -j"$(nproc)"

# Stage into the buildroot only after a successful link, so a failed build can
# never leave a stale binary behind looking like a fresh one.
[ -x "$BUILD/xxri-browser" ] || { echo "build produced no binary" >&2; exit 1; }
install -m755 "$BUILD/xxri-browser" "$B/usr/local/bin/xxri-browser"

# The same binary into the base rootfs, stripped.
#
# This is not redundancy for its own sake.  On the live medium xxri-pkg loads
# extensions with `yes "" | cp -ai`, so a file that is already in the initrd
# rootfs WINS over the extension's copy - leave a stale one there and the live
# ISO silently runs an old browser while the installed system runs the new one.
# Staging both from the same link keeps the two paths honest.
install -m755 "$BUILD/xxri-browser" "$ROOT/rootfs/usr/local/bin/xxri-browser"
strip --strip-unneeded "$ROOT/rootfs/usr/local/bin/xxri-browser" 2>/dev/null || true
echo "xxri-browser -> $B/usr/local/bin/xxri-browser  ($(du -h "$BUILD/xxri-browser" | cut -f1))"
echo "             -> $ROOT/rootfs/usr/local/bin/xxri-browser  (stripped, $(du -h "$ROOT/rootfs/usr/local/bin/xxri-browser" | cut -f1))"
echo "next: ./package.sh   then  ../../build-core.sh && ../../build-writable-image.sh"
