( LOG="$HOME/rt9.log"; : > "$LOG"
  say(){ echo "$*" >> "$LOG"; sync; }
  PBD=/usr/local/lib/gdk-pixbuf-2.0/2.10.0
  i=0; while [ $i -lt 60 ]; do route -n 2>/dev/null | grep -q '^0.0.0.0' && break; sleep 2; i=$((i+2)); done
  tce-load -wi strace >/dev/null 2>&1

  say "=== A. gdk-pixbuf decode path $(date) ==="
  say "--- A1: regenerate cache, then show what is actually IN it ---"
  gdk-pixbuf-query-loaders > /tmp/lc.new 2>/tmp/lc.err
  say "  query-loaders wrote $(wc -l < /tmp/lc.new) lines, stderr: $(wc -c < /tmp/lc.err) bytes"
  head -3 /tmp/lc.err | sed 's/^/    /' >> "$LOG"
  say "  formats it declares:"
  grep -E '^"[a-z0-9]+" [0-9]+ "gdk-pixbuf"' /tmp/lc.new | awk '{print $1}' | tr -d '"' | tr '\n' ' ' >> "$LOG"
  say ""
  say "  module paths it declares (first 3):"
  grep -E '^"/' /tmp/lc.new | head -3 | sed 's/^/    /' >> "$LOG"
  sudo cp /tmp/lc.new "$PBD/loaders.cache"
  say "  installed cache: $(wc -c < $PBD/loaders.cache) bytes"

  say ""
  say "--- A2: decode PNG with that cache, explicitly pointed at it ---"
  P=/usr/local/share/pixmaps/xxri-store.png
  GDK_PIXBUF_MODULE_FILE="$PBD/loaders.cache" gdk-pixbuf-thumbnailer "$P" /tmp/a2.png >/tmp/a2.err 2>&1
  say "  exit=$? out=$(wc -c < /tmp/a2.png 2>/dev/null || echo 0)"
  grep -v gnutls /tmp/a2.err | head -3 | sed 's/^/    /' >> "$LOG"

  say ""
  say "--- A3: does the decoder dlopen the png module? ---"
  strace -f -e trace=openat -o /tmp/a3.st gdk-pixbuf-thumbnailer "$P" /tmp/a3.png >/dev/null 2>&1
  say "  loaders.cache opens:"
  grep 'loaders.cache' /tmp/a3.st | head -3 | sed 's/^/    /' >> "$LOG"
  say "  libpixbufloader opens:"
  grep 'libpixbufloader' /tmp/a3.st | head -5 | sed 's/^/    /' >> "$LOG"
  say "  (empty above = loader table empty at decode time)"

  say ""
  say "--- A4: is /usr/local/lib a symlink farm into /tmp/tcloop? ---"
  say "  png module   : $(ls -l $PBD/loaders/libpixbufloader-png.so | sed 's/.*-> //')"
  say "  svg module   : $(ls -l $PBD/loaders/libpixbufloader-svg.so | sed 's/.*-> //')"
  say "  libgdk_pixbuf: $(readlink -f /usr/local/lib/libgdk_pixbuf-2.0.so.0)"
  say "  thumbnailer  : $(readlink -f $(command -v gdk-pixbuf-thumbnailer))"

  say ""
  say "=== B. Electron runtime (Code OSS) ==="
  B=/opt/xxri/code-oss/VSCode-linux-ia32/code
  if [ -x "$B" ]; then
      say "  ldd NOT FOUND: $(ldd "$B" 2>&1 | grep -c 'not found')"
      ldd "$B" 2>&1 | grep 'not found' | sed 's/^/    /' >> "$LOG"
      echo "code-oss" > "$HOME/stage"; sync
      ( cd "$HOME"; timeout 55 "$B" --disable-gpu --no-sandbox --disable-dev-shm-usage "$HOME" ) >/tmp/b.out 2>&1 &
      sleep 45
      say "  process alive: $(ps -eo args | grep -c 'VSCode-linux-ia32/code')"
      grep -iE 'error|cannot|fail|missing|denied' /tmp/b.out | head -8 | sed 's/^/    /' >> "$LOG"
      for p in $(ps -eo pid,args | grep -v grep | grep VSCode-linux-ia32 | awk '{print $1}'); do kill -9 $p 2>/dev/null; done
      sleep 3
  else say "  not installed on this image"; fi

  say ""
  say "=== C. Qt runtime (VLC) ==="
  if command -v vlc >/dev/null 2>&1; then
      echo "vlc" > "$HOME/stage"; sync
      ( cd "$HOME"; timeout 50 vlc --no-audio ) >/tmp/c.out 2>&1 &
      sleep 40
      say "  process alive: $(ps -eo comm | grep -c '^vlc$')"
      grep -iE 'error|cannot|fail' /tmp/c.out | head -6 | sed 's/^/    /' >> "$LOG"
      for p in $(ps -eo pid,comm | awk '$2=="vlc"{print $1}'); do kill $p 2>/dev/null; done
      sleep 3
  else say "  vlc not installed on this image"; fi

  echo DONE > "$HOME/stage"
  say "=== RT9 DONE ==="; sync
) >/dev/null 2>&1 &
