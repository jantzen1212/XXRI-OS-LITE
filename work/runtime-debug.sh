( LOG="$HOME/rt.log"; : > "$LOG"
  exec 2>>"$LOG"
  say(){ echo "$*" >> "$LOG"; sync; }

  i=0; while [ $i -lt 60 ]; do route -n 2>/dev/null | grep -q '^0.0.0.0' && break; sleep 2; i=$((i+2)); done
  say "=== RUNTIME BRING-UP EVIDENCE $(date) ==="
  say "kernel: $(uname -a)"
  say ""

  # Debugging tools only - these are not runtime dependencies of any app and
  # are loaded here purely to produce the evidence requested.
  say "--- loading debug tools ---"
  for t in strace gdb; do
      tce-load -wi "$t" >/dev/null 2>&1
      say "  $t: $(command -v $t || echo NOT AVAILABLE)"
  done
  say ""

  # ---------------------------------------------------------------- probe --
  probe() { # id  binary  toolkit
      ID="$1"; BIN="$2"; KIT="$3"
      say "################################################################"
      say "##### $ID"
      say "##### executable: $BIN"
      say "################################################################"
      if [ ! -x "$BIN" ]; then say "  FATAL: not executable"; return; fi
      say "  file: $(head -c 20 "$BIN" | hexdump -n 20 -e '20/1 \"%02x\"' 2>/dev/null)"

      # ---- STEP 2: every dependency must resolve --------------------------
      say ""
      say "--- STEP 2: ldd ---"
      ldd "$BIN" > /tmp/ldd.$ID 2>&1
      nf=$(grep -c 'not found' /tmp/ldd.$ID)
      say "  total libs : $(grep -c '=>' /tmp/ldd.$ID)"
      say "  NOT FOUND  : $nf"
      [ "$nf" -gt 0 ] && grep 'not found' /tmp/ldd.$ID | sed 's/^/    /' >> "$LOG"

      # ---- STEP 1: run it directly, capture everything --------------------
      say ""
      say "--- STEP 1: direct run (25s, stdout+stderr) ---"
      ( cd "$HOME"; timeout 25 "$BIN" ) > /tmp/out.$ID 2>&1
      rc=$?
      say "  EXIT CODE: $rc"
      say "  ---- output (first 40 lines) ----"
      head -40 /tmp/out.$ID | sed 's/^/    /' >> "$LOG"

      # ---- STEP 4: toolkit-specific diagnosis -----------------------------
      say ""
      say "--- STEP 4: toolkit debug ($KIT) ---"
      case "$KIT" in
        gtk) ( cd "$HOME"; GDK_DEBUG=misc GTK_DEBUG=modules,plugsocket timeout 20 "$BIN" ) \
                 > /tmp/kit.$ID 2>&1; say "  rc=$?" ;;
        qt)  ( cd "$HOME"; QT_DEBUG_PLUGINS=1 timeout 20 "$BIN" ) \
                 > /tmp/kit.$ID 2>&1; say "  rc=$?" ;;
        electron) ( cd "$HOME"; timeout 20 "$BIN" --disable-gpu --no-sandbox ) \
                 > /tmp/kit.$ID 2>&1; say "  rc=$?" ;;
      esac
      grep -iE 'plugin|module|cannot|could not|fail|error|no such|missing' /tmp/kit.$ID 2>/dev/null \
          | head -20 | sed 's/^/    /' >> "$LOG"

      # ---- STEP 3: first fatal ENOENT -------------------------------------
      if command -v strace >/dev/null 2>&1; then
          say ""
          say "--- STEP 3: strace, failed file opens ---"
          ( cd "$HOME"; timeout 25 strace -f -e trace=openat,open,access -o /tmp/st.$ID "$BIN" ) >/dev/null 2>&1
          say "  ENOENT count: $(grep -c ENOENT /tmp/st.$ID 2>/dev/null)"
          say "  --- ENOENT on libraries / plugins / loaders / themes ---"
          grep ENOENT /tmp/st.$ID 2>/dev/null \
            | grep -iE '\.so|loader|plugin|module|pixbuf|gtk-|qt|mesa|gl|dbus|fontconfig|icon|theme|mime|gsettings|schema' \
            | grep -viE '/proc/|/sys/|locale|\.cache/|/tmp/' \
            | head -25 | sed 's/^/    /' >> "$LOG"
          say "  --- last 8 syscalls before exit ---"
          tail -8 /tmp/st.$ID 2>/dev/null | sed 's/^/    /' >> "$LOG"
      fi

      # ---- STEP 5: backtrace if it died on a signal ------------------------
      if [ "$rc" -ge 128 ] || [ "$rc" = 134 ] || [ "$rc" = 139 ] || [ "$rc" = 11 ]; then
          if command -v gdb >/dev/null 2>&1; then
              say ""
              say "--- STEP 5: gdb backtrace (rc=$rc) ---"
              ( cd "$HOME"; timeout 60 gdb -batch -nx \
                  -ex 'set confirm off' -ex run -ex 'bt 15' -ex 'info sharedlibrary' \
                  --args "$BIN" ) 2>&1 | tail -30 | sed 's/^/    /' >> "$LOG"
          fi
      fi
      say ""
      sync
  }

  probe gimp      /usr/local/bin/gimp      gtk
  probe inkscape  /usr/local/bin/inkscape  gtk
  probe audacity  /usr/local/bin/audacity  gtk
  probe vlc       /usr/local/bin/vlc       qt
  probe code-oss  /opt/xxri/code-oss/VSCode-linux-ia32/code       electron
  probe telegram  /opt/xxri/telegram/Telegram/Telegram            qt

  say "=== RT DONE ==="
  sync
) >/dev/null 2>&1 &
