#!/bin/bash
# tools/build-grub.sh - build upstream GNU GRUB for the XXRI boot targets.
#
# XXRI does not carry its own boot loader and does not use a distribution's
# GRUB package: it builds the signed upstream release tarball, unpatched, for
#
#   i386-pc     BIOS / legacy / CSM (El Torito, MBR and GPT disks)
#   i386-efi    32-bit (IA32) UEFI: EFI/BOOT/BOOTIA32.EFI
#   x86_64-efi  64-bit UEFI.  Only used when build-iso.sh is asked for the
#               experimental BOOTX64.EFI (see docs/boot-2.6.md): upstream GRUB
#               starts a kernel without XLF_KERNEL_64 through the 32-bit boot
#               protocol after ExitBootServices, so it can run the i686 kernel.
#
# and, with --target-userland, the GRUB utilities (grub-install, grub-mkimage,
# grub-bios-setup, grub-probe, grub-editenv) as i686 binaries for the XXRI
# installer, compiled natively inside an i686 root made from core.gz.
#
# Output: build/grub/<platform>/{bin,lib/grub/<platform>,...}  (git-ignored)
#         build/grub/VERSION, build/grub/SOURCES.sha256
#
# The tarball is verified against the GNU keyring before it is unpacked.  The
# same tarball is what build-iso.sh publishes next to the ISO as the GPLv3
# Corresponding Source.
set -euo pipefail
HERE=$(cd "$(dirname "$0")/.." && pwd)
GRUB_VERSION=2.14
GRUB_SHA256=bc8d3c73535b8838d8c8e2654d73edc4e6ae8c8acdb45d5df5dc9a1547446d43
TARBALL=grub-$GRUB_VERSION.tar.xz
SRCDIR=${XXRI_SOURCES:-$HERE/sources}
OUT=$HERE/build/grub
PLATFORMS="i386-pc i386-efi x86_64-efi"
JOBS=$(nproc)
# GRUB 2.14's configure uses -Wl,--image-base (meant for LLVM lld) whenever the
# linker accepts it.  GNU ld >= 2.44 accepts it too but then counts the ELF
# headers into the first segment, so i386-pc kernel.img starts at 0x9074 and
# grub-mkimage rejects it as "miscompiled".  Arch reverts upstream commit
# 1a5417f3 for this; seeding the configure cache selects GRUB's traditional
# -Ttext instead, with the source left unmodified.
LD_FIX="ax_cv_check_ldflags___Wl___image_base_0x400000=no"

fetch() {
	mkdir -p "$SRCDIR"
	for f in "$TARBALL" "$TARBALL.sig"; do
		[ -s "$SRCDIR/$f" ] || curl -sfL -o "$SRCDIR/$f" "https://ftp.gnu.org/gnu/grub/$f"
	done
	[ -s "$SRCDIR/gnu-keyring.gpg" ] ||
		curl -sfL -o "$SRCDIR/gnu-keyring.gpg" https://ftp.gnu.org/gnu/gnu-keyring.gpg
	gpgv --keyring "$SRCDIR/gnu-keyring.gpg" "$SRCDIR/$TARBALL.sig" "$SRCDIR/$TARBALL" \
		2>&1 | grep -q 'Good signature' || { echo "!! $TARBALL: bad or missing signature" >&2; exit 1; }
	echo "$GRUB_SHA256  $SRCDIR/$TARBALL" | sha256sum -c --quiet ||
		{ echo "!! $TARBALL: sha256 mismatch" >&2; exit 1; }
}

unpack() { # $1 destination
	rm -rf "$1"; mkdir -p "$1"
	tar -xf "$SRCDIR/$TARBALL" -C "$1" --strip-components=1
}

