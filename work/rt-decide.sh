( LOG="$HOME/rt10.log"; : > "$LOG"
  say(){ echo "$*" >> "$LOG"; sync; }
  PBD=/usr/local/lib/gdk-pixbuf-2.0/2.10.0
  i=0; while [ $i -lt 60 ]; do route -n 2>/dev/null | grep -q '^0.0.0.0' && break; sleep 2; i=$((i+2)); done

  say "=== A. WHICH formats decode? (isolates module vs machinery) ==="
  sudo sh -c "gdk-pixbuf-query-loaders > $PBD/loaders.cache" 2>/dev/null
  say "  cache formats: $(grep -E '^\"[a-z0-9]+\" [0-9]+ \"gdk-pixbuf\"' $PBD/loaders.cache | awk '{print $1}' | tr -d '\"' | tr '\n' ' ')"
  say ""
  # build one sample of each format from a source we control
  mkdir -p /tmp/fmt
  # xpm is plain text - write one directly, no encoder needed
  cat > /tmp/fmt/t.xpm <<'XPMEOF'
/* XPM */
static char * t_xpm[] = {
"4 4 2 1",
" 	c #FF0000",
".	c #0000FF",
" ...",
". ..",
".. .",
"... "};
XPMEOF
  for f in /usr/local/share/pixmaps/*.png; do [ -f "$f" ] && { cp "$f" /tmp/fmt/t.png; break; }; done
  for f in $(find /usr/local/share -name '*.jpg' 2>/dev/null | head -1); do cp "$f" /tmp/fmt/t.jpg; done
  for f in $(find /usr/local/share -name '*.svg' 2>/dev/null | head -1); do cp "$f" /tmp/fmt/t.svg; done
  for f in $(find /usr/local/share -name '*.gif' 2>/dev/null | head -1); do cp "$f" /tmp/fmt/t.gif; done
  for f in $(find /usr/local/share -name '*.bmp' 2>/dev/null | head -1); do cp "$f" /tmp/fmt/t.bmp; done
  for t in /tmp/fmt/t.*; do
      [ -f "$t" ] || continue
      gdk-pixbuf-thumbnailer "$t" "/tmp/fmt/out_$(basename $t).png" >/tmp/fmt/e 2>&1
      say "  $(basename $t): exit=$? out=$(wc -c < /tmp/fmt/out_$(basename $t).png 2>/dev/null || echo 0) B  $(grep -o 'Couldn.t recognize' /tmp/fmt/e | head -1)"
  done

  say ""
  say "=== B. is it the thumbnailer or the library? try a GTK consumer ==="
  say "  (xxri-store-gui loads PNGs via cairo, so it is not a gdk-pixbuf test)"
  say "  gdk-pixbuf-pixdata on a PNG:"
  gdk-pixbuf-pixdata /tmp/fmt/t.png /tmp/fmt/pd.out >/tmp/fmt/pd.err 2>&1
  say "    exit=$?  $(head -c 200 /tmp/fmt/pd.err | tr '\n' ' ')"

  say ""
  say "=== C. Electron: Code OSS on an image that HAS libXss ==="
  say "  libXss.so.1 in OS: $(ls /usr/local/lib/libXss.so.1 2>/dev/null || echo ABSENT)"
  B=/opt/xxri/code-oss/VSCode-linux-ia32/code
  if [ -x "$B" ]; then
      say "  ldd NOT FOUND (after): $(ldd "$B" 2>&1 | grep -c 'not found')"
      ldd "$B" 2>&1 | grep 'not found' | sed 's/^/    /' >> "$LOG"
      echo "code-oss" > "$HOME/stage"; sync
      ( cd "$HOME"; timeout 60 "$B" --disable-gpu --no-sandbox --disable-dev-shm-usage ) >/tmp/b.out 2>&1 &
      sleep 50
      say "  processes alive: $(ps -eo args | grep -c 'VSCode-linux-ia32/code')"
      grep -iE 'error|cannot|fail|missing|crash' /tmp/b.out | head -8 | sed 's/^/    /' >> "$LOG"
      for p in $(ps -eo pid,args | grep -v grep | grep VSCode-linux-ia32 | awk '{print $1}'); do kill -9 $p 2>/dev/null; done
      sleep 3
  else say "  code-oss not installed on this image"; fi
  echo DONE > "$HOME/stage"
  say "=== RT10 DONE ==="; sync
) >/dev/null 2>&1 &
