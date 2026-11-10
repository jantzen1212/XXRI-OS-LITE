#!/bin/bash
# store-nav-test.sh - the real XXRI Store GUI, driven with real X events, once per OS architecture.
#
# What runs: the GUI (built here against the host's GTK 3 as C++, like the rule does), the real
# xxri-store / xxri-app engines under a device busybox (its ELF header decides the OS
# architecture), a seeded catalog cache and a local HTTP server standing in for the repository.
# Xvfb + xdotool click and type; the GUI's own XXRI_STORE_DEBUG trace says which page opened.
#
#   store-nav-test.sh [GUI_SOURCE.c] [ENGINE_DIR]
#
#   XXRI_BB32 / XXRI_BB64   device busyboxes (see store-engine-test.sh)
#   XXRI_REPO_DIR           checkout of the repository (i686.json, x64.json)
#   XXRI_TEST_DISPLAY       X display number (default 77)
#   XXRI_TEST_ONLY=search   only the search-then-scroll crash check
#   XXRI_TEST_ONLY=slow     only the slow-engine check (set XXRI_TEST_SLOW=5: state commands take 5 s)
#   XXRI_TEST_LIBPATH       library path for the GUI process only (ld.so --library-path, so the
#                           tools it starts are not affected): run it against the device's own
#                           GTK (the unpacked gtk3 .tcz and its dependencies) instead of the host's
#
# Needs Xvfb, xdotool, ImageMagick (import, magick), bwrap, python3, g++ and gtk+-3.0.pc.
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../.." && pwd)
GUISRC=${1:-$ROOT/src/xxri-store/xxri-store-gui.c}
SRC=${2:-$ROOT/src/xxri-store/root/usr/local}
BB32=${XXRI_BB32:-$HOME/.xxri-testenv/final-core/bin/busybox}
BB64=${XXRI_BB64:-$HOME/.xxri-testenv/q/x64/vopt64/core/bin/busybox}
REPO=${XXRI_REPO_DIR:-$HOME/xxri-repo}
DISP=${XXRI_TEST_DISPLAY:-77}
PORT=${XXRI_TEST_PORT:-18150}
WORK=${XXRI_TEST_WORK:-$(mktemp -d "${TMPDIR:-/tmp}/store-nav.XXXXXX")}
GUIDIR=$(dirname "$GUISRC")
pass=0; fail=0
ok() { if [ "$2" = 0 ]; then pass=$((pass + 1)); printf '  PASS  %s\n' "$1"; else fail=$((fail + 1)); printf '  FAIL  %s %s\n' "$1" "${3:-}"; fi; }
cleanup() { kill $(jobs -p) 2>/dev/null; pkill -f "Xvfb :$DISP" 2>/dev/null; [ -z "${XXRI_TEST_WORK:-}" ] && rm -rf "$WORK"; }
trap cleanup EXIT
for f in "$BB32" "$BB64" "$REPO/i686.json" "$REPO/x64.json" "$GUISRC"; do [ -e "$f" ] || { echo "missing: $f" >&2; exit 2; }; done

echo "building the GUI ($GUISRC)"
# AddressSanitizer: a use-after-free that happens not to crash would pass otherwise. On for the
# search-and-scroll check (XXRI_TEST_ONLY=search); off for the full run, which it slows down
# enough to upset the navigation timings (XXRI_TEST_ASAN=1/0 to force it either way)
ASAN=""; [ "${XXRI_TEST_ASAN:-$([ "${XXRI_TEST_ONLY:-}" = search ] && echo 1 || echo 0)}" = 1 ] && ASAN="-fsanitize=address -fno-omit-frame-pointer -g"
mkdir -p "$WORK"
g++ -x c++ -fpermissive -w -O1 $ASAN $(pkg-config --cflags gtk+-3.0) -I"$GUIDIR" "$GUISRC" -o "$WORK/gui" $(pkg-config --libs gtk+-3.0) -lm || exit 2

# the repository: catalogs whose icons point back here, where they do not exist (404)
mkdir -p "$WORK/srv/icons"
python3 - "$REPO" "$WORK/srv" "http://127.0.0.1:$PORT" <<'EOF'
import json, sys
repo, out, base = sys.argv[1:4]
for f in ("i686", "x64"):
    d = json.load(open(f"{repo}/{f}.json"))
    for a in d["applications"]:
        a["icon_url"] = f"{base}/icons/{a['id']}.png"
    json.dump(d, open(f"{out}/{f}.json", "w"), indent=2, ensure_ascii=False)
EOF
python3 -m http.server "$PORT" --bind 127.0.0.1 --directory "$WORK/srv" > "$WORK/server.log" 2>&1 &
sleep 1.2
export DISPLAY=:$DISP

