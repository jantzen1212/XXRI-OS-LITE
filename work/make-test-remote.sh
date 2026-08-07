#!/bin/bash
# make-test-remote.sh - build a LOCAL test repository for Phase 9 QA.
#
# This is not shipped: it exists so the remote -> cache -> bundled priority and
# the update flow can be exercised for real (HTTP download + SHA256 verify +
# xxri-app integrate) from a QEMU guest, which reaches the host at 10.0.2.2.
#
#   ./make-test-remote.sh              build work/test-remote/store
#   python3 -m http.server 8099 -d work/test-remote      serve it
#   guest:  REMOTE_BASE=http://10.0.2.2:8099/store xxri-store refresh
set -e
BUILD=/home/jantzen/xxri-build
SRC="$BUILD/rootfs/usr/local/share/xxri-store"
OUT="$BUILD/work/test-remote/store"
APPIMG="$1"                                   # path to the v1.1.0 test AppImage
[ -f "$APPIMG" ] || { echo "usage: $0 /path/to/xxri-test-1.1.0.AppImage" >&2; exit 1; }

rm -rf "$OUT"; mkdir -p "$OUT/repository" "$OUT/apps"
cp "$APPIMG" "$OUT/apps/xxri-test-1.1.0.AppImage"
SHA=$(sha256sum "$OUT/apps/xxri-test-1.1.0.AppImage" | cut -d' ' -f1)
SIZE=$(wc -c < "$OUT/apps/xxri-test-1.1.0.AppImage")
URL="http://10.0.2.2:8099/store/apps/xxri-test-1.1.0.AppImage"

cp "$SRC/repository"/*.json "$OUT/repository/"
mv "$OUT/repository/sections.json" "$OUT/sections.json"
mv "$OUT/repository/meta.json" "$OUT/meta.json"

# the remote publishes xxri-test 1.1.0: new version, real URL, pinned checksum
awk -F'\t' -v OFS='\t' -v u="$URL" -v s="$SHA" -v z="$SIZE" '
  $1=="xxri-test" { $5="1.1.0"; $6=z; $7=s; $8="direct"; $9=""; $10=u; $12="2026-07-16";
                    $13="i686," u "," s "," z }
  { print }
' "$SRC/repository/index.tsv" > "$OUT/index.tsv"

python3 - "$OUT" "$URL" "$SHA" "$SIZE" <<'PY'
import json, sys
out, url, sha, size = sys.argv[1], sys.argv[2], sys.argv[3], int(sys.argv[4])
p = out + "/repository/utilities.json"
d = json.load(open(p))
for a in d["apps"]:
    if a["id"] != "xxri-test": continue
    a.update(version="1.1.0", release_date="2026-07-16", updated="2026-07-16",
             size=size, download_size=size, sha256=sha, source_type="direct",
             source_ref="", url=url,
             assets={"i686": {"url": url, "sha256": sha, "size": size}},
             changelog=("1.1.0 - published on the remote repository. Proves the Store's "
                        "remote catalog, update detection and HTTP install path.\n"
                        "1.0.0 - first release shipped with XXRI OS Lite."))
json.dump(d, open(p, "w"), separators=(",", ":"))
m = json.load(open(out + "/meta.json"))
m["source"] = "XXRI test remote repository"
json.dump(m, open(out + "/meta.json", "w"), separators=(",", ":"))
print("remote: xxri-test 1.1.0  sha256=%s  %d bytes" % (sha[:16] + "...", size))
PY
echo "served layout:"; find "$OUT" -maxdepth 2 | head -8
