#!/bin/bash
# store-busybox-test.sh - the two places where the Store engine met Tiny Core's busybox and lost.
#
#  1. xxri-app payload_offset: busybox hexdump has no 8-byte unit ("bad byte count for conversion
#     character u"), so the section-header offset of every 64-bit ELF came back empty, the
#     squashfs payload of every x86_64 AppImage went unfound and its integration failed
#     (Helium: "downloads but fails during integration").
#  2. xxri-store japp: `[^,}\]]` is a set followed by a literal "]" in POSIX awk, so a number or
#     true/null never matched and `size` came back empty - no total, so no progress bar.
#
# Runs the real functions/scripts under the device's own busybox applets (a directory of
# `busybox --install -s` links first on PATH, so awk/hexdump/sed are busybox's, not the host's).
#
#   XXRI_BB32 / XXRI_BB64   device busyboxes     XXRI_REPO_DIR   checkout with x64.json
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../.." && pwd)
SRC=$ROOT/src/xxri-store/root/usr/local
BB32=${XXRI_BB32:-$HOME/.xxri-testenv/final-core/bin/busybox}
BB64=${XXRI_BB64:-$HOME/.xxri-testenv/q/x64/vopt64/core/bin/busybox}
REPO=${XXRI_REPO_DIR:-$HOME/xxri-repo}
WORK=$(mktemp -d "${TMPDIR:-/tmp}/store-bb.XXXXXX")
trap 'rm -rf "$WORK"' EXIT
pass=0; fail=0
ok() { if [ "$2" = 0 ]; then pass=$((pass + 1)); printf '  PASS  %s\n' "$1"; else fail=$((fail + 1)); printf '  FAIL  %s %s\n' "$1" "${3:-}"; fi; }

# ELF files with a squashfs magic where payload_offset says the payload starts:
# shoff + shentsize * shnum, for 64-bit (offset above 4 GiB's low word too) and 32-bit
python3 -I - "$WORK" <<'EOF'
import struct, sys
w = sys.argv[1]
def elf64(path, shoff, shentsize, shnum):
    h = bytearray(64)
    h[0:4] = b'\x7fELF'; h[4] = 2; h[5] = 1; h[6] = 1
    struct.pack_into('<H', h, 18, 62)
    struct.pack_into('<Q', h, 40, shoff)
    struct.pack_into('<HHH', h, 58, shentsize, shnum, shnum - 1)
    end = shoff + shentsize * shnum
    b = bytearray(end + 16)
    b[:64] = h; b[end:end + 4] = b'hsqs'
    open(path, 'wb').write(b); return end
def elf32(path, shoff, shentsize, shnum):
    h = bytearray(52)
    h[0:4] = b'\x7fELF'; h[4] = 1; h[5] = 1; h[6] = 1
    struct.pack_into('<H', h, 18, 3)
    struct.pack_into('<I', h, 32, shoff)
    struct.pack_into('<HHH', h, 46, shentsize, shnum, shnum - 1)
    end = shoff + shentsize * shnum
    b = bytearray(end + 16)
    b[:52] = h; b[end:end + 4] = b'hsqs'
    open(path, 'wb').write(b); return end
open(w + '/expect', 'w').write('%d %d\n' % (elf64(w + '/a64', 944000, 64, 30), elf32(w + '/a32', 186000, 40, 30)))
EOF
read -r EXP64 EXP32 < "$WORK/expect"

# the functions under test, cut out of the real script
{
	echo 'byte_at() { hexdump -s "$2" -n 1 -e '"'"'1/1 "%u"'"'"' "$1" 2>/dev/null; }'
	echo 'elf_class() { byte_at "$1" 4; }'
	sed -n '/^payload_offset()/,/^}/p; /^payload_is_squash()/,/^}/p' "$SRC/bin/xxri-app"
} > "$WORK/fns.sh"

