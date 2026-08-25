#!/bin/bash
# build.sh SYSROOT OUT  - cross-build xxri-settings (GTK3) for the i686 target
set -e
SYS="$1"; OUT="${2:-xxri-settings}"; L="$SYS/usr/local"
HERE=$(cd "$(dirname "$0")" && pwd)
# Host GTK3 dev headers, target GTK3 .so - the same recipe the Store and the
# Control Center use.  It is ABI-faithful: the resulting i686 binary runs
# against the device's own 3.24 libraries.  (The sysroot carries runtime
# libraries only; it has no headers to compile against.)
CF="$(pkg-config --cflags gtk+-3.0)"
LF="-L$L/lib -Wl,-rpath-link,$L/lib -lgtk-3 -lgdk-3 -lgobject-2.0 -lglib-2.0 -lgio-2.0 -lpango-1.0 -lpangocairo-1.0 -lcairo -lcairo-gobject -lgdk_pixbuf-2.0"
g++ -m32 -Os -w -fpermissive $CF "$HERE/xxri-settings.c" $LF -mtls-dialect=gnu -static-libgcc -o "$OUT"
strip "$OUT"; ls -l "$OUT"
