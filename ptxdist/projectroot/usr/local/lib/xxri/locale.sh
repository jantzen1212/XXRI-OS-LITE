# locale.sh - locale names and the X keyboard layout, sourced by xxri-config,
# xxri-desktop and xxri-input.  No forks: it runs during boot.
#
# Only UTF-8 locales are installed (xxri-locales), so "pt_PT" and
# "pt_PT.utf8" both mean "pt_PT.UTF-8".

XXRI_LOCALE_DIR=/usr/lib/locale
XXRI_LOCALE_DEFAULT=en_US.UTF-8
# categories /etc/sysconfig/language may set besides LANG
XXRI_LC_VARS="LC_ALL LC_CTYPE LC_NUMERIC LC_TIME LC_COLLATE LC_MONETARY LC_MESSAGES LC_PAPER LC_NAME LC_ADDRESS LC_TELEPHONE LC_MEASUREMENT LC_IDENTIFICATION"

# xxri_locale_norm NAME: set XXRI_LOCALE to the canonical spelling of NAME
xxri_locale_norm() {
	case "$1" in
	'') XXRI_LOCALE=C; return ;;
	C|POSIX) XXRI_LOCALE=$1; return ;;
	esac
	_n=${1%%@*}; _m=
	[ "$_n" = "$1" ] || _m=@${1#*@}
	_l=${_n%%.*}
	if [ "$_l" = "$_n" ]; then _c=UTF-8; else _c=${_n#*.}; fi
	case "$_c" in [Uu][Tt][Ff]-8|[Uu][Tt][Ff]8) _c=UTF-8 ;; esac
	XXRI_LOCALE=$_l.$_c$_m
}

# xxri_locale_ok NAME: true if the locale is installed (C and POSIX always are)
xxri_locale_ok() {
	case "$1" in ''|C|POSIX) return 0 ;; esac
	xxri_locale_norm "$1"
	_n=${XXRI_LOCALE%%@*}; _m=
	[ "$_n" = "$XXRI_LOCALE" ] || _m=@${XXRI_LOCALE#*@}
	case "${_n#*.}" in UTF-8) _c=utf8 ;; *) _c=${_n#*.} ;; esac
	[ -d "$XXRI_LOCALE_DIR/${_n%%.*}.$_c$_m" ]
}

# xxri_locales: C and the installed locales, one per line
xxri_locales() {
	echo C
	for _d in "$XXRI_LOCALE_DIR"/*.utf8*; do
		[ -d "$_d" ] || continue
		_b=${_d##*/}
		echo "${_b%%.*}.UTF-8${_b#*.utf8}"
	done
}

# ---------------------------------------------------------------- X keyboard
# The X server's XKB layout ("pt", "pt:nodeadkeys"), separate from the console
# keymap.  Xorg reads it from an InputClass file, which has to exist before X
# starts; plain us, the default, needs none.

XXRI_XKB_DIR=/usr/local/share/X11/xkb
XXRI_XKB_CONF=/etc/X11/xorg.conf.d/00-xxri-keyboard.conf

# xxri_xkb_ok LAYOUT [VARIANT]: LAYOUT is installed (checks each of "a,b")
xxri_xkb_ok() {
	case "$1" in ''|*[!a-z0-9_,]*) return 1 ;; esac
	case "$2" in *[!A-Za-z0-9_,-]*) return 1 ;; esac
	_rest=$1
	while [ -n "$_rest" ]; do
		_one=${_rest%%,*}
		[ -f "$XXRI_XKB_DIR/symbols/$_one" ] || return 1
		case "$_rest" in *,*) _rest=${_rest#*,} ;; *) _rest= ;; esac
	done
}

# xxri_xkb_apply LAYOUT [VARIANT]: write the file, or remove it for plain us
xxri_xkb_apply() {
	if [ "$1" = us ] && [ -z "$2" ]; then rm -f "$XXRI_XKB_CONF"; return 0; fi
	mkdir -p "${XXRI_XKB_CONF%/*}" || return 1
	{
		echo 'Section "InputClass"'
		echo '    Identifier "xxri keyboard layout"'
		echo '    MatchIsKeyboard "on"'
		echo "    Option \"XkbLayout\" \"$1\""
		[ -z "$2" ] || echo "    Option \"XkbVariant\" \"$2\""
		echo 'EndSection'
	} > "$XXRI_XKB_CONF"
}
