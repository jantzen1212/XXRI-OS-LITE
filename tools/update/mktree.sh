#!/bin/bash
# mktree.sh - the files/ tree and remove.list of an update, from the ISO the
# devices were installed from and the ISO the update brings them to.  Feed the
# tree to mkupdate.sh (docs/software-update.md).
#
#   tools/update/mktree.sh OLD.iso NEW.iso TREE
#
# Installed systems run from a disk that holds core.gz's files and every
# extension's files, so both are compared: core.gz, and each cde/optional
# .tcz whose checksum changed.  Left out: device nodes, and the installer,
# which an installed system deletes at boot.  A changed kernel or /init stops it - those need a
# new ISO, or boot/ files made by hand.
set -euo pipefail
[ $# = 3 ] || { sed -n '6p' "$0" >&2; exit 2; }
# as root: Tiny Core has mode 0111 files (sudo), and the payload keeps modes
[ "$(id -u)" = 0 ] || exec fakeroot -- "$0" "$@"
OLD=$1 NEW=$2 TREE=$3
[ -e "$TREE" ] && { echo "$TREE exists; give a new directory" >&2; exit 2; }
W=$(mktemp -d); trap 'chmod -R u+w "$W"; rm -rf "$W"' EXIT

iso() { xorriso -osirrox on -indev "$1" -extract "$2" "$3" >/dev/null 2>&1; }
root() { # ISO DIR -> core.gz and the changed extensions unpacked into DIR
	mkdir -p "$2"
	iso "$1" /boot/core.gz "$W/core.gz"
	( cd "$2" && zcat "$W/core.gz" | cpio -idm --quiet 2>/dev/null ) || true
	iso "$1" /boot/vmlinuz "$2.vmlinuz"
}
for s in OLD NEW; do
	iso "${!s}" /cde/optional "$W/$s.cde"
	root "${!s}" "$W/$s"
done

cmp -s "$W/OLD.vmlinuz" "$W/NEW.vmlinuz" || { echo "the kernel changed: not an update this tool makes" >&2; exit 1; }
cmp -s "$W/OLD/init" "$W/NEW/init" || { echo "/init changed: needs a new core.gz, not an update this tool makes" >&2; exit 1; }
for t in "$W/NEW.cde"/*.tcz; do
	n=${t##*/}
	cmp -s "$t" "$W/OLD.cde/$n" 2>/dev/null && continue
	[ -f "$W/OLD.cde/$n" ] && unsquashfs -q -f -d "$W/OLD" "$W/OLD.cde/$n" >/dev/null
	unsquashfs -q -f -d "$W/NEW" "$t" >/dev/null
	echo "extension changed: $n"
done

SKIP='^(dev/.*|usr/local/bin/xxri-installer|usr/sbin/xxri-install-backend|usr/sbin/install2disk|usr/local/share/applications/xxri-installer\.desktop|usr/local/share/xxri-installer/.*)$'
mkdir -p "$TREE/files"
( cd "$W/NEW" && find . \( -type f -o -type l \) | sed 's|^\./||' | sort ) > "$W/new.list"
( cd "$W/OLD" && find . \( -type f -o -type l \) | sed 's|^\./||' | sort ) > "$W/old.list"
while IFS= read -r f; do
	echo "$f" | grep -qE "$SKIP" && continue
	[ -c "$W/NEW/$f" ] || [ -b "$W/NEW/$f" ] || [ -p "$W/NEW/$f" ] && continue
	if [ -L "$W/OLD/$f" ] || [ -L "$W/NEW/$f" ]; then
		[ "$(readlink "$W/OLD/$f" 2>/dev/null)" = "$(readlink "$W/NEW/$f")" ] && continue
	elif [ -f "$W/OLD/$f" ] && cmp -s "$W/OLD/$f" "$W/NEW/$f"; then
		continue
	fi
	mkdir -p "$TREE/files/$(dirname "$f")"
	cp -a "$W/NEW/$f" "$TREE/files/$f"
	echo "update: /$f"
done < "$W/new.list"
comm -23 "$W/old.list" "$W/new.list" | grep -vE "$SKIP" | sed 's|^|/|' > "$TREE/remove.list" || true
[ -s "$TREE/remove.list" ] && sed 's/^/remove: /' "$TREE/remove.list" || rm -f "$TREE/remove.list"
