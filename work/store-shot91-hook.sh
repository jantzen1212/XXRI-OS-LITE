PAGE=$(sed -n 's/.*\bstorepage=\([a-zA-Z0-9:_-]*\).*/\1/p' /proc/cmdline 2>/dev/null | head -1)
[ -n "$PAGE" ] || PAGE=home
( sleep 8
  for id in xxri-clock xxri-sysinfo xxri-calc xxri-notes xxri-hello; do xxri-store install "$id" >/dev/null 2>&1; done
  sleep 2
  DISPLAY=:0 /usr/local/bin/xxri-store-gui "$PAGE" >/dev/null 2>&1
) &
