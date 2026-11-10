#!/bin/bash
# store-engine-test.sh - the real xxri-store engine, once per architecture, against local
# servers that behave like the repository (and like a broken one).
#
# The OS architecture comes from the ELF header of the shell the engine runs in, so
# each run uses a device busybox: a 32-bit Tiny Core one (i686) and a 64-bit one
# (x86_64). The host kernel is x86_64 for both - that is the point.
#
#   XXRI_BB32      32-bit Tiny Core busybox      (default ~/.xxri-testenv/final-core/bin/busybox)
#   XXRI_BB64      64-bit Tiny Core busybox      (default: busybox from an unpacked x86_64 core.gz)
#   XXRI_REPO_DIR  checkout of the repository    (default ~/xxri-repo; i686.json and x64.json)
#
# Needs bwrap and python3; nothing outside $WORK is touched.
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../.." && pwd)
SRC=$ROOT/src/xxri-store/root/usr/local
BB32=${XXRI_BB32:-$HOME/.xxri-testenv/final-core/bin/busybox}
BB64=${XXRI_BB64:-$HOME/.xxri-testenv/q/x64/vopt64/core/bin/busybox}
REPO=${XXRI_REPO_DIR:-$HOME/xxri-repo}
WORK=${XXRI_TEST_WORK:-$(mktemp -d "${TMPDIR:-/tmp}/store-engine.XXXXXX")}
PORT=${XXRI_TEST_PORT:-18100}
pass=0; fail=0
ok() { # name condition-result(0 = good) [detail]
	if [ "$2" = 0 ]; then pass=$((pass + 1)); printf '  PASS  %s\n' "$1"
	else fail=$((fail + 1)); printf '  FAIL  %s %s\n' "$1" "${3:-}"; fi
}
for f in "$BB32" "$BB64" "$REPO/i686.json" "$REPO/x64.json"; do
	[ -e "$f" ] || { echo "missing: $f" >&2; exit 2; }
done

# ---- servers: ok | html | wrong | trunc | none (404)
mkdir -p "$WORK/srv"/{ok,html,wrong,trunc,none}
cp "$REPO/i686.json" "$REPO/x64.json" "$WORK/srv/ok/"
for a in i686 x64; do
	printf '<!DOCTYPE html>\n<html><head><title>Not found</title></head><body>404</body></html>\n' > "$WORK/srv/html/$a.json"
	head -c $(( $(stat -c %s "$REPO/$a.json") / 2 )) "$REPO/$a.json" > "$WORK/srv/trunc/$a.json"
done
cp "$REPO/i686.json" "$WORK/srv/wrong/x64.json"; cp "$REPO/x64.json" "$WORK/srv/wrong/i686.json"
declare -A P; n=0
for m in ok html wrong trunc none; do
	P[$m]=$((PORT + n)); n=$((n + 1))
	python3 -m http.server "${P[$m]}" --bind 127.0.0.1 --directory "$WORK/srv/$m" > "$WORK/srv-$m.log" 2>&1 &
done
trap 'kill $(jobs -p) 2>/dev/null; [ -z "${XXRI_TEST_WORK:-}" ] && rm -rf "$WORK"' EXIT
sleep 1.5

eng() { # busybox home bundled-dir remote-base -- args
	local bb=$1 h=$2 b=$3 base=$4; shift 5
	local args=(--ro-bind / / --tmpfs /usr/local/share --tmpfs /usr/local/etc --tmpfs /usr/local/lib
		--dev /dev --proc /proc --ro-bind "$SRC/lib/xxri" /usr/local/lib/xxri
		--ro-bind "$SRC/etc/xxri" /usr/local/etc/xxri --bind "$h" "$h")
	[ -n "$b" ] && args+=(--ro-bind "$b" /usr/local/share/xxri-store)
	bwrap "${args[@]}" --setenv HOME "$h" --setenv PATH /usr/bin:/bin --setenv REMOTE_BASE "$base" \
		"$bb" ash "$SRC/bin/xxri-store" "$@"
}
fresh() { rm -rf "$1"; mkdir -p "$1"; }
jv() { sed -n "s/.*\"$2\":\"\{0,1\}\([^\",}]*\).*/\1/p" <<<"$1" | head -1; }
nreq() { grep -c "$2" "$WORK/srv-$1.log"; }   # requests that matched a pattern
url() { echo "http://127.0.0.1:${P[$1]}"; }

