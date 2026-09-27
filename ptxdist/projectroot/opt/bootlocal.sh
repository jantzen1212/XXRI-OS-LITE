#!/bin/sh
# put other system startup commands here

# xxri Hardware Enablement Layer: drivers, audio, network,
# persisted power settings.  Runs in the background; boot is not delayed.
[ -x /etc/init.d/xxri-hw-init ] && /etc/init.d/xxri-hw-init &
