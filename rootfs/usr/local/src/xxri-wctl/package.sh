#!/bin/bash
# package.sh - build xxri-wctl and stage it into the image tree.
#
# This exists because build.sh alone only produces a binary wherever you point
# it: editing xxri-wctl.c, building it to /tmp to check the warnings, and then
# rebuilding the image ships the OLD binary, silently, with no error anywhere.
# Use this instead, the same way Media/the launcher are packaged.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../../../../.." && pwd)
SYS="$ROOT/work/xxri-file/buildroot"

"$HERE/build.sh" "$SYS" "$HERE/xxri-wctl"
install -m755 "$HERE/xxri-wctl" "$ROOT/rootfs/usr/local/bin/xxri-wctl"

missing=0
for n in $(readelf -d "$HERE/xxri-wctl" | sed -n 's/.*NEEDED.*\[\(.*\)\]/\1/p'); do
    case "$n" in ld-linux*|linux-gate*) continue;; esac
    find "$ROOT/rootfs" "$SYS/usr/local/lib" -name "$n" 2>/dev/null | grep -q . && continue
    found=0
    for t in "$ROOT"/work/iso/cde/optional/*.tcz; do
        unsquashfs -l "$t" 2>/dev/null | grep -q "/$n\$" && { found=1; break; }
    done
    [ $found = 1 ] || { echo "   !! $n is in neither the rootfs nor any extension"; missing=$((missing+1)); }
done
[ "$missing" = 0 ] || { echo "   $missing unresolved libraries" >&2; exit 1; }

echo "xxri-wctl -> $ROOT/rootfs/usr/local/bin/xxri-wctl ($(du -h "$HERE/xxri-wctl" | cut -f1))"
echo "commands: list | running KEY | activate KEY | indicators (dots + dock right-click menu)"
