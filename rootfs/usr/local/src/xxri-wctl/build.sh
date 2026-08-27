#!/bin/bash
# build.sh SYSROOT [OUT] - cross-build xxri-wctl for the i686 target, against
# the target's own libX11 (same pattern as the installer/Settings/flwm builds).
set -e
SYS="${1:?usage: build.sh SYSROOT [OUT]}"; OUT="${2:-xxri-wctl}"
HERE=$(cd "$(dirname "$0")" && pwd)
L="$SYS/usr/local"
gcc -m32 -Os -Wall -Wextra -std=gnu99 \
    "$HERE/xxri-wctl.c" \
    -I"$L/include" -L"$L/lib" -Wl,-rpath-link,"$L/lib" \
    -lX11 -o "$OUT"
strip "$OUT"
ls -l "$OUT"
objdump -T "$OUT" | grep -o 'GLIBC_[0-9.]*' | sort -Vu | tail -1 | sed 's/^/glibc ceiling: /'
