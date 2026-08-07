( LOG="$HOME/rt2.log"; : > "$LOG"
  say(){ echo "$*" >> "$LOG"; sync; }
  i=0; while [ $i -lt 60 ]; do route -n 2>/dev/null | grep -q '^0.0.0.0' && break; sleep 2; i=$((i+2)); done
  tce-load -wi strace >/dev/null 2>&1

  P=/usr/local/share/pixmaps/xxri-store.png
  LIB=/usr/local/lib/libgdk_pixbuf-2.0.so.0
  PBD=/usr/local/lib/gdk-pixbuf-2.0/2.10.0

  say "=== gdk-pixbuf PNG root cause $(date) ==="
  say "sample PNG   : $P  $(wc -c < $P) bytes  magic=$(hexdump -n 8 -e '8/1 "%02x"' $P)"
  say "libgdk_pixbuf: $(readlink -f $LIB)"
  say ""
  say "--- 1. does the library link libpng, and does it resolve? ---"
  ldd "$LIB" 2>&1 | grep -iE 'png|jpeg|not found' | sed 's/^/    /' >> "$LOG"
  say ""
  say "--- 2. what module directory was compiled in? ---"
  strings "$(readlink -f $LIB)" 2>/dev/null | grep -E 'gdk-pixbuf-2\.0/2\.10\.0|loaders\.cache|GDK_PIXBUF_MODULE' | sort -u | head | sed 's/^/    /' >> "$LOG"
  say ""
  say "--- 3. is a png loader compiled in as a builtin? ---"
  say "    internal loader symbols present in the .so:"
  strings "$(readlink -f $LIB)" 2>/dev/null | grep -oE 'gdk_pixbuf__[a-z0-9_]+_image_(begin_)?load' | sed 's/gdk_pixbuf__//; s/_image.*//' | sort -u | tr '\n' ' ' >> "$LOG"
  say ""
  say "    exported dynamic symbols matching those loaders:"
  nm -D "$(readlink -f $LIB)" 2>/dev/null | grep -c 'gdk_pixbuf__' >> "$LOG"
  say ""
  say "--- 4. loader modules actually shipped ---"
  ls "$PBD/loaders" 2>/dev/null | tr '\n' ' ' >> "$LOG"; say ""
  say "--- 5. what query-loaders reports ---"
  gdk-pixbuf-query-loaders 2>/dev/null | grep -E '^"' | grep -v '^"/' | head -20 | sed 's/^/    /' >> "$LOG"
  say ""
  say "--- 6. strace: every loader file the decoder tries to open ---"
  strace -f -e trace=openat,open,access -o /tmp/pb.st gdk-pixbuf-thumbnailer "$P" /tmp/o.png >/dev/null 2>&1
  say "    exit=$?  output=$(wc -c < /tmp/o.png 2>/dev/null || echo 0) bytes"
  say "    references to loaders/cache:"
  grep -E 'loaders|pixbuf' /tmp/pb.st 2>/dev/null | head -20 | sed 's/^/    /' >> "$LOG"
  say "    the sample file itself:"
  grep 'xxri-store.png' /tmp/pb.st 2>/dev/null | head -4 | sed 's/^/    /' >> "$LOG"
  say ""
  say "--- 7. same decode with the module dir forced ---"
  GDK_PIXBUF_MODULEDIR="$PBD/loaders" GDK_PIXBUF_MODULE_FILE="$PBD/loaders.cache" \
      gdk-pixbuf-thumbnailer "$P" /tmp/o2.png 2>>"$LOG"; say "    exit=$?"
  say ""
  say "--- 8. does a JPEG decode? (isolates png vs all builtins) ---"
  J=$(find /usr/local/share -name '*.jpg' 2>/dev/null | head -1)
  say "    sample: ${J:-none}"
  [ -n "$J" ] && { gdk-pixbuf-thumbnailer "$J" /tmp/o3.png 2>>"$LOG"; say "    exit=$?"; }
  say ""
  say "--- 9. does an SVG decode? (that loader IS a shipped module) ---"
  S=$(find /usr/local/share -name '*.svg' 2>/dev/null | head -1)
  say "    sample: ${S:-none}"
  [ -n "$S" ] && { gdk-pixbuf-thumbnailer "$S" /tmp/o4.png 2>>"$LOG"; say "    exit=$? out=$(wc -c < /tmp/o4.png 2>/dev/null || echo 0)"; }

  # ---------------- remaining apps, lean (no gdb) ----------------
  for spec in "vlc:/usr/local/bin/vlc:qt" \
              "code-oss:/opt/xxri/code-oss/VSCode-linux-ia32/code:electron" \
              "telegram:/opt/xxri/telegram/Telegram/Telegram:qt"; do
      ID="${spec%%:*}"; rest="${spec#*:}"; BIN="${rest%%:*}"; KIT="${rest##*:}"
      say ""
      say "################ $ID  ($BIN) ################"
      [ -x "$BIN" ] || { say "  NOT EXECUTABLE"; continue; }
      say "--- ldd ---"
      ldd "$BIN" > /tmp/l.$ID 2>&1
      say "  libs=$(grep -c '=>' /tmp/l.$ID)  NOT FOUND=$(grep -c 'not found' /tmp/l.$ID)"
      grep 'not found' /tmp/l.$ID | head -8 | sed 's/^/    /' >> "$LOG"
      say "--- direct run (20s) ---"
      ( cd "$HOME"; timeout 20 "$BIN" ) > /tmp/o.$ID 2>&1
      say "  EXIT CODE: $?"
      head -18 /tmp/o.$ID | sed 's/^/    /' >> "$LOG"
      case "$KIT" in
        qt) say "--- QT_DEBUG_PLUGINS=1 (platform plugin only) ---"
            ( cd "$HOME"; QT_DEBUG_PLUGINS=1 timeout 20 "$BIN" ) 2>&1 \
              | grep -iE 'platform|xcb|not a valid|cannot load|failed|found metadata' \
              | head -12 | sed 's/^/    /' >> "$LOG" ;;
        electron) say "--- electron --disable-gpu --no-sandbox ---"
            ( cd "$HOME"; timeout 20 "$BIN" --disable-gpu --no-sandbox ) > /tmp/e.$ID 2>&1
            say "  EXIT CODE: $?"; head -12 /tmp/e.$ID | sed 's/^/    /' >> "$LOG" ;;
      esac
      sync
  done
  say ""
  say "=== RT2 DONE ==="
  sync
) >/dev/null 2>&1 &
