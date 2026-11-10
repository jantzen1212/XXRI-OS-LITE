#!/bin/bash
# audio-mixer-test.sh - xxri-audio: first-boot mixer levels and the output
# selector, against a fake amixer and a fake /proc/asound.
#
#   dB-scaled controls (HD Audio)  -> 0dB + unmuted, never the old 70% (-28.5 dB)
#   controls without a dB scale    -> 70% + unmuted (AC97 behaviour unchanged)
#   output auto (default)          -> the driver's Auto-Mute Mode, both switches on
#   output speakers / headphones   -> Auto-Mute off, exactly one switch on,
#                                     remembered in audio.conf and re-applied at boot
#   a card without Speaker+Headphone controls -> nothing to choose, nothing touched
#
# Needs bwrap and a 32-bit Tiny Core busybox root (default: final-core).
# Nothing outside the temp work dir is touched.
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../.." && pwd)
PRJ=$ROOT/ptxdist/projectroot/usr/local
TC=${XXRI_TC_ROOT:-$HOME/.xxri-testenv/final-core}
WORK=$(mktemp -d "${TMPDIR:-/tmp}/audio-mixer.XXXXXX")
trap '[ -n "${KEEP:-}" ] || rm -rf "$WORK"' EXIT
pass=0; fail=0
ok() { # name condition-result(0 = good)
	if [ "$2" = 0 ]; then pass=$((pass + 1)); printf '  PASS  %s\n' "$1"
	else fail=$((fail + 1)); printf '  FAIL  %s\n' "$1"; fi
}

# fake amixer: one file per control in $AMX/ctl, "<name>" = "scale level mute"
cat > "$WORK/amixer" <<'EOF'
#!/bin/busybox ash
AMX=/amx
q=""; [ "$1" = -q ] && shift
shift 2   # -c N
verb=$1; name=$2; shift 2
f="$AMX/ctl/$name"
[ -f "$f" ] || exit 1
read scale level mute < "$f"
case "$verb" in
	sget|get)
		if [ "$scale" = enum ]; then
			echo "  Capabilities: enum"; echo "  Items: 'Disabled' 'Speaker Only' 'Line Out+Speaker'"
			echo "  Item0: '$(echo "$level" | tr _ ' ')'"; exit 0
		fi
		db=""; [ "$scale" = db ] && db=" [-28.50dB]"
		echo "  Front Left: Playback 89 [70%]$db [$mute]"; exit 0 ;;
esac
for a in "$@"; do
	case "$a" in
		unmute) mute=on ;;
		mute) mute=off ;;
		*dB) [ "$scale" = db ] || exit 1; level=$a ;;
		*%) level=$a ;;
		*) level=$(echo "$a" | tr ' ' _) ;;   # enum item (Auto-Mute Mode)
	esac
done
echo "$scale $level $mute" > "$f"
echo "$name $*" >> "$AMX/log"
EOF
cat > "$WORK/alsactl" <<'EOF'
#!/bin/busybox ash
exit 0
EOF
chmod +x "$WORK/amixer" "$WORK/alsactl"

# run_case NAME STATE CONF_OUTPUT CMD -- CTLS...   (CTL = name:scale)
run_case() {
	local n=$1 state=$2 conf=$3 cmd=$4; shift 5
	local d=$WORK/$n
	rm -rf "$d"; mkdir -p "$d/ctl" "$d/proc/card0" "$d/etc"
	local c
	for c in "$@"; do printf '%s %s %s\n' "${c#*:}" 0% off > "$d/ctl/${c%%:*}"; done
	[ -f "$d/ctl/Auto-Mute Mode" ] && echo "enum ${PRE_AM:-Line_Out+Speaker} on" > "$d/ctl/Auto-Mute Mode"
	: > "$d/log"
	printf ' 0 [PCH            ]: HDA-Intel - HDA Intel PCH\n                      HDA Intel PCH at 0xf7e00000 irq 30\n' > "$d/proc/cards"
	echo PCH > "$d/proc/card0/id"
	mkdir -p "$d/proc/card0/pcm0p"; printf 'id: %s\n' Analog > "$d/proc/card0/pcm0p/info"
	: > "$d/etc/audio.conf"; [ -n "$conf" ] && echo "OUTPUT=$conf" > "$d/etc/audio.conf"
	[ "$state" = 1 ] && : > "$d/etc/alsa.state"
	bwrap --unshare-user --uid 0 --gid 0 --overlay-src "$TC" --tmp-overlay / \
		--tmpfs /proc --tmpfs /sys --dev /dev \
		--bind "$d/proc/cards" /proc/asound/cards \
		--bind "$d/proc/card0" /proc/asound/card0 \
		--bind "$d" /amx \
		--bind "$d/etc" /usr/local/etc/xxri \
		--ro-bind "$PRJ/lib/xxri/hw-lib.sh" /usr/local/lib/xxri/hw-lib.sh \
		--ro-bind "$PRJ/bin/xxri-audio" /usr/local/bin/xxri-audio \
		--ro-bind "$WORK/amixer" /usr/bin/amixer --ro-bind "$WORK/alsactl" /usr/sbin/alsactl \
		--setenv PATH /usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin \
		/bin/busybox ash -c "/usr/local/bin/xxri-audio $cmd" > "$d/out" 2>&1
	RC=$?
}
lvl() { read -r _ l _ < "$WORK/$1/ctl/$2"; echo "$l" | tr _ ' '; }     # case control -> level
mut() { read -r _ _ m < "$WORK/$1/ctl/$2"; echo "$m"; }     # case control -> on/off
conf() { sed -n 's/^OUTPUT=//p' "$WORK/$1/etc/audio.conf"; }
HDA=(Master:db PCM:db Speaker:db Headphone:db "Auto-Mute Mode:enum")

