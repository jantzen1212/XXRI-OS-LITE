#!/bin/bash
# build.sh SYSROOT [OUT] - cross-build the XXRI window manager for the i686 target.
#
# flwm's own sources (from git.tinycorelinux.net/flwm, the same tree Tiny Core
# builds flwm.tcz from) with the XXRI decoration patch compiled in:
#   -DTOPSIDE  the titlebar sits on top of the window, not rotated on its left
#   -DXXRI     XXRI titlebar geometry, palette, controls and title font
# Headers come from TC's fltk-1.3-dev.tcz; the link is against the TARGET's own
# libfltk 1.3 / libX11, exactly like xxri-installer and xxri-settings.
# Install to BOTH trees when you rebuild:
#   rootfs/usr/local/bin/flwm            - the live ISO (extensions load with
#                                          `cp -ai`, so a file already in the
#                                          initrd wins over flwm.tcz)
#   rootfs-overrides/usr/local/bin/flwm  - the installed image (step 2 unsquashes
#                                          flwm.tcz OVER rootfs, step 2b restores
#                                          the override afterwards)
# Shipping it in only one of them leaves the other session with Tiny Core's
# rotated left-side titlebar.
set -e
SYS="${1:?usage: build.sh SYSROOT [OUT]}"; OUT="${2:-flwm-xxri}"
HERE=$(cd "$(dirname "$0")" && pwd)
INC="${FLTK_INC:-$HOME/.xxri-testenv/fltkdev/usr/local/include}"
L="$SYS/usr/local"
[ -d "$INC/FL" ] || { echo "no FLTK headers at $INC/FL" >&2; exit 1; }
[ -e "$L/lib/libfltk.so.1.3" ] || { echo "no target libfltk in $L/lib" >&2; exit 1; }

g++ -m32 -Os -w -fno-exceptions -fno-rtti -DTOPSIDE -DXXRI \
    -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 \
    -D_THREAD_SAFE -D_REENTRANT -I"$INC" -I"$HERE" \
    "$HERE"/*.C \
    -L"$L/lib" -Wl,-rpath-link,"$L/lib" \
    -lfltk -lXcursor -lXext -lX11 -lm \
    -mtls-dialect=gnu -lstdc++ -static-libgcc \
    -o "$OUT"
strip "$OUT"
ls -l "$OUT"
objdump -T "$OUT" | grep -o 'GLIBC_[0-9.]*' | sort -Vu | tail -1 | sed 's/^/glibc ceiling: /'
