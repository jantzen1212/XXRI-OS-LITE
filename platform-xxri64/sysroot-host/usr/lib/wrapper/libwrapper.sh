#!/bin/sh

set -e

COMPILING=false
FORTIFY=true
FPIE=true
HOST=false
LINKING=true
OPTIMIZE=false
PIE=true
STDLIB=true
BUILDID=true
FULL_DEBUG=false

ARG_LIST=""
LATE_ARG_LIST=""

if [ -z "${PTXDIST_PLATFORMCONFIG}" ]; then
	. "${WRAPPER_DIR}/env" || exit
fi

. ${PTXDIST_PLATFORMCONFIG}

CMD="${0##*/}"
FULL_CMD="$(readlink "${0%/*}/real/${CMD}")"

wrapper_exec() {
	IFS=:
	tmp=
	for P in ${PATH}; do
		if [ "${P}" != "${PTXDIST_PATH_SYSROOT_HOST}/usr/lib/wrapper" ]; then
			tmp="${tmp}${tmp:+:}${P}"
		fi
	done
	unset IFS
	PATH="${tmp}"
	if [ -z "${ICECC_VERSION}" -o ! -e "${ICECC_VERSION}" ]; then
		PTXDIST_ICECC=${PTXDIST_ICERUN}
	fi
	if [ -n "${FAKEROOTKEY}" ]; then
		PTXDIST_ICECC=""
	fi
	if ! ${HOST} && [ -n "${PTXDIST_WORKSPACE}" ]; then
		IFS="$(printf "\037")"
		set -- $(filter_args "${@}")
		unset IFS
	fi
	if $COMPILING && [ -n "${PTXDIST_COMPILE_COMMANDS}" ]; then
		printf "%s\037%s\n" "${PWD}" "${CMD} ${ARG_LIST} $* ${LATE_ARG_LIST}" >> "${PTXDIST_COMPILE_COMMANDS}"
	fi
	if [ "${PTXDIST_VERBOSE}" = 1 -a -n "${PTXDIST_LOGFILE_PATH}" ]; then
		echo "wrapper: ${PTXDIST_ICECC}${PTXDIST_CCACHE} ${CMD} ${ARG_LIST} $* ${LATE_ARG_LIST}" >>${PTXDIST_LOGFILE_PATH}
	fi
	exec ${PTXDIST_ICECC}${PTXDIST_CCACHE} "${FULL_CMD}" ${ARG_LIST} "$@" ${LATE_ARG_LIST}
}

filter_args() {
	# PTXDIST_SYSROOT_TOOLCHAIN may not be defined yet
	if [ -n "${PTXDIST_SYSROOT_TOOLCHAIN}" ]; then
		pkg_wrapper_accept_paths="${PTXDIST_SYSROOT_TOOLCHAIN}${IFS}${pkg_wrapper_accept_paths}"
		if [ "${PTXDIST_SYSROOT_TOOLCHAIN}" != "${PTXDIST_REAL_SYSROOT_TOOLCHAIN}" ]; then
			pkg_wrapper_accept_paths="${PTXDIST_REAL_SYSROOT_TOOLCHAIN}${IFS}${pkg_wrapper_accept_paths}"
		fi
	fi
	pkg_wrapper_accept_paths="${PTXDIST_PLATFORMDIR}${IFS}${pkg_wrapper_accept_paths}"
	if [ "${PTXDIST_PLATFORMDIR}" != "${PTXDIST_REAL_PLATFORMDIR}" ]; then
		pkg_wrapper_accept_paths="${PTXDIST_REAL_PLATFORMDIR}${IFS}${pkg_wrapper_accept_paths}"
	fi
	for ARG in "${@}"; do
		good=false
		for path in ${pkg_wrapper_accept_paths}; do
			if [ -z "${path}" ]; then
				continue
			fi
			case "${ARG}" in
			-[IL]"${path}"*)
				good=true
				;;
			esac
			if ${good}; then
				break
			fi
		done
		if ! ${good}; then
			case "${ARG}" in
			-L/*|-I/*)
				# skip all absolute search directories outside the BSP
				echo "wrapper: removing '${ARG}' from the commandline" >&2
				continue
				;;
			esac
		fi
		case "${ARG}" in
			-I/*/sysroot-host/*|-I/*/sysroot-cross/*|-I/*/sysroot-target/*)
				# turn include paths in sysroot into system includes
				printf "%s\037" "-isystem"
				ARG="${ARG#-I}"
				;;
			-isystem)
				;;
			-isystem*)
				# not detected correctly without space by icecc so split it
				printf "%s\037" "-isystem"
				ARG="${ARG#-isystem}"
				;;
		esac
		printf "%s\037" "${ARG}"
	done
}

cc_check_args() {
	for ARG in "$@"; do
		case "${ARG}" in
			-c | -E)
				LINKING=false
				PIE=false
				;;
			-D_FORTIFY_SOURCE | -D_FORTIFY_SOURCE=*)
				FORTIFY=false
				;;
			-r | -Wl,--build-id | -Wl,--build-id=*)
				BUILDID=false
				;;
			-nostdlib | -ffreestanding)
				STDLIB=false
				FPIE=false
				PIE=false
				;;
			-fno-PIC | -fno-pic | -fno-PIE | -fno-pie | \
			-nopie | -static | -shared | \
			-D__KERNEL__ | -nostartfiles)
				FPIE=false
				PIE=false
				;;
			-fPIC | -fpic)
				FPIE=false
				;;
			-O0)
				;;
			-O*)
				OPTIMIZE=true
				;;
			-ggdb3)
				FULL_DEBUG=true
				;;
			-Wl,-rpath,/*build-target*)
				if ! ${HOST}; then
					add_late_arg "-Wl,-rpath-link${ARG#-Wl,-rpath}"
				fi
				;;
			-fplugin=*)
				# The plugins are not available on the icecc node
				PTXDIST_ICECC=${PTXDIST_ICERUN}
				;;
			-save-temps*)
				# only icecc >= 1.4 filters those correctly
				PTXDIST_ICECC=${PTXDIST_ICERUN}
				;;
			-print-file-name=plugin)
				if [ "${PTXDIST_NO_GCC_PLUGINS}" = "1" ]; then
					echo "wrapper: gcc plugins disabled" >&2
					exit 1
				fi
				;;
			-|-print-search-dirs|--print-search-dirs)
				COMPILING=true
				LINKING=true
				;;
			-*)
				;;
			*)
				COMPILING=true
				;;
		esac
		case " ${pkg_flags_blacklist} " in
			*" ${ARG} "*)
				echo "wrapper: found blacklisted flag '${ARG}'" >&2
				exit 1
				;;
		esac
	done
	# Used e.g. by the kernel to get the compiler version. Adding
	# linker options confuses gcc because there is nothing to link.
	if ! $COMPILING; then
		LINKING=false
	fi
}

add_arg() {
	ARG_LIST="${ARG_LIST} ${*}"
}

add_late_arg() {
	LATE_ARG_LIST="${LATE_ARG_LIST} ${*}"
}

test_opt() {
	local opt="${1}" popt

	for item in ${pkg_wrapper_blacklist}; do
		if [ "${item}" = "${opt}" ]; then
			return 1
		fi
	done
	popt="PTXCONF_${opt}"
	eval "popt=\$${popt} opt=\$${opt}"
	if [ -z "${opt}" -a -z "${popt}" ]; then
		return 1
	fi
	return 0
}

add_opt_arg() {
	test_opt "${1}" || return 0
	shift
	add_arg "${@}"
}

add_late_opt_arg() {
	test_opt "${1}" || return 0
	shift
	add_late_arg "${@}"
}

add_host_arg() {
	case "${pkg_stamp}" in
		host-*|cross-*) add_arg "${@}";;
	esac
}

add_ld_args() {
	add_opt_arg TARGET_HARDEN_RELRO "${1}-z${2}relro"
	add_opt_arg TARGET_HARDEN_BINDNOW "${1}-z${2}now"
	add_opt_arg TARGET_LINKER_HASH_GNU "${1}--hash-style=gnu"
	add_opt_arg TARGET_LINKER_HASH_SYSV "${1}--hash-style=sysv"
	add_opt_arg TARGET_LINKER_HASH_BOTH "${1}--hash-style=both"
	add_opt_arg TARGET_LINKER_AS_NEEDED "${1}--as-needed"
	if ${BUILDID}; then
		add_opt_arg TARGET_BUILD_ID "${1}--build-id=sha1"
	fi
	if ! ( PTXCONF_TARGET_BUILD_ID=y test_opt TARGET_BUILD_ID ); then
		add_arg "${1}--build-id=none"
	fi
}

ld_add_ld_args() {
	# ld warns about ignored keywords (-z <keyword>) when only asked for the
	# version
	if [ "$*" != "-v" ]; then
		add_ld_args
	fi
}

cc_add_target_ld_args() {
	if ${LINKING}; then
		cc_check_args ${pkg_ldflags}
		add_ld_args "-Wl," ","
		add_late_arg ${PTXDIST_CROSS_LDFLAGS}
		add_arg ${pkg_ldflags}
		add_opt_arg TARGET_EXTRA_LDFLAGS ${PTXCONF_TARGET_EXTRA_LDFLAGS}
	fi
}

cc_add_host_ld_args() {
	if ${LINKING}; then
		cc_check_args ${pkg_ldflags}
		add_host_arg ${pkg_ldflags}
		add_late_arg ${PTXDIST_HOST_LDFLAGS}
	fi
}

cc_add_fortify() {
	# fortify only works when optimizing is enabled. The warning
	# generated when combining -D_FORTIFY_SOURCE with -O0 can confuse
	# configure checks
	if ${FORTIFY} && ${OPTIMIZE}; then
		add_opt_arg TARGET_HARDEN_FORTIFY "-D_FORTIFY_SOURCE=2"
	fi
}

cc_add_stack() {
	# TARGET_HARDEN_STACK blacklists all stack protector options
	( PTXCONF_TARGET_HARDEN_STACK=y test_opt TARGET_HARDEN_STACK ) || return 0
	if ${STDLIB}; then
		add_opt_arg TARGET_HARDEN_STACK "-fstack-protector" "--param=ssp-buffer-size=4"
		add_opt_arg TARGET_HARDEN_STACK_STRONG "-fstack-protector-strong"
		add_opt_arg TARGET_HARDEN_STACK_ALL "-fstack-protector-all"
	fi
	add_opt_arg TARGET_HARDEN_STACKCLASH "-fstack-clash-protection"
}

cc_add_pie() {
	if ${FPIE}; then
		add_opt_arg TARGET_HARDEN_PIE "-fPIE"
	fi
	if ${PIE}; then
		add_opt_arg TARGET_HARDEN_PIE "-pie"
	fi
}

cc_add_glibcxx() {
	add_opt_arg TARGET_HARDEN_GLIBCXX_ASSERTIONS "-D_GLIBCXX_ASSERTIONS"
}

cc_add_debug() {
	# TARGET_DEBUG is no real option but used to blacklist all debug options
	PTXCONF_TARGET_DEBUG=y test_opt TARGET_DEBUG || return 0
	add_late_opt_arg TARGET_DEBUG_OFF "-g0"
	add_late_opt_arg TARGET_DEBUG_ENABLE "-g"
	add_late_opt_arg TARGET_DEBUG_FULL "-ggdb3"
	if ${FULL_DEBUG} && ! test_opt TARGET_DEBUG_FULL && [ -n "${PTXDIST_ICECC}" ]; then
		# '-ggdb3' breaks reproducible builds with icecc
		# downgrade to '-g' unless full debugging is explicitly requested
		add_late_arg "-ggdb0 -g"
	fi
}

cc_add_arch() {
	if test_opt ARCH_ARM; then
		add_opt_arg ARCH_ARM_NEON "-mfpu=neon"
	fi
}

cc_add_optimizations() {
	add_opt_arg TARGET_NO_SEMANTIC_INTERPOSITION "-fno-semantic-interposition"
}

cpp_add_target_extra() {
	cc_check_args ${pkg_cppflags}
	add_opt_arg TARGET_COMPILER_RECORD_SWITCHES "-frecord-gcc-switches"
	add_opt_arg PTXDIST_Y2038 "-D_TIME_BITS=64 -D_FILE_OFFSET_BITS=64"
	add_late_arg ${PTXDIST_CROSS_CPPFLAGS}
	add_arg ${pkg_cppflags}
	add_opt_arg TARGET_EXTRA_CPPFLAGS ${PTXCONF_TARGET_EXTRA_CPPFLAGS}
}

cc_add_target_extra() {
	cc_check_args ${pkg_cflags}
	cpp_add_target_extra
	cc_add_optimizations
	cc_add_debug
	cc_add_arch
	add_arg ${pkg_cflags}
	add_opt_arg TARGET_EXTRA_CFLAGS ${PTXCONF_TARGET_EXTRA_CFLAGS}
}

cxx_add_target_extra() {
	cc_check_args ${pkg_cxxflags}
	cpp_add_target_extra
	cc_add_optimizations
	cc_add_debug
	cc_add_arch
	add_arg ${pkg_cxxflags}
	add_opt_arg TARGET_EXTRA_CXXFLAGS ${PTXCONF_TARGET_EXTRA_CXXFLAGS}
}

cc_add_target_reproducible() {
	add_arg -fdebug-prefix-map="${PTXDIST_PLATFORMDIR%/*}/="
	add_arg -fdebug-prefix-map="${WRAPPER_DIR%/sysroot-host/lib/wrapper}/=${PTXDIST_PLATFORMDIR##*/}/"
}

