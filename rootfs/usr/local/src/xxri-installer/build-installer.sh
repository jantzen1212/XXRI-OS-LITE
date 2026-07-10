#!/bin/bash
# build-installer.sh - cross-build xxri-installer for the 32-bit target.
#
# Links against the SAME libfltk.so.1.3 / libX11 the OS ships (extracted
# from work/iso/cde/optional), with libstdc++/libgcc linked statically so
# the binary only depends on the target's glibc (2.40) and its own libs.
# Headers come from TC's fltk-1.3-dev.tcz (same repo generation).
#
# Usage: build-installer.sh <fltk-include-dir> <tc-libs-dir> [outfile]
set -e
INC="$1"; LIBS="$2"; OUT="${3:-xxri-installer}"
HERE=$(cd "$(dirname "$0")" && pwd)

g++ -m32 -Os -fno-rtti -Wall \
    -I"$INC" \
    "$HERE/xxri-installer.cxx" \
    -L"$LIBS" -Wl,-rpath-link,"$LIBS" \
    -lfltk_images -lfltk -lX11 -lm \
    -mtls-dialect=gnu -lstdc++ -static-libgcc \
    -o "$OUT"
strip "$OUT"

echo "== symbol-version ceiling (target glibc is 2.40) =="
objdump -T "$OUT" | grep -o 'GLIBC_[0-9.]*' | sort -Vu | tail -3
objdump -T "$OUT" | grep -o 'GLIBCXX_[0-9.]*' | sort -Vu | tail -1 || echo "(no GLIBCXX deps - static)"
ls -l "$OUT"
