#!/bin/sh
# put other system startup commands here

# xxri Hardware Enablement Layer: drivers, audio, network,
# persisted power settings.  Runs in the background; boot is not delayed.
[ -x /etc/init.d/xxri-hw-init ] && /etc/init.d/xxri-hw-init &

# QA ONLY (tools/qa/install-hook, never shipped): unattended install test.
grep -q xxriqa= /proc/cmdline && /opt/xxri-qa-install.sh >/dev/null 2>&1 &
