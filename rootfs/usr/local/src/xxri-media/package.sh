#!/bin/bash
# package.sh - build xxri-media and stage it into the image tree.
#
# The player itself is 39KB and links only against libraries the image already
# carries, so unlike the Browser it needs no extension of its own: the binary
# goes into the base rootfs and the playback engine comes from the MPlayer
# extensions on onboot.lst.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../../../../.." && pwd)
SYS="$ROOT/work/xxri-file/buildroot"

"$HERE/build.sh" "$SYS" "$HERE/xxri-media"
install -m755 "$HERE/xxri-media" "$ROOT/rootfs/usr/local/bin/xxri-media"

# Everything the player needs at runtime, checked here rather than discovered
# on a booted image.
missing=0
for n in $(readelf -d "$HERE/xxri-media" | sed -n 's/.*NEEDED.*\[\(.*\)\]/\1/p'); do
    case "$n" in ld-linux*|linux-gate*) continue;; esac
    find "$ROOT/rootfs" "$SYS/usr/local/lib" -name "$n" 2>/dev/null | grep -q . && continue
    found=0
    for t in "$ROOT"/work/iso/cde/optional/*.tcz; do
        unsquashfs -l "$t" 2>/dev/null | grep -q "/$n\$" && { found=1; break; }
    done
    [ $found = 1 ] || { echo "   !! $n is in neither the rootfs nor any extension"; missing=$((missing+1)); }
done
[ "$missing" = 0 ] || { echo "   $missing unresolved libraries" >&2; exit 1; }

echo "xxri-media -> $ROOT/rootfs/usr/local/bin/xxri-media ($(du -h "$HERE/xxri-media" | cut -f1))"
echo "engine: mplayer, from mplayer-cli.tcz on onboot.lst"
