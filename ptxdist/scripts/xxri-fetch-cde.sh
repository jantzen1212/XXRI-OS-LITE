#!/bin/bash
# xxri-fetch-cde.sh MANIFEST_DIR CACHE OUT
# Assemble the Tiny Core part of the XXRI extension directory (cde/) from
# pinned upstream sources (XXRI's own extensions are image rules):
#   tc16        repo.tinycorelinux.net/16.x/x86/tcz/<name>        (md5 checked)
#   tc16.0-iso  /cde/optional/<name> in the archived TinyCore-16.0.iso (md5 checked)
# Anything whose checksum does not match stops the build: an upstream package
# rebuilt in place shows up here instead of silently changing the ISO.
set -euo pipefail
M=$1 CACHE=$2 OUT=$3
MIRROR=http://repo.tinycorelinux.net/16.x/x86/tcz
ISO160=http://tinycorelinux.net/16.x/x86/archive/16.0/TinyCore-16.0.iso
ISO160_MD5=0be8fc418289e148619a70d7b63246a2
mkdir -p "$CACHE" "$OUT/optional"
md5ok() { [ "$(md5sum < "$1" | cut -c1-32)" = "$2" ]; }
if grep -q ' tc16.0-iso$' "$M/manifest"; then
	[ -s "$CACHE/TinyCore-16.0.iso" ] && md5ok "$CACHE/TinyCore-16.0.iso" $ISO160_MD5 ||
		wget -q -O "$CACHE/TinyCore-16.0.iso" $ISO160
	md5ok "$CACHE/TinyCore-16.0.iso" $ISO160_MD5 || { echo "TinyCore-16.0.iso: md5 mismatch" >&2; exit 1; }
fi
n=0
while read -r name md5 origin; do
	case "$name" in ''|'#'*) continue ;; esac
	f="$CACHE/$name"
	case "$origin" in
	tc16)
		md5ok "$f" "$md5" 2>/dev/null || wget -q -O "$f" "$MIRROR/$name" ;;
	tc16.0-iso)
		md5ok "$f" "$md5" 2>/dev/null ||
			xorriso -osirrox on -indev "$CACHE/TinyCore-16.0.iso" -extract "/cde/optional/$name" "$f" >/dev/null 2>&1
		chmod u+w "$f" ;;
	*) echo "$name: unknown origin $origin" >&2; exit 1 ;;
	esac
	md5ok "$f" "$md5" || { echo "$name: md5 mismatch (upstream rebuilt it?)" >&2; exit 1; }
	cp "$f" "$OUT/optional/$name"
	( cd "$OUT/optional" && md5sum "$name" > "$name.md5.txt" )
	[ -f "$M/dep/$name.dep" ] && cp "$M/dep/$name.dep" "$OUT/optional/"
	n=$((n + 1))
done < "$M/manifest"
cp "$M/onboot.lst" "$M/copy2fs.lst" "$M/xbase.lst" "$OUT/"
echo "xxri-cde: $n extensions verified"
