#!/bin/sh
# put other system startup commands here, the boot process will wait until they complete.
# Use bootlocal.sh for system startup commands that can run in the background
# and therefore not slow down the boot process.

# Apply the persistent hostname; seed the default only when it is unset.
# (sethostname rewrites /etc/hostname and /etc/hosts, so it must not run
# on every boot of an installed system where those files are user-owned.)
if [ -s /etc/hostname ]; then
	/bin/hostname -F /etc/hostname
else
	/usr/bin/sethostname box
fi
/opt/bootlocal.sh &
