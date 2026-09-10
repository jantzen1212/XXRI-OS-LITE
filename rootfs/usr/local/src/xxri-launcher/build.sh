#!/bin/bash
# build.sh SYSROOT [OUT] - cross-build xxri-launcher (GTK3) for the i686 target.
# The same recipe Settings, the Store, the Control Center and Media use: host
# GTK3 headers against the target's own .so files, which is ABI-faithful
# because both sides are GTK 3.24.
set -e
SYS="${1:?usage: build.sh SYSROOT [OUT]}"; OUT="${2:-xxri-launcher}"
HERE=$(cd "$(dirname "$0")" && pwd); L="$SYS/usr/local"
CF="$(pkg-config --cflags gtk+-3.0)"
LF="-L$L/lib -Wl,-rpath-link,$L/lib -lgtk-3 -lgdk-3 -lgobject-2.0 -lglib-2.0 \
    -lgio-2.0 -lpango-1.0 -lpangocairo-1.0 -lcairo -lcairo-gobject \
    -lgdk_pixbuf-2.0 -lX11 -lm"
# -std=gnu17: without an explicit standard, a host GCC new enough to default
# to C23 silently redirects plain sscanf() to __isoc23_sscanf, a GLIBC_2.38
# symbol the target's much older libc does not have - the binary would link
# fine here and then fail to even start on the real system.  gnu17 keeps the
# classic sscanf, which has existed since glibc's very first release.
gcc -m32 -std=gnu17 -Os -w $CF "$HERE/xxri-launcher.c" $LF \
    -mtls-dialect=gnu -static-libgcc -o "$OUT"
strip "$OUT"; ls -l "$OUT"
objdump -T "$OUT" | grep -o 'GLIBC_[0-9.]*' | sort -Vu | tail -1 | sed 's/^/glibc ceiling: /'