cpp_add_host_extra() {
	cc_check_args ${pkg_cppflags}
	add_arg ${PTXDIST_HOST_CPPFLAGS}
	add_host_arg ${pkg_cppflags}
}

cc_add_host_extra() {
	cc_check_args ${pkg_cflags}
	cpp_add_host_extra
	add_host_arg ${pkg_cflags}
}

cxx_add_host_extra() {
	cc_check_args ${pkg_cxxflags}
	cpp_add_host_extra
	add_host_arg ${pkg_cxxflags}
}

cc_add_host_clang() {
	add_arg --gcc-toolchain=/usr
}

add_icecc_args() {
	if [ -n "${PTXDIST_ICECC}" ]; then
		if [ "${1}" != clang ]; then
			add_late_arg "-fno-diagnostics-show-caret"
			add_late_arg "-gno-record-gcc-switches"
		fi
		add_late_arg "-Wno-implicit-fallthrough"
	fi
}

clang_check_target_icecc() {
	if [ "${PTXDIST_ICECC_CLANG}" != 1 ]; then
	    PTXDIST_ICECC=${PTXDIST_ICERUN}
	fi
}

clang_check_host_icecc() {
	if [ "${PTXDIST_ICECC_HOST_CLANG}" != 1 ]; then
	    PTXDIST_ICECC=${PTXDIST_ICERUN}
	fi
}

