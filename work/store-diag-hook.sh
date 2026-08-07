# diagnostic: run the Store GUI headless-ish and capture why it does/doesn't draw
( sleep 6
  echo "DISPLAY=$DISPLAY PAGE=home  $(date)" > "$HOME/store-gui.log"
  which xxri-store-gui >> "$HOME/store-gui.log" 2>&1
  ldd /usr/local/bin/xxri-store-gui 2>&1 | grep -i 'not found' >> "$HOME/store-gui.log"
  timeout 18 /usr/local/bin/xxri-store-gui home >> "$HOME/store-gui.log" 2>&1
  echo "exit=$?" >> "$HOME/store-gui.log"
  sync
) &