# ---- helpers on the running GUI
LOG=""; CALLS=""
nlog() { grep -ac "$1" "$LOG" 2>/dev/null; }
wait_log() { # pattern count-before [seconds] -> 0 when a new match appeared
	local i; for i in $(seq 1 $(( ${3:-6} * 10 ))); do [ "$(nlog "$1")" -gt "$2" ] && return 0; sleep 0.1; done; return 1
}
shot_px() { import -window root png:- | magick png:- -format "%[pixel:p{$1,$2}]" info:; }
RAILX=31; declare -A RAILY=([home]=102 [categories]=152 [search]=202 [downloads]=252 [updates]=302 [installed]=352)
click_rail() { # id [button]
	xdotool mousemove $RAILX "${RAILY[$1]}"; sleep 0.25; xdotool click "${2:-1}"
}
go() { # id -> 0 when it opened (a "NAV open" and a "NAV shown" line appeared)
	local o s; o=$(nlog "NAV open page=$1 "); s=$(nlog "NAV page=$1 shown")
	click_rail "$1"; wait_log "NAV page=$1 shown" "$s" 5
}
calls_since() { # line-number pattern
	tail -n +$(( $1 + 1 )) "$CALLS" | grep -cE "$2"
}

# ---- the engine is slow (a few seconds per state command, as on an old laptop): the window must
# stay alive. The Store used to run those commands on the GTK thread, at startup and every 1.5 s,
# so it never repainted or answered a click (it froze on opening). Run with XXRI_TEST_SLOW=5.
slow_engine_check() { # uses $arch
	local s i ok_n=0
	for i in 1 2 3; do
		s=$(nlog "NAV page=categories shown"); click_rail categories
		wait_log "NAV page=categories shown" "$s" 3 && ok_n=$((ok_n + 1))
		s=$(nlog "NAV page=home shown"); click_rail home
		wait_log "NAV page=home shown" "$s" 3 && ok_n=$((ok_n + 1))
	done
	ok "$arch: with a ${XXRI_TEST_SLOW:-5}s engine the Store still answers clicks ($ok_n of 6 page switches within 3 s each)" "$([ "$ok_n" = 6 ] && echo 0 || echo 1)"
	local running; running=$(pgrep -fc "ash .*bin/xxri-store (installed|updates|downloads)" || true)
	ok "$arch:   ... and never has more than one set of state commands in flight ($running running now)" "$([ "${running:-0}" -le 6 ] && echo 0 || echo 1)"
}

# ---- search, then scroll. The page's scrolled window kept a pointer to the first results grid,
# which the next keystroke freed; reaching the bottom then ran on freed memory (search
# "browser", scroll: crash). Type slowly so every letter rebuilds the results, scroll to the end.
search_scroll_check() { # uses $arch, $WORK from run_arch
	go search; sleep 0.8
	xdotool mousemove 790 33; sleep 0.2; xdotool click 1; sleep 0.3
	xdotool type --delay 250 browser; sleep 1.5
	local k; for k in $(seq 1 12); do xdotool mousemove 560 $(( 330 + k )) click 5; sleep 0.05; done
	xdotool key ctrl+a; xdotool type --delay 200 "a"; sleep 1      # a long result list: lazy loading
	for k in $(seq 1 80); do xdotool mousemove 560 $(( 330 + k % 5 )) click 5; sleep 0.04; done
	sleep 1.5; import -window root "$WORK/search-$arch.png" 2>/dev/null
	ok "$arch: search, type and scroll to the bottom: the Store is still running" "$(pgrep -f "$WORK/gui" >/dev/null && echo 0 || echo 1)"
	ok "$arch:   ... and AddressSanitizer saw no memory error" "$(grep -q 'ERROR: AddressSanitizer' "$WORK/gui-$arch.log" && echo 1 || echo 0)" "$(grep -m3 -A6 'ERROR: AddressSanitizer' "$WORK/gui-$arch.log" | head -12)"
}

