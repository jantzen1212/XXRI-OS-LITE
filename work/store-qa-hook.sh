# xxri Store Phase 9 end-to-end QA hook (injected into /home/xxri/.X.d).
# Runs the ENTIRE user journey through the real backends and logs each step to
# ~/store-qa.log, which the host reads back with debugfs.  Nothing here is
# Store-GUI code: it drives exactly the commands the GUI's buttons call.
(
  LOG="$HOME/store-qa.log"
  echo "=== xxri Store QA  $(date) ===" > "$LOG"
  echo "arch: $(xxri-store arch)" >> "$LOG"
  echo "repo(before): $(xxri-store repo-path)" >> "$LOG"

  # 1. OFFLINE install of the bundled i686 sample (download->verify->integrate)
  echo "--- install xxri-test (offline, bundled) ---" >> "$LOG"
  xxri-store install xxri-test >> "$LOG" 2>&1
  echo "installed list:" >> "$LOG"
  xxri-app list >> "$LOG" 2>&1
  echo "registry version: $(xxri-app info xxri-test 2>/dev/null | sed -n 's/^VERSION=//p')" >> "$LOG"

  # 2. LAUNCH it (proves the integrated AppImage actually runs)
  echo "--- launch xxri-test ---" >> "$LOG"
  xxri-app launch xxri-test >> "$LOG" 2>&1
  sleep 3
  if pgrep -f 'XXRI Test App' >/dev/null 2>&1; then echo "LAUNCH: running" >> "$LOG"; else echo "LAUNCH: not detected" >> "$LOG"; fi

  # 3. REMOTE repository: point at the host test server, refresh, detect update
  echo "--- refresh from test remote (10.0.2.2:8099) ---" >> "$LOG"
  export REMOTE_BASE="http://10.0.2.2:8099/store"
  xxri-store refresh --force >> "$LOG" 2>&1
  echo "repo(after): $(xxri-store repo-path)" >> "$LOG"
  echo "catalog xxri-test version: $(xxri-store field xxri-test version)" >> "$LOG"
  echo "updates JSON: $(xxri-store updates)" >> "$LOG"

  # 4. UPDATE to 1.1.0 (HTTP download from the remote + verify + re-integrate)
  echo "--- update xxri-test -> 1.1.0 ---" >> "$LOG"
  xxri-store install xxri-test >> "$LOG" 2>&1
  echo "registry version now: $(xxri-app info xxri-test 2>/dev/null | sed -n 's/^VERSION=//p')" >> "$LOG"

  # 5. REMOVE it through the backend the Store's Remove button uses
  echo "--- remove xxri-test ---" >> "$LOG"
  xxri-app remove xxri-test --purge >> "$LOG" 2>&1
  echo "installed after remove:" >> "$LOG"
  xxri-app list >> "$LOG" 2>&1

  echo "=== QA DONE ===" >> "$LOG"
  sync
) >/dev/null 2>&1 &
