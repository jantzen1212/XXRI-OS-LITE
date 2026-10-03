# xxri OS Lite design system - session environment
#
# Don't export GTK_THEME or XCURSOR_THEME here: they would override the
# Settings choices (xxri-desktop theme/appearance/cursor), which live in
# ~/.config/gtk-3.0/settings.ini, ~/.gtkrc-2.0, ~/.Xdefaults and
# ~/.icons/default/index.theme - that's why Appearance once did nothing.
# System defaults: /usr/local/etc/xdg/gtk-3.0/settings.ini and
# /usr/local/share/icons/default/index.theme.
export XCURSOR_PATH="${XCURSOR_PATH:-$HOME/.icons:/usr/local/share/icons:/usr/share/icons}"
export XCURSOR_SIZE=24
export QT_QPA_PLATFORMTHEME=qt5ct
export QT_STYLE_OVERRIDE=Fusion
export XDG_DATA_DIRS="${XDG_DATA_DIRS:-/usr/local/share:/usr/share}"
# Settings scale (UI_SCALE, percent). GTK3 on X11 ignores gtk-xft-dpi in
# settings.ini and there's no XSETTINGS daemon, so GDK_DPI_SCALE it is (new
# session needed). XXRI stylesheets use pt so they follow; dock, CC and
# launcher are px and don't.
_xs=$(sed -n 's/^UI_SCALE=\([0-9]*\)$/\1/p' /usr/local/etc/xxri/desktop.conf 2>/dev/null | head -1)
case "$_xs" in
	125) export GDK_DPI_SCALE=1.25 ;; 150) export GDK_DPI_SCALE=1.5 ;;
	175) export GDK_DPI_SCALE=1.75 ;; 200) export GDK_DPI_SCALE=2 ;;
	*) unset GDK_DPI_SCALE ;;
esac
unset _xs
