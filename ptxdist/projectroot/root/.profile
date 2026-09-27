#!/bin/sh
NOAUTOLOGIN=/etc/sysconfig/noautologin
if [ -f "$NOAUTOLOGIN" ]; then
	if [ -s "$NOAUTOLOGIN" ]; then
		> "$NOAUTOLOGIN"
		exit
	fi
else
	if [ ! -f /etc/sysconfig/superuser ]; then 
		clear
		XXRI_USER="$(cat /etc/sysconfig/tcuser)"
		exec /bin/login -f "$XXRI_USER"
	fi
fi
