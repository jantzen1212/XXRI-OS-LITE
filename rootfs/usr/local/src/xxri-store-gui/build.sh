#!/bin/bash
# build.sh SYSROOT [OUT]  - cross-build xxri-store-gui (GTK3) for the i686 target.
#
#   SYSROOT = a tree holding the TARGET's own runtime libraries, i.e. every
#             onboot .tcz unsquashed into one directory:
#               for t in work/iso/cde/optional/*.tcz; do unsquashfs -n -f -d SYS $t; done
#
# The target has no -dev packages, so headers come from the HOST's GTK3 while
# the link and the ABI come from the target's own .so files (GTK 3.24.07 vs the
# host's 3.24.x - same stable ABI).  The result is a real i686 binary for the
# device; the identical binary also runs on the host under Xvfb for fast UI
# iteration (see ~/.xxri-testenv/shoot-store.sh).
#
# Keep the GLIBC ceiling <= the target's 2.40:
#   objdump -T xxri-store-gui | grep -o 'GLIBC_[0-9.]*' | sort -uV | tail -1
set -e
SYS="$1"; OUT="${2:-xxri-store-gui}"; L="$SYS/usr/local"
HERE=$(cd "$(dirname "$0")" && pwd)
[ -d "$L/lib" ] || { echo "usage: $0 SYSROOT [OUT]   (SYSROOT/usr/local/lib must hold the target's libgtk-3.so)" >&2; exit 1; }

# headers: the sysroot's own if it happens to carry -dev, else the host's
if [ -d "$L/include/gtk-3.0" ]; then
	CF="-I$L/include/gtk-3.0 -I$L/include/glib-2.0 -I$L/lib/glib-2.0/include -I$L/include/pango-1.0 -I$L/include/cairo -I$L/include/gdk-pixbuf-2.0 -I$L/include/atk-1.0 -I$L/include/harfbuzz -I$L/include/freetype2 -I$L/include"
else
	CF="$(pkg-config --cflags gtk+-3.0)"
fi
LF="-L$L/lib -Wl,-rpath-link,$L/lib -lgtk-3 -lgdk-3 -lgobject-2.0 -lglib-2.0 -lgio-2.0 -lpango-1.0 -lpangocairo-1.0 -lcairo -lcairo-gobject -lgdk_pixbuf-2.0 -lm"

g++ -m32 -Os -w -fpermissive $CF "$HERE/xxri-store-gui.c" $LF -mtls-dialect=gnu -static-libgcc -o "$OUT"
strip "$OUT"
ls -l "$OUT"
objdump -T "$OUT" | grep -o 'GLIBC_[0-9.]*' | sort -uV | tail -1 | sed 's/^/glibc ceiling: /'
