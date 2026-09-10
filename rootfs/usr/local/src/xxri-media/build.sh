#!/bin/bash
# build.sh SYSROOT OUT  - cross-build xxri-media (GTK3) for the i686 target.
# The same recipe Settings, the Store and the Control Center use: host GTK3
# headers against the target's own .so files, which is ABI-faithful because
# both are GTK 3.24.
set -e
SYS="$1"; OUT="${2:-xxri-media}"; L="$SYS/usr/local"
HERE=$(cd "$(dirname "$0")" && pwd)
CF="$(pkg-config --cflags gtk+-3.0)"
LF="-L$L/lib -Wl,-rpath-link,$L/lib -lgtk-3 -lgdk-3 -lgobject-2.0 -lglib-2.0 \
    -lgio-2.0 -lpango-1.0 -lpangocairo-1.0 -lcairo -lcairo-gobject \
    -lgdk_pixbuf-2.0 -lX11 -lm"
gcc -m32 -Os -w $CF "$HERE/xxri-media.c" "$HERE/player.c" $LF \
    -mtls-dialect=gnu -static-libgcc -o "$OUT"
strip "$OUT"; ls -l "$OUT"
