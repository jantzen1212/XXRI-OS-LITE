#!/bin/bash
# build.sh SYSROOT OUT  - cross-build xxri-settings (GTK3) for the i686 target
set -e
SYS="$1"; OUT="${2:-xxri-settings}"; L="$SYS/usr/local"
HERE=$(cd "$(dirname "$0")" && pwd)
CF="-I$L/include/gtk-3.0 -I$L/include/glib-2.0 -I$L/lib/glib-2.0/include -I$L/include/pango-1.0 -I$L/include/cairo -I$L/include/gdk-pixbuf-2.0 -I$L/include/atk-1.0 -I$L/include/harfbuzz -I$L/include/freetype2 -I$L/include"
LF="-L$L/lib -Wl,-rpath-link,$L/lib -lgtk-3 -lgdk-3 -lgobject-2.0 -lglib-2.0 -lgio-2.0 -lpango-1.0 -lpangocairo-1.0 -lcairo -lcairo-gobject -lgdk_pixbuf-2.0"
g++ -m32 -Os -w -fpermissive $CF "$HERE/xxri-settings.c" $LF -mtls-dialect=gnu -static-libgcc -o "$OUT"
strip "$OUT"; ls -l "$OUT"
