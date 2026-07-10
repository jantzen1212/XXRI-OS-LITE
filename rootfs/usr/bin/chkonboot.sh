#!/bin/sh
. /etc/init.d/xxri-functions
BOOTLIST=`getbootparam lst` || BOOTLIST="onboot.lst"
XXRI_PACKAGE_DIR=/etc/sysconfig/tcedir
[ -s "$XXRI_PACKAGE_DIR"/"$BOOTLIST" ] || exit 1
for B in $(cat "$XXRI_PACKAGE_DIR"/"$BOOTLIST")
do
	if [ -s "$XXRI_PACKAGE_DIR"/optional/"$B".dep ]
	then
		for D in $(cat "$XXRI_PACKAGE_DIR"/optional/"$B".dep)
		do
			if grep -q "^$D$" "$XXRI_PACKAGE_DIR"/"$BOOTLIST"
			then
				echo "$D" not needed a dep of "$B"
			fi
		done
	fi
done
echo "Scan of $BOOTLIST completed."
