#!/bin/sh
# Build index.tsv for one architecture of the XXRI runtime repository.
#
#   tools/runtime-libs/make-index.sh DIR
#
# DIR holds the libraries for one architecture (runtime/x86_64/ or
# runtime/i686/ in the repository), each file named exactly like the soname
# programs ask for (libudev.so.1).  This writes DIR/index.tsv:
#
#   soname <TAB> sha256 <TAB> size <TAB> file
#
# which the Store reads to install the host libraries an AppImage needs.
# Publish DIR with the rest of the repository (repo.xxri.flows.best/runtime/<arch>/).
set -eu
dir="${1:?usage: make-index.sh DIR}"
[ -d "$dir" ] || { echo "$dir: not a directory" >&2; exit 1; }
out="$dir/index.tsv"
tmp="$out.tmp"
printf '# soname\tsha256\tsize\tfile\n' > "$tmp"
for f in "$dir"/*.so*; do
	[ -f "$f" ] || continue
	[ -L "$f" ] && { echo "$f: symlinks are not served, ship the real file" >&2; exit 1; }
	n="${f##*/}"
	printf '%s\t%s\t%s\t%s\n' "$n" "$(sha256sum "$f" | cut -d' ' -f1)" "$(wc -c < "$f")" "$n" >> "$tmp"
done
mv "$tmp" "$out"
echo "$out: $(($(wc -l < "$out") - 1)) libraries"
