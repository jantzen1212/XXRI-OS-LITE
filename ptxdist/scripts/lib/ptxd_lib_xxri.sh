#!/bin/bash
#
# XXRI image helpers, sourced by PTXdist (project scripts/lib/ptxd_lib_*.sh).
#

#
# ptxd_make_xxri_extract - the selected packages' files, as installed, in
# ${pkg_dir} (inside the caller's fakeroot session), without the package
# manager's own bookkeeping
#
ptxd_make_xxri_extract() {
    ptxd_make_image_init &&
    ptxd_get_ipkg_files ${image_pkgs} &&
    ptxd_make_image_extract_xpkg_files "${pkg_dir}" &&
    rm -rf "${pkg_dir}/usr/lib/opkg" "${pkg_dir}/usr/share/opkg" "${pkg_dir}/var/lib/opkg" "${pkg_dir}/etc/opkg" &&
    # drop only the parents this left empty and opkg's lock directory (opkg
    # runs with the image as its offline root); packages may ship empty
    # directories on purpose (/opt/appimages)
    local d &&
    for d in usr/lib usr/share var/lib etc run/lock; do
	if [ -d "${pkg_dir}/${d}" ]; then
	    (cd "${pkg_dir}" && rmdir -p --ignore-fail-on-non-empty "${d}") || return
	fi
    done
}
export -f ptxd_make_xxri_extract

#
# core.gz: Tiny Core's initramfs + the XXRI packages
# $1: Tiny Core core.gz  $2: removal list
#
ptxd_make_xxri_core_impl() {
    ptxd_make_xxri_extract &&
    mkdir -p "${pkg_dir}" &&
    "${XXRI_HOST_PYTHON}" "${PTXDIST_WORKSPACE}/scripts/xxri-mkinitramfs.py" \
	"${1}" "${2}" "${pkg_dir}" "${image_image}" &&
    rm -rf "${pkg_dir}"
}
export -f ptxd_make_xxri_core_impl

ptxd_make_xxri_core() {
    fakeroot ptxd_make_xxri_core_impl "${@}"
}
export -f ptxd_make_xxri_core

#
# a Tiny Core extension (.tcz + .md5.txt) of the selected packages
# $1: onboot dependencies (.dep lines), may be empty
#
ptxd_make_xxri_tcz_impl() {
    ptxd_make_xxri_extract &&
    find "${pkg_dir}" -exec touch -h -d "@${SOURCE_DATE_EPOCH}" {} + &&
    rm -f "${image_image}" &&
    # timestamps: mksquashfs takes SOURCE_DATE_EPOCH (set by PTXdist) itself
    mksquashfs "${pkg_dir}" "${image_image}" -noappend -no-xattrs -comp xz > /dev/null &&
    ( cd "$(dirname "${image_image}")" &&
	md5sum "$(basename "${image_image}")" > "$(basename "${image_image}").md5.txt" ) &&
    if [ -n "${1}" ]; then
	printf '%s\n' ${1} > "${image_image}.dep"
    else
	rm -f "${image_image}.dep"
    fi &&
    rm -rf "${pkg_dir}"
}
export -f ptxd_make_xxri_tcz_impl

ptxd_make_xxri_tcz() {
    fakeroot ptxd_make_xxri_tcz_impl "${@}"
}
export -f ptxd_make_xxri_tcz