cc_add_target_icecc() {
	add_icecc_args "${@}"
	export ICECC_VERSION="$(echo ${PTXDIST_ICECC_DIR}/target/*.tar.gz)"
	export ICECC_CC="${FULL_CMD}"
}

cxx_add_target_icecc() {
	add_icecc_args "${@}"
	export ICECC_VERSION="$(echo ${PTXDIST_ICECC_DIR}/target/*.tar.gz)"
	export ICECC_CXX="${FULL_CMD}"
}

cc_add_host_icecc() {
	add_icecc_args "${@}"
	export ICECC_VERSION="$(echo ${PTXDIST_ICECC_DIR}/host/*.tar.gz)"
	export ICECC_CC="${FULL_CMD}"
}

cxx_add_host_icecc() {
	add_icecc_args "${@}"
	export ICECC_VERSION="$(echo ${PTXDIST_ICECC_DIR}/host/*.tar.gz)"
	export ICECC_CXX="${FULL_CMD}"
}

cc_add_target_clang() {
	triple="${CMD%-*}"
	if [ -n "${PTXDIST_SYSROOT_TOOLCHAIN}" ]; then
		add_arg --sysroot="${PTXDIST_SYSROOT_TOOLCHAIN}"
	fi
	env="${FULL_CMD%/*}/.${triple}.flags"
	if [ -e "${env}" ]; then
		. "${env}"
		add_arg ${flags}
	else
		add_arg --target ${triple}
	fi
}
