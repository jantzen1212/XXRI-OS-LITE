#!/bin/bash
# build.sh SYSROOT [OUT] - cross-build xxri-control-center for the i686 target.
# Same recipe as the Store and Settings: host GTK3 headers, target GTK3 .so.
set -e
SYS="${1:?usage: build.sh SYSROOT [OUT]}"; OUT="${2:-xxri-control-center}"
HERE=$(cd "$(dirname "$0")" && pwd); L="$SYS/usr/local"
LF="-L$L/lib -Wl,-rpath-link,$L/lib -lgtk-3 -lgdk-3 -lgobject-2.0 -lglib-2.0 -lgio-2.0 \
    -lpango-1.0 -lpangocairo-1.0 -lcairo -lcairo-gobject -lgdk_pixbuf-2.0 -lX11 -lm"
gcc -m32 -Os -w $(pkg-config --cflags gtk+-3.0) "$HERE/xxri-control-center.c" $LF \
    -mtls-dialect=gnu -static-libgcc -o "$OUT"
strip "$OUT"; ls -l "$OUT"
objdump -T "$OUT" | grep -o 'GLIBC_[0-9.]*' | sort -Vu | tail -1 | sed 's/^/glibc ceiling: /'