run_arch() { # arch busybox catalog-file
	local arch=$1 BB=$2 cat=$3 H
	H=$WORK/home-$arch
	echo "== $arch OS (device busybox: $(basename "$(dirname "$(dirname "$BB")")"), host kernel $(uname -m))"
	: > "$WORK/server.log"
	rm -rf "$H"; mkdir -p "$H/.cache/xxri-store" "$WORK/stubs"
	cp "$WORK/srv/$cat" "$H/.cache/xxri-store/$cat"; date +%s > "$H/.cache/xxri-store/refreshed"
	for n in xxri-store xxri-app; do
		# XXRI_TEST_SLOW=N: installed/updates/downloads take N seconds more, like a slow laptop's engine
		printf '#!/bin/sh\necho "$(date +%%s.%%N | cut -c1-14) %s $*" >> "$HOME/calls.log"\ncase "$1" in installed|updates|downloads) [ -n "$XXRI_TEST_SLOW" ] && sleep "$XXRI_TEST_SLOW" ;; esac\nexec %s ash %s/bin/%s "$@"\n' \
			$n "$BB" "$SRC" $n > "$WORK/stubs/$n"; chmod +x "$WORK/stubs/$n"
	done
	LOG=$H/xxri-store-debug.log; CALLS=$H/calls.log
	pkill -f "Xvfb :$DISP" 2>/dev/null; rm -f /tmp/.X11-unix/X$DISP
	Xvfb :$DISP -screen 0 1024x768x24 >/dev/null 2>&1 & sleep 1.5
	local SHARE=$ROOT/src/xxri-store/root/usr/local/share
	bwrap --ro-bind / / --tmpfs /usr/local/share --tmpfs /usr/local/etc --tmpfs /usr/local/lib --dev /dev --proc /proc \
		--ro-bind "$SHARE/xxri-store" /usr/local/share/xxri-store \
		--ro-bind "$ROOT/ptxdist/projectroot/usr/local/share/pixmaps" /usr/local/share/pixmaps \
		--ro-bind "$ROOT/src/xxri-settings/root/usr/local/share/xxri-settings" /usr/local/share/xxri-settings \
		--ro-bind "$SRC/etc/xxri" /usr/local/etc/xxri --ro-bind "$SRC/lib/xxri" /usr/local/lib/xxri \
		--bind "$H" "$H" --setenv HOME "$H" --setenv DISPLAY :$DISP --setenv PATH "$WORK/stubs:/usr/bin:/bin" \
		--setenv REMOTE_BASE "http://127.0.0.1:$PORT" --setenv XXRI_STORE_DEBUG 1 --setenv GTK_OVERLAY_SCROLLING 0 \
		--setenv NO_AT_BRIDGE 1 --setenv GDK_BACKEND x11 --setenv ASAN_OPTIONS detect_leaks=0 ${XXRI_TEST_SLOW:+--setenv XXRI_TEST_SLOW "$XXRI_TEST_SLOW"} \
		${XXRI_TEST_LIBPATH:+/lib64/ld-linux-x86-64.so.2 --library-path "$XXRI_TEST_LIBPATH"} \
		"$WORK/gui" > "$WORK/gui-$arch.log" 2>&1 &
	local i; for i in $(seq 1 150); do grep -q "REPO_AVAILABLE=" "$LOG" 2>/dev/null && break; sleep 0.2; done; sleep 4
	ok "$arch: the GUI started and loaded a catalog" "$(grep -q 'REPO_AVAILABLE=1' "$LOG" && echo 0 || echo 1)" "$(tail -3 "$LOG" 2>/dev/null)"

	# the catalog the engine picked, and whether it fits this device (the 64-bit symptom was: no apps)
	local n; n=$(nlog "NAV page="); click_rail categories; sleep 1; click_rail home; sleep 1.5
	local line; line=$(grep "NAV page=home shown" "$LOG" | tail -1)
	ok "$arch: model reports $arch and apps installable here: $line" "$(echo "$line" | grep -qE "\($arch: [0-9]+ apps, [1-9][0-9]* installable here\)" && echo 0 || echo 1)"
	ok "$arch: it loaded $cat" "$(grep -q "catalog=.*/$cat" "$LOG" && echo 0 || echo 1)"

	if [ "${XXRI_TEST_ONLY:-}" = slow ]; then   # only the slow-engine responsiveness check
		sleep 3; slow_engine_check; pkill -f "$WORK/gui" 2>/dev/null; sleep 0.5; return
	fi
	if [ "${XXRI_TEST_ONLY:-}" = search ]; then   # just the search-and-scroll crash check
		search_scroll_check; pkill -f "$WORK/gui" 2>/dev/null; sleep 0.5; return
	fi

	# ---- every rail item, in a fixed order, with the highlight and the backend work it costs
	local prev=home id from c0 hl others r
	for id in categories search downloads updates installed home; do
		from=$prev; c0=$(wc -l < "$CALLS")
		go $id; r=$?; ok "$arch: rail '$id' opens page $id" "$r"
		ok "$arch:   opened from '$from'" "$(grep "NAV open page=$id " "$LOG" | tail -1 | grep -q "from=$from\$" && echo 0 || echo 1)"
		sleep 0.5
		hl=$(shot_px 15 "${RAILY[$id]}"); others=""
		local o; for o in "${!RAILY[@]}"; do [ $o != $id ] && others="$others $(shot_px 15 "${RAILY[$o]}")"; done
		ok "$arch:   '$id' is the highlighted item ($hl, others differ)" "$(case "$others" in *"$hl"*) echo 1 ;; *) echo 0 ;; esac)" "others:$others"
		ok "$arch:   no catalog fetch for the page switch (refresh/status calls: $(calls_since $c0 'xxri-store (refresh|status)'))" "$([ "$(calls_since $c0 'xxri-store (refresh|status)')" = 0 ] && echo 0 || echo 1)"
		prev=$id
	done

	# ---- clicking where you already are, and the other mouse buttons, do nothing
	local o1 o2; o1=$(nlog "NAV open"); click_rail home; sleep 1; o2=$(nlog "NAV open")
	ok "$arch: clicking the current page does not rebuild it" "$([ "$o1" = "$o2" ] && echo 0 || echo 1)"
	click_rail search 3; sleep 1; ok "$arch: right-click on an item does not navigate" "$([ "$(nlog "NAV open")" = "$o1" ] && echo 0 || echo 1)"

	# ---- keyboard: focus is on the clicked item, Tab moves on, Enter / Space activate
	click_rail home; sleep 0.4     # already there: only takes the focus
	local s; s=$(nlog "NAV page=categories shown"); xdotool key Tab; sleep 0.3; xdotool key Return
	ok "$arch: Tab then Enter opens the next item (categories)" "$(wait_log 'NAV page=categories shown' "$s" 4 && echo 0 || echo 1)"
	s=$(nlog "NAV page=search shown"); xdotool key Tab; sleep 0.3; xdotool key space
	ok "$arch: Tab then Space opens the one after (search)" "$(wait_log 'NAV page=search shown' "$s" 4 && echo 0 || echo 1)"
	s=$(nlog "NAV page=categories shown"); xdotool key shift+Tab; sleep 0.3; xdotool key Return
	ok "$arch: Shift+Tab then Enter goes back to categories" "$(wait_log 'NAV page=categories shown' "$s" 4 && echo 0 || echo 1)"

	# ---- the highlight follows a page change that did not come from the rail (the Back button)
	go downloads; sleep 0.5; go updates; sleep 0.5
	s=$(nlog "NAV page=downloads shown"); xdotool mousemove 107 33; sleep 0.25; xdotool click 1
	ok "$arch: Back returns to downloads" "$(wait_log 'NAV page=downloads shown' "$s" 4 && echo 0 || echo 1)"
	sleep 0.6; hl=$(shot_px 15 "${RAILY[downloads]}"); local u; u=$(shot_px 15 "${RAILY[updates]}")
	ok "$arch: after Back the highlight moved to downloads ($hl, not the old $u)" "$([ "$hl" != "$u" ] && [ "$u" = "$(shot_px 15 "${RAILY[installed]}")" ] && echo 0 || echo 1)"

	search_scroll_check

	# ---- the downloads page owns one timer, however it was reached
	go home; sleep 2; go downloads; local c1; c1=$(wc -l < "$CALLS"); sleep 5
	local single; single=$(calls_since $c1 'xxri-store downloads')
	for i in 1 2 3 4 5; do go home; sleep 0.3; go downloads; sleep 0.3; done
	c1=$(wc -l < "$CALLS"); sleep 5; local rapid; rapid=$(calls_since $c1 'xxri-store downloads')
	ok "$arch: reopening Downloads quickly adds no timers (5 s of calls: $single once, $rapid after 5 reopenings)" "$([ "$rapid" -le $(( single + 2 )) ] && echo 0 || echo 1)"

	# ---- icons that never arrive are asked for once, not on every page switch
	local dup; dup=$(grep "xxri-store icons" "$CALLS" | sed 's/^[0-9.]* xxri-store icons //' | tr ' ' '\n' | grep . | sort | uniq -d | wc -l)
	ok "$arch: no icon was requested twice across all those page switches ($dup repeated)" "$([ "$dup" = 0 ] && echo 0 || echo 1)"
	ok "$arch: the catalog came only from the cache/this arch ($(grep -ac "/$([ $cat = x64.json ] && echo i686 || echo x64).json" "$WORK/server.log") requests for the other one)" "$([ "$(grep -ac "/$([ $cat = x64.json ] && echo i686 || echo x64).json" "$WORK/server.log")" = 0 ] && echo 0 || echo 1)"
	pkill -f "$WORK/gui" 2>/dev/null; sleep 0.5
}

run_arch i686   "$BB32" i686.json
run_arch x86_64 "$BB64" x64.json

echo; echo "store-nav-test: $pass passed, $fail failed"
[ $fail = 0 ]
