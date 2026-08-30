#!/bin/bash
# build.sh - build XXRI File and stage it into rootfs/.
#
# XXRI File is a fork of PCManFM (src/xxri-file/pcmanfm).  It is built
# NATIVELY for the i686 target, not cross-compiled: the x86_64 host kernel
# runs i686 binaries directly, so we assemble a Tiny Core i686 root from the
# image's own core.gz, add Tiny Core's own toolchain and GTK3 headers, and
# build inside it with bubblewrap.  The result links against exactly the
# libraries the device ships.
#
#   ./build.sh            build (reuses the buildroot if present)
#   ./build.sh --clean    discard the buildroot and start over
#
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
BASE=$(cd "$HERE/../.." && pwd)
WORK="$BASE/work/xxri-file"
ROOT="$WORK/buildroot"
SRC="$WORK/src"
ROOTFS="$BASE/rootfs"
LIBFM_VER=1.3.2
MIRROR=http://repo.tinycorelinux.net/16.x/x86/tcz

# 2021-era sources against GCC 14: incompatible-pointer-types and friends
# became hard errors, and -Os keeps the shipped binary small.
CFLAGS_XXRI="-Os -fpermissive -Wno-incompatible-pointer-types -Wno-int-conversion -Wno-implicit-function-declaration -Wno-return-mismatch -Wno-discarded-qualifiers"

[ "${1:-}" = "--clean" ] && rm -rf "$ROOT"
mkdir -p "$WORK" "$SRC"

# --- 1. the native i686 build root -----------------------------------------
if [ ! -x "$ROOT/usr/local/bin/gcc" ]; then
    echo ">> Building i686 buildroot from core.gz"
    [ -f "$BASE/work/iso/boot/core.gz" ] || { echo "need work/iso/boot/core.gz" >&2; exit 1; }
    mkdir -p "$ROOT"
    ( cd "$ROOT" && zcat "$BASE/work/iso/boot/core.gz" | cpio -idm --quiet 2>/dev/null ) || true
    "$HERE/tcz-install.sh" "$ROOT" compiletc gtk3-dev glib2-dev libexif-dev \
        menu-cache-dev intltool pkg-config
fi

# run a command inside the build root
br() {
    bwrap --bind "$ROOT" / --dev /dev --proc /proc --tmpfs /tmp \
      --bind "$SRC" /src --bind "$HERE" /fork --setenv HOME /root \
      --setenv PATH /usr/local/bin:/usr/local/sbin:/bin:/sbin:/usr/bin:/usr/sbin \
      --setenv PKG_CONFIG_PATH /usr/local/lib/pkgconfig:/usr/lib/pkgconfig \
      --setenv LD_LIBRARY_PATH /usr/local/lib:/lib:/usr/lib \
      /bin/busybox sh -c "$*"
}

# --- 2. libfm (upstream, unmodified, built for GTK3) -----------------------
if [ ! -f "$ROOT/usr/local/lib/libfm-gtk3.so" ]; then
    echo ">> Building libfm $LIBFM_VER (GTK3)"
    [ -f "$SRC/libfm-$LIBFM_VER.tar.xz" ] || \
        curl -sL "https://downloads.sourceforge.net/pcmanfm/libfm-$LIBFM_VER.tar.xz" \
             -o "$SRC/libfm-$LIBFM_VER.tar.xz"
    # NEVER "make distclean" a release tarball: it deletes the pre-generated
    # marshallers, and regenerating them needs glib-genmarshal, which is a
    # python script - and Tiny Core's python3 tcz ships no interpreter.
    rm -rf "$SRC/libfm-$LIBFM_VER"
    tar xJf "$SRC/libfm-$LIBFM_VER.tar.xz" -C "$SRC"
    br "cd /src/libfm-$LIBFM_VER && CFLAGS='$CFLAGS_XXRI' ./configure \
          --prefix=/usr/local --sysconfdir=/etc --with-gtk=3 --disable-static \
          --disable-dependency-tracking --disable-gtk-doc --disable-udisks \
          >/tmp/libfm-conf.log 2>&1 && make -j\$(nproc 2>/dev/null || echo 4) \
          >/tmp/libfm-make.log 2>&1 && make install >/tmp/libfm-inst.log 2>&1"
fi

# --- 3. XXRI File ----------------------------------------------------------
echo ">> Building XXRI File"
# `make` must be allowed to fail loudly: staging a stale binary because the
# compile broke is worse than no build at all.
if ! br "cd /fork/pcmanfm && { [ -f Makefile ] || CFLAGS='$CFLAGS_XXRI' ./configure \
      --prefix=/usr/local --sysconfdir=/etc --with-gtk=3 \
      --disable-dependency-tracking >/tmp/xxri-conf.log 2>&1; } && \
      make -j\$(nproc 2>/dev/null || echo 4) >/tmp/xxri-make.log 2>&1"; then
    echo "BUILD FAILED - not staging (see the make log inside the buildroot)" >&2
    exit 1
fi
[ -x "$HERE/pcmanfm/src/pcmanfm" ] || { echo "build failed" >&2; exit 1; }

# --- 4. stage into rootfs --------------------------------------------------
echo ">> Staging the XXRI artwork"
"$HERE/stage-icons.sh"

echo ">> Staging into rootfs"
install -d "$ROOTFS/usr/local/bin" "$ROOTFS/usr/local/lib" \
           "$ROOTFS/usr/local/share/xxri-file"
# the binary is installed under its XXRI name
install -m 755 "$HERE/pcmanfm/src/pcmanfm" "$ROOTFS/usr/local/bin/xxri-file"
"$ROOT/usr/local/bin/strip" "$ROOTFS/usr/local/bin/xxri-file" 2>/dev/null || \
    strip "$ROOTFS/usr/local/bin/xxri-file" 2>/dev/null || true
# libfm runtime (not the headers, not the .la files)
for so in libfm.so.4 libfm-gtk3.so.4 libfm-extra.so.4 libmenu-cache.so.3 libexif.so.12; do
    for cand in "$ROOT/usr/local/lib/$so".*; do
        [ -f "$cand" ] || continue
        install -m 755 "$cand" "$ROOTFS/usr/local/lib/"
        ( cd "$ROOTFS/usr/local/lib" && ln -sf "$(basename "$cand")" "$so" )
    done
done
# libfm's vfs/menu modules and its data
if [ -d "$ROOT/usr/local/lib/libfm/modules" ]; then
    install -d "$ROOTFS/usr/local/lib/libfm/modules"
    install -m 755 "$ROOT"/usr/local/lib/libfm/modules/*.so \
        "$ROOTFS/usr/local/lib/libfm/modules/" 2>/dev/null || true
fi
for d in libfm; do
    [ -d "$ROOT/usr/local/share/$d" ] && cp -a "$ROOT/usr/local/share/$d" \
        "$ROOTFS/usr/local/share/" || true
done
# XXRI File's own data (the .ui files pcmanfm loads at runtime)
install -d "$ROOTFS/usr/local/share/xxri-file/ui"
install -m 644 "$HERE"/pcmanfm/data/ui/*.ui "$ROOTFS/usr/local/share/xxri-file/ui/" 2>/dev/null || true
echo ">> Done: $(ls -l "$ROOTFS/usr/local/bin/xxri-file" | awk '{print $5}') bytes"
