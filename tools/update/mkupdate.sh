#!/bin/sh
# mkupdate.sh - build an XXRI OS Lite system update: the payload and the
# manifest.json that announces it (docs/software-update.md).
#
#   tools/update/mkupdate.sh -v VERSION -t TREE -u URL_BASE -o OUTDIR \
#       [-T TITLE] [-n NOTES_FILE] [-m MIN_VERSION] [-r reboot|session|none] [-b BUILD]
#
# TREE holds what the release changes, in the payload's own layout:
#   TREE/files/...    normal files, as they sit under /  (usr/local/bin/xxri-foo)
#   TREE/boot/...     vmlinuz, core.gz, intel-ucode.img, amd-ucode.img
#   TREE/remove.list  absolute paths to delete, one per line (optional)
# A kernel update must bring its modules as files/lib/modules/<release>/...:
# the installed system runs from its disk, not from core.gz.
#
# Writes OUTDIR/xxri-update-VERSION.tar.gz and OUTDIR/manifest.json.  The
# payload is reproducible (sorted, root-owned, mtimes from SOURCE_DATE_EPOCH,
# gzip -n).  Publish it where URL_BASE points (a GitHub release of
# jantzen1212/XXRI-OS-LITE: .../releases/download/vVERSION), then commit
# manifest.json to the repository's main branch - devices read it from
# raw.githubusercontent.com/jantzen1212/XXRI-OS-LITE/main/manifest.json.
set -e
usage() { sed -n '4,6p' "$0" >&2; exit 2; }
VER="" TREE="" URLBASE="" OUT="" TITLE="" NOTES="" MIN="" RESTART=reboot BUILD=""
while getopts v:t:u:o:T:n:m:r:b: o; do
	case $o in
		v) VER=$OPTARG ;; t) TREE=$OPTARG ;; u) URLBASE=${OPTARG%/} ;; o) OUT=$OPTARG ;;
		T) TITLE=$OPTARG ;; n) NOTES=$OPTARG ;; m) MIN=$OPTARG ;; r) RESTART=$OPTARG ;;
		b) BUILD=$OPTARG ;; *) usage ;;
	esac
done
[ -n "$VER" ] && [ -d "$TREE" ] && [ -n "$URLBASE" ] && [ -n "$OUT" ] || usage
echo "$VER" | grep -qE '^[0-9]+(\.[0-9]+){0,3}$' || { echo "version must be numeric, dot-separated: $VER" >&2; exit 2; }
case "$RESTART" in reboot|session|none) ;; *) usage ;; esac
[ -n "$TITLE" ] || TITLE="XXRI OS Lite $VER"
[ -z "$NOTES" ] || [ -r "$NOTES" ] || { echo "notes file not readable: $NOTES" >&2; exit 2; }

# only the members the device accepts
for e in "$TREE"/*; do
	case "${e##*/}" in files|boot|remove.list|UPDATE) ;; *) echo "not part of an update: $e" >&2; exit 2 ;; esac
done
if [ -d "$TREE/boot" ]; then
	for f in "$TREE"/boot/*; do
		case "${f##*/}" in vmlinuz|core.gz|intel-ucode.img|amd-ucode.img) ;;
			*) echo "boot/ may hold vmlinuz, core.gz, intel-ucode.img, amd-ucode.img - not ${f##*/}" >&2; exit 2 ;;
		esac
	done
fi

mkdir -p "$OUT"
PAY="xxri-update-$VER.tar.gz"
EPOCH="${SOURCE_DATE_EPOCH:-$(date +%s)}"
( cd "$TREE" && ls -A | sort | tar --sort=name --owner=0 --group=0 --numeric-owner \
	--mtime="@$EPOCH" --format=gnu -cf - -T - ) | gzip -9n > "$OUT/$PAY"
SHA=$(sha256sum "$OUT/$PAY" | cut -d' ' -f1)
SIZE=$(wc -c < "$OUT/$PAY")

# the values travel in the environment, so a title or notes can hold any text
U_VER="$VER" U_BUILD="${BUILD:-$VER}" U_TITLE="$TITLE" U_NOTES="$NOTES" U_MIN="$MIN" \
U_URL="$URLBASE/$PAY" U_SHA="$SHA" U_SIZE="$SIZE" U_RESTART="$RESTART" \
python3 - "$OUT/manifest.json" <<'PY'
import json, os, sys, datetime
e = os.environ
notes = open(e["U_NOTES"], encoding="utf-8").read().strip() if e["U_NOTES"] else ""
m = {
    "schema": 1,
    "product": "xxri-os-lite",
    "channel": "stable",
    "version": e["U_VER"],
    "build": e["U_BUILD"],
    "released": datetime.date.today().isoformat(),
    "title": e["U_TITLE"],
    "notes": notes,
    "min_version": e["U_MIN"],
    "payload_url": e["U_URL"],
    "payload_sha256": e["U_SHA"],
    "payload_size": int(e["U_SIZE"]),
    "restart": e["U_RESTART"],
}
with open(sys.argv[1], "w", encoding="utf-8") as f:
    json.dump(m, f, ensure_ascii=False, indent=2)
    f.write("\n")
PY
echo "$OUT/$PAY  $SIZE bytes  sha256 $SHA"
echo "$OUT/manifest.json"
