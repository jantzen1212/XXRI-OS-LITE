# Screenshot hook: install the five bundled apps so the dock shows them, then
# launch XXRI System Info so a real installed app is visible running.
( sleep 8
  for id in xxri-clock xxri-sysinfo xxri-calc xxri-notes xxri-hello; do
    xxri-store install "$id" >/dev/null 2>&1
  done
  sleep 2
  xxri-app launch xxri-sysinfo >/dev/null 2>&1
  sync
) &