for bb in 32 64; do
	BB=$(eval echo \$BB$bb)
	[ -x "$BB" ] || { echo "missing busybox: $BB" >&2; exit 2; }
	mkdir -p "$WORK/bin$bb"; "$BB" --install -s "$WORK/bin$bb" 2>/dev/null
	ln -sf "$BB" "$WORK/bin$bb/busybox"
	echo "== device busybox $bb ($("$BB" 2>&1 | head -1))"
	off=$(PATH=$WORK/bin$bb "$WORK/bin$bb/ash" -c ". $WORK/fns.sh; payload_offset $WORK/a64" 2>&1)
	ok "ELF64 payload offset ($off, want $EXP64)" "$([ "$off" = "$EXP64" ] && echo 0 || echo 1)" "$off"
	sq=$(PATH=$WORK/bin$bb "$WORK/bin$bb/ash" -c ". $WORK/fns.sh; payload_is_squash $WORK/a64 $EXP64 && echo squash")
	ok "  the squashfs magic is there" "$([ "$sq" = squash ] && echo 0 || echo 1)"
	off=$(PATH=$WORK/bin$bb "$WORK/bin$bb/ash" -c ". $WORK/fns.sh; payload_offset $WORK/a32" 2>&1)
	ok "ELF32 payload offset ($off, want $EXP32)" "$([ "$off" = "$EXP32" ] && echo 0 || echo 1)" "$off"

	# the catalog reader: numbers, booleans and null at depth 1; the nested object's own "size" stays out
	H=$WORK/home$bb; mkdir -p "$H/.cache/xxri-store"
	python3 -I - "$H/.cache/xxri-store/x64.json" <<'EOF'
import json, sys
apps = [
 {"id": "num", "title": "Num", "download_url": "http://x/num", "checksum": "abc", "version": "1", "size": 158005752,
  "verified": True, "rank": 22, "sources": [{"size": 5, "url": "u"}], "architecture": "x86_64"},
 {"id": "nul", "title": "Nul", "download_url": "http://x/nul", "version": "2", "size": None, "architecture": "x86_64"},
 {"id": "str", "title": "Str", "download_url": "http://x/str", "version": "3", "size": "77", "architecture": "x86_64"}]
json.dump({"repository": "t", "schema_version": "3.0", "last_updated": "2026-10-26", "applications": apps},
          open(sys.argv[1], "w"), indent=2)
EOF
	run() { # args... -> xxri-store output, busybox applets first on PATH, arch from the busybox's own ELF header
		bwrap --ro-bind / / --tmpfs /usr/local/share --tmpfs /usr/local/etc --tmpfs /usr/local/lib --dev /dev --proc /proc \
			--ro-bind "$SRC/lib/xxri" /usr/local/lib/xxri --ro-bind "$SRC/etc/xxri" /usr/local/etc/xxri --bind "$H" "$H" \
			--setenv HOME "$H" --setenv PATH "$WORK/bin$bb" "$WORK/bin$bb/ash" "$SRC/bin/xxri-store" "$@"
	}
	if [ $bb = 64 ]; then
		r=$(run resolve num | cut -f4);  ok "resolve: a numeric size comes through ($r)" "$([ "$r" = 158005752 ] && echo 0 || echo 1)"
		r=$(run resolve nul | cut -f4);  ok "resolve: size null -> 0 ($r)" "$([ "$r" = 0 ] && echo 0 || echo 1)"
		r=$(run resolve str | cut -f4);  ok "resolve: size \"77\" (a string) -> 77 ($r)" "$([ "$r" = 77 ] && echo 0 || echo 1)"
		r=$(run field num size);         ok "field size = 158005752 ($r)" "$([ "$r" = 158005752 ] && echo 0 || echo 1)"
		if [ -f "$REPO/x64.json" ]; then
			cp "$REPO/x64.json" "$H/.cache/xxri-store/x64.json"
			r=$(run resolve helium | cut -f4)
			ok "the real catalog: Helium's size reaches the progress bar ($r)" "$([ "${r:-0}" -gt 100000000 ] && echo 0 || echo 1)"
		fi
	fi
done

# ---- `installed` / `updates` with a registry: fast (one catalog scan, not one per app and field) and right.
# They used to scan the whole catalog ~14 times per installed app (about 1 s each on a fast desktop), and
# the Store's poll ran them on its GUI thread: it froze on opening.
if [ -f "$REPO/x64.json" ]; then
	RH=$WORK/regs; mkdir -p "$RH/reg" "$RH/stubs" "$RH/home/.cache/xxri-store"; cp "$REPO/x64.json" "$RH/home/.cache/xxri-store/x64.json"
	python3 -I - "$REPO/x64.json" "$RH/reg" <<'EOF'
