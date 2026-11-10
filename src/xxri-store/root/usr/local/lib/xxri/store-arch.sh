# store-arch.sh - which architecture is this OS? Sourced by xxri-store and
# xxri-app, so the catalog, the Tiny Core mirror and the "can this binary run
# here" check all agree.
#
# The answer is the userspace's, read from the ELF header of the running shell.
# uname -m is the kernel's (a 32-bit userspace on a 64-bit kernel says x86_64)
# and /proc/cpuinfo only says what the CPU could run.
#
# detect_os_arch sets OS_ARCH (i686 | x86_64, empty when unknown), OS_BITS,
# OS_EMACH (ELF e_machine), OS_ARCH_NAME, CATALOG_NAME (the repository file for
# it) and TC_ARCH (Tiny Core's name for it); it fails when the arch is unknown.

elf_arch() { # ELF file -> i686 | x86_64 (fails for anything else)
	# bytes 0-3 magic, 4 class (1 = 32-bit, 2 = 64-bit), 5 byte order (1 = little),
	# 18-19 e_machine
	set -- $(dd if="$1" bs=1 count=20 2>/dev/null | od -An -tu1)
	[ "$#" -eq 20 ] && [ "$1.$2.$3.$4.$6" = 127.69.76.70.1 ] || return 1
	case "$5.$(( ${19} + 256 * ${20} ))" in
		1.3)  echo i686 ;;
		2.62) echo x86_64 ;;
		*)    return 1 ;;
	esac
}

detect_os_arch() {
	OS_ARCH=""; ARCH_PROBE=""
	for _p in /proc/$$/exe /bin/busybox /bin/sh; do
		[ -r "$_p" ] || continue
		OS_ARCH="$(elf_arch "$_p")" && { ARCH_PROBE="$_p"; break; }
		OS_ARCH=""
	done
	case "$OS_ARCH" in
		i686)   OS_BITS=32; OS_EMACH=3;  OS_ARCH_NAME="32-bit Intel (i686)"
		        CATALOG_NAME=i686.json; TC_ARCH=x86 ;;
		x86_64) OS_BITS=64; OS_EMACH=62; OS_ARCH_NAME="64-bit Intel/AMD (x86_64)"
		        CATALOG_NAME=x64.json;  TC_ARCH=x86_64 ;;
		*)      OS_BITS=0;  OS_EMACH=0;  OS_ARCH_NAME=unknown
		        CATALOG_NAME="";        TC_ARCH="" ;;
	esac
	[ -n "$OS_ARCH" ]
}
