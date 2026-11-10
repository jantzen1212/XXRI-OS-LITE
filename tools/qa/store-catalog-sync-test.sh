#!/bin/bash
# store-catalog-sync-test.sh - ptxdist/scripts/xxri-store-catalog-sync.py against the real
# repo.xxri.flows.best (needs the network) and against broken catalogs served locally.
#
#   XXRI_REPO_DIR  a checkout of xxri-repo to build the broken catalogs from (default ~/xxri-repo)
#   XXRI_PYTHON    the Python to run the tool with (default python3; the build uses PTXdist's)
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
SYNC=$HERE/../../ptxdist/scripts/xxri-store-catalog-sync.py
PY=${XXRI_PYTHON:-python3}
REPO=${XXRI_REPO_DIR:-$HOME/xxri-repo}
WORK=$(mktemp -d "${TMPDIR:-/tmp}/catalog-sync.XXXXXX")
SRV=""
trap '[ -n "$SRV" ] && kill $SRV 2>/dev/null; rm -rf "$WORK"' EXIT
pass=0; fail=0
ok() { if [ "$2" = 0 ]; then pass=$((pass + 1)); printf '  PASS  %s\n' "$1"; else fail=$((fail + 1)); printf '  FAIL  %s %s\n' "$1" "${3:-}"; fi; }
sync() { "$PY" "$SYNC" "$OUT" 2>&1; }
fresh() { OUT=$WORK/out-$1; rm -rf "$OUT"; }
j() { python3 -c "import json,sys; print(json.load(open(sys.argv[1]))[sys.argv[2]])" "$1" "$2"; }
[ -e "$REPO/x64.json" ] || { echo "missing $REPO/x64.json" >&2; exit 2; }

echo "== repo.xxri.flows.best"
fresh net; r=$(sync); rc=$?
ok "fetches, validates, packages (rc=$rc)" "$([ $rc = 0 ] && [ -s "$OUT/x64.json" ] && [ -s "$OUT/x64.json.source" ] && echo 0 || echo 1)" "$r"
src=$OUT/x64.json.source
ok "the bytes the server has" "$(curl -s https://repo.xxri.flows.best/x64.json | cmp -s - "$OUT/x64.json" && echo 0 || echo 1)"
ok "every entry is x86_64" "$(python3 -c "import json,sys; d=json.load(open(sys.argv[1])); print(0 if {a['architecture'] for a in d['applications']}=={'x86_64'} else 1)" "$OUT/x64.json")"
ok "provenance: URL, ETag, sha256 of the packaged bytes" "$([ "$(j $src url)" = https://repo.xxri.flows.best/x64.json ] && [ "$(j $src etag)" != None ] && [ "$(j $src sha256)" = "$(sha256sum "$OUT/x64.json" | cut -d' ' -f1)" ] && echo 0 || echo 1)"
m1=$(stat -c %Y "$OUT/x64.json" "$src" | tr '\n' ' '); sleep 1.2
r2=$(sync); m2=$(stat -c %Y "$OUT/x64.json" "$src" | tr '\n' ' ')
ok "second run: 304, not downloaded, files untouched" "$(echo "$r2" | grep -q 'not modified' && echo "$r2" | grep -q 'already current' && [ "$m1" = "$m2" ] && echo 0 || echo 1)" "$r2"
cp -r "$OUT" "$WORK/out-off"; OUT=$WORK/out-off
r=$(https_proxy=http://127.0.0.1:9 HTTPS_PROXY=http://127.0.0.1:9 sync); rc=$?
ok "offline with an earlier copy: fails, says what to do" "$([ $rc != 0 ] && echo "$r" | grep -q ERROR && echo "$r" | grep -q XXRI_STORE_CATALOG_URL && echo 0 || echo 1)" "$r"
fresh off2; https_proxy=http://127.0.0.1:9 HTTPS_PROXY=http://127.0.0.1:9 sync >/dev/null
ok "offline, nothing there: nothing written" "$([ -z "$(ls "$OUT" 2>/dev/null)" ] && echo 0 || echo 1)"
fresh file; r=$(XXRI_STORE_CATALOG_URL=file://$REPO/x64.json sync); rc=$?
ok "file:// copy on disk (offline build)" "$([ $rc = 0 ] && [ "$(j $OUT/x64.json.source url)" = "file://$REPO/x64.json" ] && echo 0 || echo 1)" "$r"

echo "== catalogs that must not be packaged (served locally)"
mkdir -p "$WORK/www"
port=$((20000 + RANDOM % 20000))
python3 -m http.server -d "$WORK/www" -b 127.0.0.1 $port >/dev/null 2>&1 & SRV=$!
for i in $(seq 50); do curl -s -o /dev/null http://127.0.0.1:$port/ && break; sleep 0.1; done
try() { # name description -> expects a refusal and nothing written
	fresh "$1"; r=$(XXRI_STORE_CATALOG_URL=http://127.0.0.1:$port/$1.json sync); rc=$?
	ok "$2 -> refused" "$([ $rc != 0 ] && echo "$r" | grep -q "ERROR" && [ ! -e "$OUT/x64.json" ] && echo 0 || echo 1)" "rc=$rc $(echo "$r" | grep ERROR | head -1)"
}
bad() { # name python-on-d description
	python3 - "$REPO/x64.json" "$WORK/www/$1.json" "$2" <<'PY'
import json, sys
d = json.load(open(sys.argv[1])); a = d["applications"]
exec(sys.argv[3])
json.dump(d, open(sys.argv[2], "w"), indent=2)
PY
	try "$1" "$3"
}
cp "$REPO/i686.json" "$WORK/www/i686.json"; try i686 "the i686 catalog"
bad mixed 'a[7]["architecture"]="i686"' "one entry of another architecture"
bad nestedarch 'a[2]["sources"] = [{"architecture": "i686"}]' "a source of another architecture"
bad dupid 'a[4]["id"] = a[3]["id"]' "a duplicate id"
bad shellid 'a[1]["id"] = "x; rm -rf /"' "an id with shell characters"
bad sizestr 'a[0]["size"] = "123"' "a size that is a string"
bad badsum 'a[0]["checksum"] = "not-a-digest"' "a checksum that is not SHA-256"
bad nourl 'a[0].pop("download_url")' "an entry without download_url"
bad ftpurl 'a[0]["download_url"] = "ftp://example.com/x"' "a download_url that is not http(s)"
bad schema4 'd["schema_version"] = "4.0"' "schema 4.0"
bad noapps 'd["applications"] = []' "no applications"
bad nodate 'd["last_updated"] = "yesterday"' "a last_updated that is not a date"
printf '{"schema_version": "3.0", "applications": [' > "$WORK/www/trunc.json"; try trunc "truncated JSON"
printf '<!DOCTYPE html><html><body>404</body></html>' > "$WORK/www/html.json"; try html "an HTML page"
try missing "a 404"
# a good copy is not kept when the next one is bad
fresh keep; XXRI_STORE_CATALOG_URL=file://$REPO/x64.json sync >/dev/null
r=$(XXRI_STORE_CATALOG_URL=http://127.0.0.1:$port/mixed.json sync); rc=$?
ok "bad catalog after a good one: build fails (old copy not reported as success)" "$([ $rc != 0 ] && echo 0 || echo 1)" "$r"

echo; echo "store-catalog-sync-test: $pass passed, $fail failed"
[ $fail = 0 ]
