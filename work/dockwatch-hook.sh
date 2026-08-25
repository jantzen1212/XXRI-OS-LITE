# dockwatch-hook.sh - QA only.  Every few seconds, print the CLIENT windows the
# window manager is managing, by name, from the guest's own X server.
# `xwininfo -root -children` only lists the window manager's frames, whose names
# are not the application's - reading one level deeper is what makes an
# application's window actually show up in this log.
(
  exec >/dev/ttyS0 2>&1
  sleep 25
  echo "=== DOCKWATCH START ==="
  n=0
  while [ $n -lt 200 ]; do
    n=$((n+1))
    names=$(DISPLAY=:0 xwininfo -root -tree 2>/dev/null |
      sed -n 's/^ *0x[0-9a-f]* "\([^"]*\)": *("\([^"]*\)".*/\1/p' |
      grep -v '^$' | tr '\n' '|')
    echo "DW t=$((n*4)) wm=$(pidof flwm >/dev/null && echo up || echo DOWN) win=[$names]"
    sleep 4
  done
) &