echo "== HD Audio card, first boot"
run_case hda 0 "" init -- "${HDA[@]}"
ok "init exits 0" "$RC"
for m in Master PCM Speaker Headphone; do
	[ "$(lvl hda $m)" = 0dB ] && [ "$(mut hda $m)" = on ]; ok "$m -> 0dB, unmuted" $?
done
[ "$(lvl hda 'Auto-Mute Mode')" = "Line Out+Speaker" ]; ok "Auto-Mute Mode left at the driver default" $?

echo "== card without dB scale (AC97)"
run_case ac97 0 "" init -- Master:pct PCM:pct
for m in Master PCM; do
	[ "$(lvl ac97 $m)" = 70% ] && [ "$(mut ac97 $m)" = on ]; ok "$m -> 70%, unmuted (unchanged)" $?
done

echo "== output selector"
run_case outh 0 "" "output set headphones --json" -- "${HDA[@]}"
ok "set headphones exits 0" "$RC"
[ "$(mut outh Headphone)" = on ] && [ "$(mut outh Speaker)" = off ]; ok "headphones: Headphone on, Speaker off" $?
[ "$(lvl outh 'Auto-Mute Mode')" = Disabled ]; ok "headphones: Auto-Mute off" $?
[ "$(conf outh)" = headphones ]; ok "choice saved in audio.conf" $?
grep -q '"selected":"headphones"' "$WORK/outh/out" && grep -q '"available":true' "$WORK/outh/out"; ok "json: selected + available" $?
run_case outs 0 "" "output set speakers" -- "${HDA[@]}"
[ "$(mut outs Speaker)" = on ] && [ "$(mut outs Headphone)" = off ] && [ "$(lvl outs 'Auto-Mute Mode')" = Disabled ]; ok "speakers: Speaker on, Headphone off, Auto-Mute off" $?
# back to automatic from an explicit choice
run_case outa3 0 "" "output set headphones >/dev/null; /usr/local/bin/xxri-audio output set auto" -- "${HDA[@]}"
[ "$(lvl outa3 'Auto-Mute Mode')" = "Line Out+Speaker" ] && [ "$(mut outa3 Speaker)" = on ] && [ "$(mut outa3 Headphone)" = on ]; ok "auto: Auto-Mute back on, both switches on" $?
[ "$(conf outa3)" = auto ]; ok "auto saved" $?
run_case bad 0 "" "output set loudspeakers" -- "${HDA[@]}"
[ "$RC" != 0 ] && [ -z "$(conf bad)" ]; ok "unknown value refused, nothing saved" $?
run_case nosw 0 "" "output set headphones --json" -- Master:db PCM:db
grep -q '"available":false' "$WORK/nosw/out" && [ -z "$(grep . "$WORK/nosw/log")" ]; ok "no Speaker+Headphone controls: unavailable, nothing touched" $?

echo "== the choice survives a boot"
run_case boot 1 headphones init -- "${HDA[@]}"
[ "$(lvl boot 'Auto-Mute Mode')" = Disabled ] && [ "$(mut boot Speaker)" = off ] && [ "$(mut boot Headphone)" = on ]; ok "restored state + OUTPUT=headphones re-applied" $?
[ "$(lvl boot Speaker)" = 0% ]; ok "restored state: levels not re-primed" $?
# an older build saved Auto-Mute as disabled (and may have left a switch off); with
# nothing chosen, boot puts the driver's Auto-Mute back, so plugging in headphones or
# external speakers silences the built-in ones
PRE_AM=Disabled run_case bootold 1 "" init -- "${HDA[@]}"
[ "$(lvl bootold 'Auto-Mute Mode')" = "Line Out+Speaker" ] && [ "$(mut bootold Speaker)" = on ] && [ "$(mut bootold Headphone)" = on ]; ok "OUTPUT unset: a saved \"disabled\" Auto-Mute is switched back on at boot" $?
PRE_AM=Disabled run_case bootset 1 speakers init -- "${HDA[@]}"
[ "$(lvl bootset 'Auto-Mute Mode')" = Disabled ] && [ "$(mut bootset Headphone)" = off ]; ok "an explicit Speakers choice is not undone at boot" $?
run_case firstboot 0 "" init -- "${HDA[@]}"
[ "$(lvl firstboot 'Auto-Mute Mode')" = "Line Out+Speaker" ]; ok "first boot: Auto-Mute on (driver default)" $?

echo "== $pass passed, $fail failed"
[ "$fail" = 0 ]
