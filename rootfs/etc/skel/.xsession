# Screen resolution: honour an `xres=WxH` boot code (e.g. xres=1366x768) so
# the desktop adapts to the panel; default to 1024x768 when unset/invalid.
XRES=$(sed -n 's/.*\bxres=\([0-9]\{3,\}x[0-9]\{3,\}\).*/\1/p' /proc/cmdline 2>/dev/null | head -1)
[ -n "$XRES" ] || XRES=1024x768
Xvesa -br -screen ${XRES}x32 -shadow -2button -mouse /dev/input/mice,5 -fp /usr/local/share/fonts/xxri/,/usr/local/lib/X11/fonts/misc/ -nolisten tcp -I >/dev/null 2>&1 &
export XPID=$!
waitforX || ! echo failed in waitforX || exit
# xxri OS Lite design system: flwm takes its palette from the standard FLTK
# switches (surface / ink / input-field colors, gtk+ scheme for soft bevels).
XXRI_WM_ARGS=""
[ "$DESKTOP" = "flwm" ] && XXRI_WM_ARGS="-bg #EFEDF8 -fg #1C1B24 -bg2 #FFFFFF -scheme gtk+"
"$DESKTOP" $XXRI_WM_ARGS 2>/tmp/wm_errors &
export WM_PID=$!
[ -x $HOME/.setbackground ] && $HOME/.setbackground
[ -x $HOME/.mouse_config ] && $HOME/.mouse_config &
[ $(which "$ICONS".sh) ] && ${ICONS}.sh &
[ -d "/usr/local/etc/X.d" ] && find "/usr/local/etc/X.d" -type f -o -type l | sort | while read F; do . "$F"; done
[ -d "$HOME/.X.d" ] && find "$HOME/.X.d" -type f -o -type l | sort | while read F; do . "$F"; done
