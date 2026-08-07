# Store screenshot hook (injected into /home/xxri/.X.d for QA only).
# Reads `storepage=<id>` from the kernel command line and opens the Store on
# that page, so the host can screendump each page on the real Xvesa desktop
# without needing a working pointer.  Not shipped in the release image.
PAGE=$(sed -n 's/.*\bstorepage=\([a-zA-Z0-9:_-]*\).*/\1/p' /proc/cmdline 2>/dev/null | head -1)
[ -n "$PAGE" ] || PAGE=home
( sleep 6; DISPLAY=:0 /usr/local/bin/xxri-store-gui "$PAGE" >/dev/null 2>&1 ) &
