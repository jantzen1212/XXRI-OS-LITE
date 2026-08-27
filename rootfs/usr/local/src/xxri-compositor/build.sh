#!/bin/bash
# build.sh SYSROOT [OUT] - cross-build the XXRI compositor for the i686 target.
set -e
SYS="${1:?usage: build.sh SYSROOT [OUT]}"; OUT="${2:-xxri-compositor}"
HERE=$(cd "$(dirname "$0")" && pwd); L="$SYS/usr/local"
gcc -m32 -Os -Wall -Wextra -std=gnu99 "$HERE/xxri-compositor.c" \
    -I"$L/include" -L"$L/lib" -Wl,-rpath-link,"$L/lib" \
    -lXcomposite -lXdamage -lXfixes -lXrender -lXext -lX11 -o "$OUT"
strip "$OUT"; ls -l "$OUT"
objdump -T "$OUT" | grep -o 'GLIBC_[0-9.]*' | sort -Vu | tail -1 | sed 's/^/glibc ceiling: /'