for arch in i686 x86_64; do
	if [ $arch = i686 ]; then BB=$BB32; cat=i686.json; other=x64.json; apps=38; else BB=$BB64; cat=x64.json; other=i686.json; apps=245; fi
	H=$WORK/home-$arch
	echo "== $arch OS (device busybox: $(basename "$(dirname "$(dirname "$BB")")"), host kernel: $(uname -m))"

	mkdir -p "$H"; a=$(eng "$BB" "$H" "" "$(url ok)" -- arch)
	ok "$arch: arch is $arch although uname says $(uname -m)" "$([ "$(jv "$a" arch)" = $arch ] && echo 0 || echo 1)" "$a"
	ok "$arch: catalog is $cat" "$([ "$(jv "$a" catalog)" = $cat ] && echo 0 || echo 1)" "$a"

	# nothing cached, nothing bundled
	fresh "$H"; s=$(eng "$BB" "$H" "" "$(url ok)" -- status)
	ok "$arch: before any refresh: unavailable, says why" "$([ "$(jv "$s" available)" = false ] && [ "$(jv "$s" error)" = no-catalog ] && echo 0 || echo 1)" "$s"

	# first refresh: right file, only that file
	: > "$WORK/srv-ok.log"; r=$(eng "$BB" "$H" "" "$(url ok)" -- refresh --force)
	ok "$arch: refresh -> updated" "$([ "$r" = updated ] && echo 0 || echo 1)" "[$r]"
	ok "$arch: requested $cat" "$([ "$(nreq ok "GET /$cat")" -ge 1 ] && echo 0 || echo 1)"
	ok "$arch: never requested $other" "$([ "$(nreq ok "/$other")" = 0 ] && echo 0 || echo 1)"
	ok "$arch: cached as $cat, byte-identical" "$(cmp -s "$H/.cache/xxri-store/$cat" "$REPO/$cat" && echo 0 || echo 1)"
	ok "$arch: no $other in the cache" "$([ ! -e "$H/.cache/xxri-store/$other" ] && echo 0 || echo 1)"
	s=$(eng "$BB" "$H" "" "$(url ok)" -- status)
	ok "$arch: status: available, $apps apps, $arch, remote" "$([ "$(jv "$s" available)" = true ] && [ "$(jv "$s" apps)" = $apps ] && [ "$(jv "$s" arch)" = $arch ] && [ "$(jv "$s" repo)" = remote ] && [ "$(jv "$s" error)" = "" ] && echo 0 || echo 1)" "$s"
	ok "$arch: status probes with HEAD, not a full GET" "$([ "$(nreq ok "HEAD /$cat")" -ge 1 ] && echo 0 || echo 1)"
	: > "$WORK/srv-ok.log"; r=$(eng "$BB" "$H" "" "$(url ok)" -- refresh)
	ok "$arch: refresh inside max age -> fresh, no request" "$([ "$r" = fresh ] && [ ! -s "$WORK/srv-ok.log" ] && echo 0 || echo 1)" "[$r]"

	# a bad answer never replaces a good cache, and says why
	cp "$H/.cache/xxri-store/$cat" "$WORK/good-$arch.json"
	for mode in html:rejected:html-page wrong:rejected:wrong-arch trunc:rejected:truncated none:offline-cached; do
		m=${mode%%:*}; want=${mode#*:}
		[ $m = wrong ] && want="rejected:wrong-arch:$([ $arch = i686 ] && echo x86_64 || echo i686)"
		r=$(eng "$BB" "$H" "" "$(url $m)" -- refresh --force)
		ok "$arch: server sends '$m' -> $want" "$([ "$r" = "$want" ] && echo 0 || echo 1)" "[$r]"
		ok "$arch:   good cache untouched" "$(cmp -s "$H/.cache/xxri-store/$cat" "$WORK/good-$arch.json" && echo 0 || echo 1)"
		s=$(eng "$BB" "$H" "" "$(url ok)" -- status)
		case $m in none) why=download-failed ;; wrong) why=wrong-arch:$([ $arch = i686 ] && echo x86_64 || echo i686) ;; html) why=html-page ;; trunc) why=truncated ;; esac
		ok "$arch:   status keeps working and reports '$why'" "$([ "$(jv "$s" available)" = true ] && [ "$(jv "$s" error)" = "$why" ] && echo 0 || echo 1)" "$(jv "$s" error)"
	done
	r=$(eng "$BB" "$H" "" "$(url ok)" -- refresh --force)
	s=$(eng "$BB" "$H" "" "$(url ok)" -- status)
	ok "$arch: next good refresh clears the error" "$([ "$(jv "$s" error)" = "" ] && echo 0 || echo 1)" "[$r] $(jv "$s" error)"

	# a bad answer with nothing cached: failure, no file, no other catalog
	for mode in html:invalid:html-page trunc:invalid:truncated none:unavailable; do
		m=${mode%%:*}; want=${mode#*:}; fresh "$H"
		r=$(eng "$BB" "$H" "" "$(url $m)" -- refresh --force 2>&1); rc=$?
		ok "$arch: empty cache + '$m' -> $want, exit 1, nothing cached" "$([ "$r" = "$want" ] && [ $rc = 1 ] && [ -z "$(ls "$H/.cache/xxri-store" | grep '\.json$')" ] && echo 0 || echo 1)" "[$r] rc=$rc"
	done
	fresh "$H"; r=$(eng "$BB" "$H" "" "$(url wrong)" -- refresh --force 2>&1)
	ok "$arch: empty cache + wrong-arch catalog -> refused, not used" "$(case $r in invalid:wrong-arch:*) echo 0 ;; *) echo 1 ;; esac)" "[$r]"

	# bundled copy (x64 only ships one; an i686 OS must not look at it)
	B=$WORK/bundled-$arch; mkdir -p "$B"; cp "$REPO/x64.json" "$B/x64.json"
	fresh "$H"; s=$(eng "$BB" "$H" "$B" "$(url none)" -- status); p=$(eng "$BB" "$H" "$B" "$(url none)" -- catalog-path)
	if [ $arch = x86_64 ]; then
		ok "$arch: bundled copy alone, offline: usable, repo=bundled, $apps apps" "$([ "$(jv "$s" available)" = true ] && [ "$(jv "$s" repo)" = bundled ] && [ "$(jv "$s" apps)" = $apps ] && echo 0 || echo 1)" "$s"
		ok "$arch: catalog-path is the bundled file" "$([ "$p" = /usr/local/share/xxri-store/x64.json ] && echo 0 || echo 1)" "$p"
		r=$(eng "$BB" "$H" "$B" "$(url none)" -- refresh --force)
		ok "$arch: offline refresh keeps the bundled copy, writes no cache" "$([ "$r" = offline-cached ] && [ ! -e "$H/.cache/xxri-store/x64.json" ] && echo 0 || echo 1)" "[$r]"
		r=$(eng "$BB" "$H" "$B" "$(url ok)" -- refresh --force); p=$(eng "$BB" "$H" "$B" "$(url ok)" -- catalog-path)
		ok "$arch: online refresh caches it; on a tie the cache is used" "$([ "$r" = updated ] && [ "$p" = "$H/.cache/xxri-store/x64.json" ] && echo 0 || echo 1)" "[$r] $p"
		sed 's/"last_updated": "2026-10-25"/"last_updated": "2027-01-01"/' "$REPO/x64.json" > "$B/x64.json"
		p=$(eng "$BB" "$H" "$B" "$(url none)" -- catalog-path)
		ok "$arch: a newer bundled copy beats an older cache" "$([ "$p" = /usr/local/share/xxri-store/x64.json ] && echo 0 || echo 1)" "$p"
	else
		ok "$arch: an x64.json bundled in the image is ignored" "$([ "$(jv "$s" available)" = false ] && [ "$p" = "$H/.cache/xxri-store/i686.json" ] && echo 0 || echo 1)" "$s $p"
	fi
done

echo "== install refuses a catalog entry of the other architecture (hand-placed cache)"
for arch in i686 x86_64; do
	[ $arch = i686 ] && { BB=$BB32; cat=i686.json; foreign=x64.json; } || { BB=$BB64; cat=x64.json; foreign=i686.json; }
	H=$WORK/gate-$arch; fresh "$H"; mkdir -p "$H/.cache/xxri-store"; cp "$REPO/$foreign" "$H/.cache/xxri-store/$cat"; date +%s > "$H/.cache/xxri-store/refreshed"
	id=$(python3 -c "import json,sys; print(json.load(open(sys.argv[1]))['applications'][0]['id'])" "$REPO/$foreign")
	r=$(eng "$BB" "$H" "" "$(url ok)" -- install "$id" 2>&1 | tail -1)
	ok "$arch: install $id (a $([ $arch = i686 ] && echo x86_64 || echo i686) app) -> ERR:arch" "$(case $r in ERR:arch:*) echo 0 ;; *) echo 1 ;; esac)" "[$r]"
done

echo; echo "store-engine-test: $pass passed, $fail failed"
[ $fail = 0 ]
