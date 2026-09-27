#!/bin/bash
# xxri-tcz-sysroot.sh MANIFEST CACHE SYSROOT PCLIST TC_CORE_GZ
# Fetch each Tiny Core 16.x package in MANIFEST (name md5) from the mirror,
# check its md5, and unpack it into SYSROOT (Tiny Core installs under
# /usr/local, so headers, libraries and .pc files land in SYSROOT/usr/local).
# PCLIST receives the pkg-config module names the packages provide: PTXdist's
# pkg-config wrapper only answers for modules a package's dependencies declare.
set -euo pipefail
M=$1 CACHE=$2 SYSROOT=$3 PCLIST=$4 CORE=$5
MIRROR=http://repo.tinycorelinux.net/16.x/x86/tcz
mkdir -p "$CACHE" "$SYSROOT"
: > "$PCLIST.tmp"
n=0
while read -r name md5; do
	case "$name" in ''|'#'*) continue ;; esac
	f="$CACHE/$name"
	[ -s "$f" ] && [ "$(md5sum < "$f" | cut -c1-32)" = "$md5" ] || wget -q -O "$f" "$MIRROR/$name"
	[ "$(md5sum < "$f" | cut -c1-32)" = "$md5" ] || { echo "$name: md5 mismatch" >&2; exit 1; }
	unsquashfs -q -f -n -d "$SYSROOT" "$f" >/dev/null
	unsquashfs -l -d '' "$f" | sed -n 's|.*/pkgconfig/\(.*\)\.pc$|\1|p' >> "$PCLIST.tmp"
	n=$((n + 1))
done < "$M"
sort -u "$PCLIST.tmp" > "$PCLIST" && rm -f "$PCLIST.tmp"
# Tiny Core keeps some libraries in its base (core.gz), not in extensions:
# libz, libuuid, libblkid, libcrypt... (their headers come with the *_base-dev
# packages above).  The linker needs them to resolve the extensions' own
# NEEDED entries.  glibc, libgcc_s and libstdc++ are the toolchain's (the same
# glibc 2.40 / GCC 14 generation), so they are left out.
T=$(mktemp -d)
zcat "$CORE" | ( cd "$T" && cpio -idm --quiet 'lib/*' 'usr/lib/*' 2>/dev/null )
for d in lib usr/lib; do
	mkdir -p "$SYSROOT/$d"
	( cd "$T/$d" && find . -maxdepth 1 \( -type f -o -type l \) -name 'lib*.so*' ) | sed 's|^\./||' |
		grep -Ev '^(ld-linux|libc|libm|libdl|libpthread|librt|libresolv|libnsl|libutil|libanl|libnss_[a-z]+|libgcc_s|libstdc\+\+|libmvec|libBrokenLocale|libthread_db|libmemusage|libpcprofile|libSegFault)\.so' |
		while read -r f; do cp -a "$T/$d/$f" "$SYSROOT/$d/$f"; done
done
rm -rf "$T"
# .la files would point libtool at /usr/local on the build host
find "$SYSROOT/usr/local/lib" -name '*.la' -delete
# Relocatable .pc files, as PTXdist makes its own: every /usr/local becomes
# ${pcfiledir}/../.. (both lib/pkgconfig and share/pkgconfig sit two levels
# below /usr/local), so pkg-config answers with paths inside the sysroot.
find "$SYSROOT/usr/local/lib/pkgconfig" "$SYSROOT/usr/local/share/pkgconfig" -name '*.pc' \
	-exec sed -i 's|/usr/local|${pcfiledir}/../..|g' {} + 2>/dev/null || true
echo "tc-sysroot: $n packages unpacked into $SYSROOT"