import json, sys
cat = json.load(open(sys.argv[1]))["applications"]
n = 0
for a in cat:
    if n < 12 and a["package_type"].endswith("appimage"):
        open("%s/%s.reg" % (sys.argv[2], a["id"]), "w").write("ID=%s\nNAME=%s\nFILE=/x/%s\nVERSION=0.0.1\nICON=\nDESKTOP=\nLOCATION=/x\nINSTALLED=2026-10-01 10:00\nLAST_LAUNCH=never\nINTEGRATED=1\n" % (a["id"], a["title"], a["id"]))
        n += 1
# one entry whose id is not the catalog's, matched by its name; one the catalog does not know
h = [a for a in cat if a["id"] == "helium"][0]
open(sys.argv[2] + "/helium-9.reg", "w").write("ID=helium-9\nNAME=Helium\nFILE=/x/h\nVERSION=0.1\nICON=\nDESKTOP=\nLOCATION=/x\nINSTALLED=2026-10-01 10:00\nLAST_LAUNCH=never\nINTEGRATED=1\n")
open(sys.argv[2] + "/zz-unknown.reg", "w").write("ID=zz-unknown\nNAME=Not In The Catalog\nFILE=/x/z\nVERSION=1\nICON=\nDESKTOP=\nLOCATION=/x\nINSTALLED=2026-10-01 10:00\nLAST_LAUNCH=never\nINTEGRATED=1\n")
EOF
	for n in xxri-store xxri-app; do printf '#!/bin/sh\nexec %s/ash %s/bin/%s "$@"\n' "$WORK/bin64" "$SRC" $n > "$RH/stubs/$n"; chmod +x "$RH/stubs/$n"; done
	reg() { bwrap --ro-bind / / --tmpfs /usr/local/share --tmpfs /usr/local/etc --tmpfs /usr/local/lib --dev /dev --proc /proc \
		--ro-bind "$SRC/lib/xxri" /usr/local/lib/xxri --ro-bind "$SRC/etc/xxri" /usr/local/etc/xxri --ro-bind "$RH/reg" /usr/local/share/xxri-apps \
		--bind "$RH/home" "$RH/home" --setenv HOME "$RH/home" --setenv PATH "$RH/stubs:$WORK/bin64" "$WORK/bin64/ash" -c "$*"; }
	reg "xxri-store updates" > /dev/null 2>&1          # builds the index once
	t0=$(date +%s%N); reg "xxri-store updates" > "$RH/upd.json" 2>/dev/null; t1=$(date +%s%N); reg "xxri-store installed" > "$RH/inst.json" 2>/dev/null; t2=$(date +%s%N)
	ms_u=$(( (t1 - t0) / 1000000 )); ms_i=$(( (t2 - t1) / 1000000 ))
	ok "installed/updates with 14 registry entries: ${ms_i} ms / ${ms_u} ms (each under 4 s; it was ~10 s)" "$([ $ms_u -lt 4000 ] && [ $ms_i -lt 4000 ] && echo 0 || echo 1)"
	python3 -I - "$REPO/x64.json" "$RH/reg" "$RH/inst.json" "$RH/upd.json" <<'EOF'
import json, glob, sys
cat = json.load(open(sys.argv[1]))["applications"]; byid = {a["id"]: a for a in cat}
bytitle = {}
for a in cat: bytitle.setdefault(a["title"].lower(), a["id"])
exp_i, exp_u = {}, {}
for f in glob.glob(sys.argv[2] + "/*.reg"):
    r = dict(l.rstrip("\n").split("=", 1) for l in open(f) if "=" in l)
    cid = r["ID"] if r["ID"] in byid else bytitle.get(r["NAME"].lower(), "")
    exp_i[r["ID"]] = cid
    if cid:
        a = byid[cid]; lat = a.get("version") or a.get("current_upstream_version") or ""
        if lat and lat != "latest" and lat != r["VERSION"]:
            exp_u[r["ID"]] = (cid, lat, a["size"] if isinstance(a.get("size"), int) else 0)
inst = {a["id"]: a["catalog_id"] for a in json.load(open(sys.argv[3]))["apps"]}
upd = {u["id"]: (u["catalog_id"], u["latest"], u["size"]) for u in json.load(open(sys.argv[4]))["updates"]}
sys.exit(0 if inst == exp_i and upd == exp_u else 1)
EOF
	ok "  ... and the answers are right (catalog ids, by-name match, unknown app, latest versions, sizes)" $?
fi

echo; echo "store-busybox-test: $pass passed, $fail failed"
[ $fail = 0 ]
