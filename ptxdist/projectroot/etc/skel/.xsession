# Resolution: xres=WxH boot code, then Tiny Core's xvesa=WxHxD (old boot
# menus still pass it), then Settings > Display, then 1024x768.
XRES=$(sed -n 's/.*\bxres=\([0-9]\{3,\}x[0-9]\{3,\}\).*/\1/p' /proc/cmdline 2>/dev/null | head -1)
[ -n "$XRES" ] || XRES=$(sed -n 's/.*\bxvesa=\([0-9]\{3,\}x[0-9]\{3,\}\).*/\1/p' /proc/cmdline 2>/dev/null | head -1)
XRATE=
if [ -z "$XRES" ]; then
	XRES=$(sed -n 's/^MODE=\([0-9]\{3,\}x[0-9]\{3,\}\).*/\1/p' /usr/local/etc/xxri/display.conf 2>/dev/null | head -1)
	# and the rate saved with it
	[ -n "$XRES" ] && XRATE=$(sed -n 's/^RATE=\([0-9]\{1,3\}\(\.[0-9]\{1,2\}\)\{0,1\}\)$/\1/p' /usr/local/etc/xxri/display.conf 2>/dev/null | head -1)
fi
[ -n "$XRES" ] || XRES=1024x768
# X servers (XLibre), best first:
#   Xorg         modesetting on KMS: XInput2, DRI3, glamor, GLX - what
#                Chromium/Electron and Qt need (Electron needs XInput2)
#   Xfbdev       kdrive on the firmware/fbdev framebuffer, software only;
#                fallback when DRM isn't available
#   Xorg + vesa  video BIOS, BIOS machines with no kernel display device
# Xorg without an input driver means no mouse or keyboard, worse than Xfbdev.
xorg_usable() {
	command -v Xorg >/dev/null 2>&1 || return 1
	for f in /usr/local/lib/xorg/modules/xlibre-25/drivers/input/*_drv.so \
	         /usr/local/lib/xorg/modules/input/*_drv.so; do
		[ -e "$f" ] && return 0
	done
	return 1
}
# Tiny Core's Mesa needs SSE2 (libgallium) and CMOV (libEGL): both X servers
# died with SIGILL on emulated Pentium/Pentium III. Without SSE2, Xorg runs
# without GLX/glamor and Xfbdev with -extension GLX.
nosse2() { ! grep -qw sse2 /proc/cpuinfo; }
# Pre-GeForce 8 nouveau on the primary display (NV04..NV4x, nForce, C51-C68):
# no usable Mesa driver, glamor misrenders, so modesetting without glamor
# (xxri-noaccel-kms.conf; GLX via llvmpipe). Family decoded from nouveau's
# "NVIDIA <chip> (<boot0>)" probe line, as nvkm_device_ctor() does (6.12).
# Checked here, not in hw-init: the DRM device registers asynchronously.
legacy_nv() {
	for c in /sys/class/drm/card[0-9] /sys/class/drm/card[0-9][0-9]; do
		[ "$(cat "$c/device/boot_vga" 2>/dev/null)" = 1 ] || continue
		case "$(readlink "$c/device/driver")" in */nouveau) ;; *) return 1 ;; esac
		pci=$(basename "$(readlink -f "$c/device")")
		b0=$(dmesg | sed -n "s/.*nouveau $pci: NVIDIA [^ ]* (\([0-9a-f]\{8\}\))\$/\1/p" | head -n 1)
		[ -n "$b0" ] || return 1
		b=$((0x$b0))
		[ $((b & 0x1f000000)) -eq 0 ] && return 0	# NV04/NV05: Riva TNT, TNT2
		case $(( (b >> 20) & 0x1f0 )) in
		16|32|48|64|96) return 0 ;;	# NV1x, NV2x, NV3x, NV4x, C6x/C7x
		esac
		return 1
	done
	return 1
}
start_xorg() {
	xorg_usable || return 1
	# Don't wait for /dev/dri/card0 itself: the node appears before the minor
	# is published, and an open() in between is ENODEV ("no screens found"
	# on the NEC VK24MX, i915 otherwise fine). Connectors register only after
	# the minors, so a card*-<output> sysfs entry means it's openable. Don't
	# probe by opening it either, that races Xorg for DRM master.
	kms_ready() {
		for c in /sys/class/drm/card*-*/status; do
			[ -e "$c" ] && return 0
		done
		return 1
	}
	for i in $(seq 1 60); do
		kms_ready && break
		sleep 0.25
	done
	kms_ready || return 1
	XORG_CFG=""
	if nosse2; then XORG_CFG="-config xxri-nosse2-kms.conf"
	elif legacy_nv; then XORG_CFG="-config xxri-noaccel-kms.conf"
	fi
	run_xorg
}
# -dpi 75: what Xvesa reported; the layout was designed at it (Xorg's 96dpi
# blows everything up ~28%).
# Font path on the command line, not `xset +fp` later: flwm asks for
# -xxri-helvetica-bold-* when it first draws a titlebar and falls back to a
# bitmap face if the foundry isn't there yet. Xorg's default path with ours
# first, minus 75dpi (its bitmap helvetica beats the scalable face in FLTK's
# exact-size match).
run_xorg() {
	XXRI_FP=/usr/local/share/fonts/xxri/,/usr/local/lib/X11/fonts/misc,/usr/local/lib/X11/fonts/TTF,/usr/local/lib/X11/fonts/Type1,built-ins
	Xorg -nolisten tcp -novtswitch -sharevts vt7 -dpi 75 $XORG_CFG -fp "$XXRI_FP" >/tmp/xorg.log 2>&1 &
	export XPID=$!
	for i in $(seq 1 40); do
		[ -e /tmp/.X11-unix/X0 ] && { waitforX && return 0; }
		kill -0 "$XPID" 2>/dev/null || return 1
		sleep 0.25
	done
	kill "$XPID" 2>/dev/null
	return 1
}
# Xorg + vesa (VBE through x86emu): BIOS machines left with neither KMS nor
# /dev/fb0, i.e. a KMS driver kicked vesafb out and then failed (missing
# firmware, rejected hardware). What Xvesa used to cover. No VBE on UEFI.
start_xorg_vesa() {
	# a registered framebuffer, not the node: core.gz carries a static /dev/fb0
	[ -e /sys/class/graphics/fb0 ] && return 1
	[ -d /sys/firmware/efi ] && return 1
	[ -e /usr/local/lib/xorg/modules/xlibre-25/drivers/vesa_drv.so ] || return 1
	xorg_usable || return 1
	XORG_CFG=""; nosse2 && XORG_CFG="-config xxri-nosse2.conf"
	run_xorg
}
# Xorg picks the monitor's preferred mode; the layout expects $XRES.
apply_xres() {
	command -v xrandr >/dev/null 2>&1 || return 0
	out=""
	# RandR may still be settling (output or mode not listed yet); asking
	# once sometimes came up 1280x800 instead of 1024x768
	for _try in 1 2 3 4 5 6 7 8 9 10; do
		out=$(xrandr 2>/dev/null | awk '/ connected/{print $1; exit}')
		[ -n "$out" ] && xrandr 2>/dev/null | grep -q "[ ]${XRES}[ ]" && break
		sleep 0.3
	done
	[ -n "$out" ] || return 0
	for _try in 1 2 3; do
		xrandr --output "$out" --mode "${XRES%x*}x${XRES#*x}" ${XRATE:+--rate "$XRATE"} 2>/dev/null && return 0
		# a saved rate this output no longer offers must not cost the saved size
		XRATE=
		sleep 0.4
	done
	# only now settle for whatever the display says it prefers
	xrandr --output "$out" --preferred 2>/dev/null
}
# Xfbdev: vesafb/efifb or a legacy fbdev driver, console keyboard and
# /dev/input/mice like Xvesa; the mode is the framebuffer's.
start_xfbdev() {
	[ -e /sys/class/graphics/fb0 ] || return 1
	command -v Xfbdev >/dev/null 2>&1 || return 1
	XXRI_FP=/usr/local/share/fonts/xxri/,/usr/local/lib/X11/fonts/misc,/usr/local/lib/X11/fonts/TTF,/usr/local/lib/X11/fonts/Type1,built-ins
	# GLX loads Tiny Core's Mesa (i686, SSE2): not on a processor without SSE2
	noglx=""; nosse2 && noglx="-extension GLX"
	# mousedev scales absolute pointers (VM tablet, touchscreen) to 1024x768
	# unless told otherwise; on other sizes two screen edges are unreachable.
	# Tell it the framebuffer size. (Xorg uses libinput, unaffected.)
	_fb=$(sed -n '1s/^[A-Za-z]*:\([0-9]*\)x\([0-9]*\).*/\1 \2/p' /sys/class/graphics/fb0/modes 2>/dev/null)
	[ -n "$_fb" ] || _fb=$(tr ',' ' ' < /sys/class/graphics/fb0/virtual_size 2>/dev/null)
	if [ -n "$_fb" ] && [ -e /sys/module/mousedev/parameters/xres ]; then
		sudo sh -c "echo ${_fb% *} > /sys/module/mousedev/parameters/xres
			echo ${_fb#* } > /sys/module/mousedev/parameters/yres" 2>/dev/null
	fi
	Xfbdev -nolisten tcp -br -2button -dpi 75 $noglx -fp "$XXRI_FP" >/tmp/xfbdev.log 2>&1 &
	export XPID=$!
	waitforX
}
XSERVER=xorg
# With a KMS device Xorg gets a second try before Xfbdev (the device can
# still be settling, and fb0 there is just the KMS driver's emulation).
start_xorg || { { [ -e /dev/dri/card0 ] || [ -e /dev/dri/card1 ]; } && sleep 2 && start_xorg; } ||
	{ XSERVER=xfbdev; start_xfbdev; } ||
	{ XSERVER=xorg-vesa; start_xorg_vesa || ! echo failed in waitforX || exit; }
export XSERVER
[ "$XSERVER" = xfbdev ] || apply_xres
# No `xset +fp` / `fp rehash` here: -fp above covers it, and that block cost
# 0.62s of boot for a pixel-identical result. Re-measure before re-adding.
# Maximized windows stop above the dock: icon size (DOCK_SIZE from Settings,
# default 40) plus padding and gap.
_ds=$(sed -n 's/^DOCK_SIZE=\([0-9]*\)$/\1/p' /usr/local/etc/xxri/desktop.conf 2>/dev/null | head -1)
case "$_ds" in 3[2-9]|4[0-9]|5[0-6]) ;; *) _ds=40 ;; esac
export XXRI_DOCK_RESERVE=$((_ds + 38))
# XXRI's flwm (rule xxri-flwm) has the palette built in, no switches needed.
# Stock flwm is kept as flwm-stock for recovery.
# `noanim` boot code: no motion - flwm's minimise flight, the compositor's
# fade-in
grep -qw noanim /proc/cmdline 2>/dev/null && export XXRI_NO_ANIM=1
XXRI_WM_ARGS=""
# Root cursor now, so a late or missing WM never leaves the "X" pointer on
# the root window and on windows with no cursor of their own (dock, installer).
xsetroot -cursor_name left_ptr 2>/dev/null
# DESKTOP comes from /etc/sysconfig/desktop, which flwm.tcz used to write.
# XXRI's flwm is in core.gz, so live sessions once got DESKTOP="" and no WM.
command -v "${DESKTOP:-}" >/dev/null 2>&1 || DESKTOP=flwm
export DESKTOP
"$DESKTOP" $XXRI_WM_ARGS 2>/tmp/wm_errors &
export WM_PID=$!
# Compositor for real alpha on the system surfaces (damage-driven: 0.8% idle,
# 1.9MB). If it dies X unredirects and the desktop just loses translucency.
# `nocomposite` boot code skips it.
# written by the compositor once it has redirected and painted
export XXRI_COMP_READY="/tmp/.xxri-compositor-ready.$(id -u)"
rm -f "$XXRI_COMP_READY"
COMP_PID=""
if ! grep -qw nocomposite /proc/cmdline 2>/dev/null; then
	if [ -x /usr/local/bin/xxri-compositor ]; then
		# a session restart replaces the old one; it owns the CM selection
		# until its process is gone, and the new one would refuse to start
		old="$(pidof xxri-compositor 2>/dev/null)"
		if [ -n "$old" ]; then
			kill $old 2>/dev/null
			for _i in $(seq 40); do
				kill -0 $old 2>/dev/null || break
				sleep 0.05
			done
		fi
		/usr/local/bin/xxri-compositor 2>/var/tmp/xxri-compositor.nohup.log &
		COMP_PID=$!
	fi
