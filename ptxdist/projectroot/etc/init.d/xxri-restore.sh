#!/bin/busybox ash
# (c) Robert Shingledecker 2003-2012
# Called from xxri-config
# A non-interactive script to restore configs, directories, etc defined by the user
# in the file .filetool.lst
. /etc/init.d/xxri-functions
useBusybox

# A disk-installed root persists natively; there is no backup archive to
# restore.  (xxri-config also skips the call; this guards direct invocation.)
if isDiskRoot; then
	echo "${GREEN}Disk root detected: skipping backup restore.${NORMAL}"
	exit 0
fi

TCE="$1"
DEVICE=""
MYDATA=mydata
[ -r /etc/sysconfig/mydata ] && read MYDATA < /etc/sysconfig/mydata
for i in `cat /proc/cmdline`; do
	case $i in
		*=*)
			case $i in
				restore*)
					RESTORE=1
					DEVICE=${i#*=}
				;;
			esac
		;;
		*)
			case $i in
				restore) RESTORE=1 ;;
				protect) PROTECT=1 ;;
			esac
		;;
	esac
done

if [ -n "$PROTECT" ]; then
	# Check if backup file is in TCE directory
	if [ -d "$TCE" ] && [ -f "$TCE"/"$MYDATA".tgz.bfe ]; then
		DEVICE="$(echo $TCE|cut -f3- -d/)"
	fi
	if [ -z "$DEVICE" ]; then
		DEVICE=`tc_autoscan "$MYDATA".tgz.bfe 'f'`
	fi
	if [ -n "$DEVICE" ]; then
		/usr/bin/xxri-backup.sh -r "$DEVICE"
		exit 0
	fi
fi

# Check if backup file is in TCE directory
if [ -d "$TCE" ] && [ -f "$TCE"/"$MYDATA".tgz ]; then
	XXRI_PACKAGE_DIR="$(echo $TCE|cut -f3- -d/)"
fi

if [ -z "$DEVICE" ]; then
	if [ -n "$XXRI_PACKAGE_DIR" ]; then
		DEVICE="$XXRI_PACKAGE_DIR"
	else
		DEVICE=`tc_autoscan "$MYDATA".tgz 'f'`
	fi
fi

if [ -n "$DEVICE" ]; then
	/usr/bin/xxri-backup.sh -r "$DEVICE"
	exit 0
fi

# Nothing found, set default backup location
# use persistent TCE directory
if [ "${TCE:0:8}" != "/tmp/tce" ]; then
	DEVICE="${TCE#/mnt/}"
	echo "$DEVICE" > /etc/sysconfig/backup_device
fi
