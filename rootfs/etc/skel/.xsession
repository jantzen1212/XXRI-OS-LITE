# Screen resolution: honour an `xres=WxH` boot code (e.g. xres=1366x768) so
# the desktop adapts to the panel; default to 1024x768 when unset/invalid.
XRES=$(sed -n 's/.*\bxres=\([0-9]\{3,\}x[0-9]\{3,\}\).*/\1/p' /proc/cmdline 2>/dev/null | head -1)
[ -n "$XRES" ] || XRES=1024x768
# X server: prefer the real X.Org server.  Xvesa (TinyX) implements neither
# XInput2 nor XKB, RandR or MIT-SHM, and modern toolkits depend on them -
# Chromium/Electron refuses to map a window without XInput2 ("Xlib: extension
# XInputExtension missing"), and Qt logs XKeyboard/XRandr/xcb_shm failures.
# Xvesa stays as the fallback so a machine X.Org cannot drive still boots.
start_xorg() {
	command -v Xorg >/dev/null 2>&1 || return 1
	# An X.Org with no input driver gives a desktop with no mouse and no
	# keyboard, which is far worse than Xvesa.  Refuse to use it unless a real
	# input driver is present (the server ships only inputtest_drv.so, a stub).
	[ -e /usr/local/lib/xorg/modules/input/libinput_drv.so ] ||
	[ -e /usr/local/lib/xorg/modules/input/evdev_drv.so ] || return 1
	# hw-init modprobes the KMS driver, but the device node is created
	# asynchronously and X.Org exits immediately with "open /dev/dri/card0: No
	# such file or directory" if it starts first.  Wait briefly for it.
	for i in $(seq 1 24); do
		[ -e /dev/dri/card0 ] && break
		sleep 0.25
	done
	[ -e /dev/dri/card0 ] || return 1
	# -dpi 75 matches what Xvesa reported (it advertised 347mm x 260mm for every
	# mode, i.e. exactly 75dpi at 1024x768).  X.Org otherwise defaults to 96dpi,
	# which scales every font and widget up ~28% and changes the whole desktop
	# layout the design system was built against.
	# Font path on the SERVER COMMAND LINE, not patched in afterwards with
	# `xset +fp`.  The window manager draws its titlebar with FLTK's core-X-font
	# path, so it asks the server for "-xxri-helvetica-bold-r-normal--*"; if that
	# foundry is not in the font path at the moment the titlebar is first drawn,
	# every window title silently falls back to a bitmap face.  Doing it here
	# removes the ordering question entirely - the server has never served a
	# single request without the XXRI face.  These are X.Org's own compiled-in
	# defaults with ours prepended, minus 75dpi: its bitmap helvetica wins
	# FLTK's exact-size match against the scalable face and undoes the point.
	XXRI_FP=/usr/local/share/fonts/xxri/,/usr/local/lib/X11/fonts/misc,/usr/local/lib/X11/fonts/TTF,/usr/local/lib/X11/fonts/Type1,built-ins
	Xorg -nolisten tcp -novtswitch -sharevts vt7 -dpi 75 -fp "$XXRI_FP" >/tmp/xorg.log 2>&1 &
	export XPID=$!
	for i in $(seq 1 40); do
		[ -e /tmp/.X11-unix/X0 ] && { waitforX && return 0; }
		kill -0 "$XPID" 2>/dev/null || return 1
		sleep 0.25
	done
	kill "$XPID" 2>/dev/null
	return 1
}
# X.Org picks the monitor's preferred mode, which is not what Xvesa did - it
# always used $XRES (the xres= boot code, default 1024x768) and the desktop
# layout is designed around that.  Put the resolution back under our control.
apply_xres() {
	command -v xrandr >/dev/null 2>&1 || return 0
	out=""
	# RandR can still be settling when the session gets here: the output may
	# not be listed yet, or the mode list may not yet carry $XRES.  Asking once
	# and giving up meant intermittently dropping to the panel's preferred mode
	# instead - the desktop would come up 1280x800 when it was told 1024x768,
	# which moves the dock and every window the layout was designed around.
	for _try in 1 2 3 4 5 6 7 8 9 10; do
		out=$(xrandr 2>/dev/null | awk '/ connected/{print $1; exit}')
		[ -n "$out" ] && xrandr 2>/dev/null | grep -q "[ ]${XRES}[ ]" && break
		sleep 0.3
	done
	[ -n "$out" ] || return 0
	for _try in 1 2 3; do
		xrandr --output "$out" --mode "${XRES%x*}x${XRES#*x}" 2>/dev/null && return 0
		sleep 0.4
	done
	# only now settle for whatever the display says it prefers
	xrandr --output "$out" --preferred 2>/dev/null
}
start_xvesa() {
	Xvesa -br -screen ${XRES}x32 -shadow -2button -mouse /dev/input/mice,5 -fp /usr/local/share/fonts/xxri/,/usr/local/lib/X11/fonts/misc/ -nolisten tcp -I >/dev/null 2>&1 &
	export XPID=$!
	waitforX
}
XSERVER=xorg
start_xorg || { XSERVER=xvesa; start_xvesa || ! echo failed in waitforX || exit; }
export XSERVER
[ "$XSERVER" = xorg ] && apply_xres
# The XXRI face is published to the X server as the "xxri" foundry.  Xvesa
# gets it through -fp above; X.Org needs it added to the running server, and
# without it every FLTK surface (window titles, dialogs, the desktop menu)
# silently falls back to a stock bitmap font.
[ "$XSERVER" = xorg ] && command -v xset >/dev/null 2>&1 && {
	xset +fp /usr/local/share/fonts/xxri/ >/tmp/xset.log 2>&1
	xset fp rehash >>/tmp/xset.log 2>&1
	xset q >>/tmp/xset.log 2>&1
}
# A maximized window stops above the dock instead of vanishing behind it.
# 44px icons + 2x2 padding + the 26px the dock is lifted off the bottom edge.
export XXRI_DOCK_RESERVE=78
# xxri OS Lite design system: the window manager is XXRI's own build of flwm
# (work/flwm-xxri) and carries the palette internally, so it needs no colour
# switches.  Stock flwm is kept as flwm-stock for recovery.
XXRI_WM_ARGS=""
"$DESKTOP" $XXRI_WM_ARGS 2>/tmp/wm_errors &
export WM_PID=$!
# XXRI compositor: real per-window alpha for the system surfaces.
#
# The target does have what this needs - the server offers ARGB visuals and
# grants an overlay window - and the compositor is damage-driven, so it costs
# nothing when nothing is changing (measured: 0.8% idle, 1.9MB resident).
# Redirection belongs to this client, so if it ever dies the X server
# unredirects and the desktop simply goes back to painting itself: a crash
# costs translucency, not the session.  Boot with `nocomposite` to skip it.
if ! grep -qw nocomposite /proc/cmdline 2>/dev/null; then
	if [ -x /usr/local/bin/xxri-compositor ]; then
		pkill -f "/usr/local/bin/xxri-compositor" || true
		sleep 0.2
		/usr/local/bin/xxri-compositor 2>/var/tmp/xxri-compositor.nohup.log &
	fi
