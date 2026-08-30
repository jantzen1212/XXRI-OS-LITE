#!/bin/bash
# tcz-install.sh ROOT PKG...  - resolve .dep closure and unsquashfs into ROOT.
# Reuses tczs already in work/iso/cde/optional/; downloads only what's missing
# into a local cache so repeat runs are offline.
set -u
ROOT="$1"; shift
BASE=$(cd "$(dirname "$0")/../.." && pwd)   # repo root
LOCAL="$BASE/work/iso/cde/optional"
CACHE="$BASE/work/xxri-file/tczcache"
MIRROR=http://repo.tinycorelinux.net/16.x/x86/tcz
mkdir -p "$CACHE"
declare -A seen
get() { # echo path to $1.tcz, fetching if needed
  local p="$1"
  [ -f "$LOCAL/$p.tcz" ] && { echo "$LOCAL/$p.tcz"; return 0; }
  [ -f "$CACHE/$p.tcz" ] && { echo "$CACHE/$p.tcz"; return 0; }
  timeout 120 curl -sf "$MIRROR/$p.tcz" -o "$CACHE/$p.tcz" || return 1
  echo "$CACHE/$p.tcz"
}
deps() { # echo dep names of $1
  local p="$1" f
  f="$CACHE/$p.tcz.dep"
  [ -f "$LOCAL/$p.tcz.dep" ] && f="$LOCAL/$p.tcz.dep"
  [ -f "$f" ] || timeout 60 curl -sf "$MIRROR/$p.tcz.dep" -o "$f" 2>/dev/null
  [ -f "$f" ] && sed 's/\.tcz[[:space:]]*$//; s/[[:space:]]*$//' "$f" | grep -v '^$'
}
install_one() {
  local p="$1" t
  [ -n "${seen[$p]:-}" ] && return 0
  seen[$p]=1
  for d in $(deps "$p"); do install_one "$d"; done
  t=$(get "$p") || { echo "  !! MISSING $p"; return 1; }
  unsquashfs -f -n -d "$ROOT" "$t" >/dev/null 2>&1 && echo "  + $p" || echo "  !! unpack failed $p"
}
for p in "$@"; do install_one "$p"; done
