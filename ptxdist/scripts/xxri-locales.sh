#!/bin/bash
# xxri-locales.sh TOOLCHAIN_SYSROOT OUTDIR LOCALE...
# Compile UTF-8 locales (glibc names such as pt_PT) with the toolchain's
# localedef into OUTDIR/<LOCALE>.utf8/.  Plain per-locale directories rather
# than a locale-archive, which would sit mostly empty in the initramfs.
set -euo pipefail
SYSROOT=$1 OUT=$2; shift 2
LOCALEDEF=$SYSROOT/usr/bin/localedef
I18N=$SYSROOT/usr/share/i18n
[ -x "$LOCALEDEF" ] || { echo "xxri-locales: no localedef in $SYSROOT" >&2; exit 1; }

rm -rf "$OUT"; mkdir -p "$OUT"
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
# localedef wants the charmap uncompressed
zcat "$I18N/charmaps/UTF-8.gz" > "$tmp/UTF-8"

for l in "$@"; do
	# 32-bit binary: run it with the toolchain's own loader
	"$SYSROOT/lib/ld-linux.so.2" --library-path "$SYSROOT/lib:$SYSROOT/usr/lib" \
		"$LOCALEDEF" --no-archive --little-endian \
		-i "$I18N/locales/$l" -f "$tmp/UTF-8" "$tmp/$l.utf8"
	mv "$tmp/$l.utf8" "$OUT/"
done

# localedef hard-links the files that locales share (LC_COLLATE alone is
# 2.5 MB); the image keeps no hard links, so use relative symlinks
first=()
for d in "$OUT"/*/; do
	d=${d%/}
	for f in "$d"/LC_*; do
		[ -f "$f" ] || continue
		for p in ${first[@]+"${first[@]}"}; do
			g=$p/${f##*/}
			if [ -f "$g" ] && [ ! -L "$g" ] && cmp -s "$f" "$g"; then
				rm "$f"; ln -s "../${p##*/}/${f##*/}" "$f"; break
			fi
		done
	done
	first+=("$d")
done
[ -z "$(find "$OUT" -type f -empty)" ] || { echo "xxri-locales: empty locale file" >&2; exit 1; }
