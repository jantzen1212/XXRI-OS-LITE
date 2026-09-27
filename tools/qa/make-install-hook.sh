#!/bin/bash
# tools/qa/make-install-hook.sh OUT.img - FAT disk holding xxri-qa-hook.gz (QA only)
set -euo pipefail
H=$(cd "$(dirname "$0")/install-hook" && pwd)
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
( cd "$H" && find . | sort | cpio -o -H newc --quiet -R 0:0 ) | gzip -9 -n > "$W/xxri-qa-hook.gz"
rm -f "$1"; mkfs.fat -C -n XXRIQA "$1" 4096 >/dev/null
mcopy -i "$1" "$W/xxri-qa-hook.gz" ::/