build_platform() { # $1 = i386-pc | i386-efi | x86_64-efi
	local p=$1 target=${1%%-*} platform=${1#*-}
	local b=$HERE/build/grub-work/$p
	unpack "$b/src"
	# Always a fresh object tree: make does not track configure flags, so a
	# kept tree relinks nothing that does not include config.h (found when the
	# --image-base fix left a stale, misplaced i386-pc/pxeboot.image behind).
	rm -rf "$b/obj"; mkdir -p "$b/obj"
	( cd "$b/obj"
	  ../src/configure --quiet --target="$target" --with-platform="$platform" \
		--prefix=/usr --disable-werror --disable-nls \
		--disable-grub-mkfont --disable-grub-themes --disable-grub-mount \
		--disable-device-mapper --disable-libzfs --disable-liblzma "$LD_FIX"
	  make -s -j"$JOBS"
	  rm -rf "$OUT/$p"
	  make -s install DESTDIR="$OUT/$p" ) >"$b/build.log" 2>&1 ||
		{ echo "!! GRUB $p build failed - see $b/build.log" >&2; tail -20 "$b/build.log" >&2; exit 1; }
	echo "   $p: $(ls "$OUT/$p/usr/lib/grub/$p"/*.mod | wc -l) modules"
}

# i686 userland for the installer.  Built natively: the x86_64 host kernel runs
# i686 binaries, so a root unpacked from XXRI's own core.gz plus Tiny Core's
# compiletc gives the target's exact glibc and gcc (see work/xxri-file/br.sh).
build_userland() {
	local br=${XXRI_I686_ROOT:?set XXRI_I686_ROOT to an i686 build root with compiletc}
	local b=$HERE/build/grub-work/userland
	unpack "$b/src"
	rm -rf "$b/obj"; mkdir -p "$b/obj"
	# configure runs Python (to read the platform groups from gentpl.py); the
	# i686 root has no interpreter, so the host's python3 runs through the
	# host's own loader from /host.  Build-time only - nothing it touches ships.
	local py; py=$(readlink -f "$(command -v python3)")
	printf '#!/bin/sh\nPYTHONHOME=/host/usr exec /host/usr/lib/ld-linux-x86-64.so.2 --library-path /host/usr/lib /host%s "$@"\n' \
		"$py" > "$b/python3"; chmod +x "$b/python3"
	# The build root is only read: everything is written under /tmp/build (= build/grub-work/userland).
	bwrap --ro-bind "$br" / --ro-bind / /host --dev /dev --proc /proc --tmpfs /tmp \
		--bind "$b" /tmp/build --setenv PATH /usr/local/bin:/usr/bin:/bin:/usr/local/sbin:/usr/sbin:/sbin \
		--setenv LD_LIBRARY_PATH /usr/local/lib:/lib:/usr/lib \
		--chdir /tmp/build/obj /bin/sh -c "
		../src/configure --quiet --target=i386 --with-platform=pc --prefix=/usr/local \
			--disable-werror --disable-nls --disable-grub-mkfont --disable-grub-themes \
			--disable-grub-mount --disable-device-mapper --disable-libzfs --disable-liblzma \
			CFLAGS='-Os' PYTHON=/tmp/build/python3 $LD_FIX &&
		make -s -j$JOBS && make -s install DESTDIR=/tmp/build/dest" >"$b/build.log" 2>&1 ||
		{ echo "!! GRUB i686 userland build failed - see $b/build.log" >&2; tail -20 "$b/build.log" >&2; exit 1; }
	rm -rf "$OUT/i686-userland"; mkdir -p "$OUT/i686-userland/sbin"
	for t in grub-install grub-mkimage grub-bios-setup grub-probe grub-editenv; do
		f=$(find "$b/dest/usr/local" -name "$t" -type f | head -1)
		[ -n "$f" ] || { echo "!! $t missing from the i686 build" >&2; exit 1; }
		strip -o "$OUT/i686-userland/sbin/$t" "$f"
	done
	file "$OUT/i686-userland/sbin/grub-install" | grep -qE 'Intel (80386|i386)' ||
		{ echo "!! userland is not i386" >&2; exit 1; }
	echo "   i686 userland: $(ls "$OUT/i686-userland/sbin" | tr '\n' ' ')"
}

fetch
mkdir -p "$OUT"
case "${1:-}" in
	--target-userland) build_userland ;;
	*) for p in $PLATFORMS; do build_platform "$p"; done ;;
esac
echo "GNU GRUB $GRUB_VERSION (upstream release tarball, unpatched)" > "$OUT/VERSION"
echo "$GRUB_SHA256  $TARBALL" > "$OUT/SOURCES.sha256"