fi
# Desktop runtime every GTK/Qt application expects to find. Without these a
# real app starts, fails silently and never maps a window: Qt needs
# XDG_RUNTIME_DIR and a shared-memory segment, and D-Bus refuses to start at
# all when there is no machine ID, which costs Qt apps their entire interface.
[ -x /usr/local/bin/xxri-session-runtime ] && . /usr/local/bin/xxri-session-runtime
# TEMPORARY: trace the Store when it is launched from the dock (no terminal).
export XXRI_STORE_DEBUG=1
[ -x $HOME/.setbackground ] && $HOME/.setbackground
[ -x $HOME/.mouse_config ] && $HOME/.mouse_config &
[ $(which "$ICONS".sh) ] && ${ICONS}.sh &
# Dock running indicators: the small macOS-style dash under every icon whose
# application has a window open.  It is a separate process on purpose - the
# dock itself is a launcher with no idea what is running, and keeping the
# indicator out of it means a crash here can never take the dock down.
[ -x /usr/local/bin/xxri-wctl ] && /usr/local/bin/xxri-wctl indicators &
# XXRI Control Center: the always-available quick-settings chip in the
# bottom-right corner (click it to open the panel).  It is a single-instance
# override-redirect window, so it never joins the task list or takes focus.
[ -x /usr/local/bin/xxri-control-center ] && /usr/local/bin/xxri-control-center &
[ -d "/usr/local/etc/X.d" ] && find "/usr/local/etc/X.d" -type f -o -type l | sort | while read F; do . "$F"; done
[ -d "$HOME/.X.d" ] && find "$HOME/.X.d" -type f -o -type l | sort | while read F; do . "$F"; done