fi
# XDG_RUNTIME_DIR, /dev/shm, machine-id, D-Bus: without them Qt apps start
# and never map a window.
[ -x /usr/local/bin/xxri-session-runtime ] && . /usr/local/bin/xxri-session-runtime
# XXRI File's rail hides a place whose folder doesn't exist, so make them
# (no Desktop: there is no desktop-icon layer).
for d in Documents Downloads Pictures Music Videos; do
	[ -d "$HOME/$d" ] || mkdir -p "$HOME/$d" 2>/dev/null
done
if [ ! -f "$HOME/.config/user-dirs.dirs" ]; then
	mkdir -p "$HOME/.config" 2>/dev/null
	{
		echo 'XDG_DOCUMENTS_DIR="$HOME/Documents"'
		echo 'XDG_DOWNLOAD_DIR="$HOME/Downloads"'
		echo 'XDG_MUSIC_DIR="$HOME/Music"'
		echo 'XDG_PICTURES_DIR="$HOME/Pictures"'
		echo 'XDG_VIDEOS_DIR="$HOME/Videos"'
	} > "$HOME/.config/user-dirs.dirs" 2>/dev/null
fi
[ -x $HOME/.setbackground ] && $HOME/.setbackground
[ -x $HOME/.mouse_config ] && $HOME/.mouse_config &
[ $(which "$ICONS".sh) ] && ${ICONS}.sh &
# The always-on surfaces start once the compositor has redirected: an
# override-redirect window that paints before XCompositeRedirectSubwindows
# stays blank all session (the Control Center did once boot got fast).
# It says so with XXRI_COMP_READY. If it dies first (no Composite, another
# compositor) X never redirects and there is nothing to wait for; the bound
# only guards against a compositor that hangs without ever painting.
if [ -n "$COMP_PID" ]; then
	for _i in $(seq 200); do
		[ -e "$XXRI_COMP_READY" ] && break
		kill -0 "$COMP_PID" 2>/dev/null || break
		sleep 0.05
	done
fi
# dock running dots, kept out of wbar so a crash can't take the dock down
[ -x /usr/local/bin/xxri-wctl ] && /usr/local/bin/xxri-wctl indicators &
# quick-settings chip, bottom right (override-redirect, never takes focus)
[ -x /usr/local/bin/xxri-control-center ] && /usr/local/bin/xxri-control-center &
# All Apps key left of the dock: its own surface, not a wbar slot; it finds
# wbar's window itself, so start order doesn't matter
[ -x /usr/local/bin/xxri-launcher ] && /usr/local/bin/xxri-launcher --button &
# running-but-unpinned apps go into wbar's config after a separator; this
# keeps that section in step with the open windows
[ -x /usr/local/bin/xxri-dock-pin ] && /usr/local/bin/xxri-dock-pin watch &
[ -d "/usr/local/etc/X.d" ] && find "/usr/local/etc/X.d" -type f -o -type l | sort | while read F; do . "$F"; done
[ -d "$HOME/.X.d" ] && find "$HOME/.X.d" -type f -o -type l | sort | while read F; do . "$F"; done
