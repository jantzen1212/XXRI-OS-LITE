#!/bin/sh
NOAUTOLOGIN=/etc/sysconfig/noautologin
XXRI_USER="$(cat /etc/sysconfig/tcuser)"
if [ -f "$NOAUTOLOGIN" ]; then
	if [ -s "$NOAUTOLOGIN" ]; then
		> "$NOAUTOLOGIN"
		# login screen, if the user's files know it (older homes keep the
		# text prompt) and X is wanted
		H="/home/$XXRI_USER"
		if [ -x /usr/local/bin/xxri-login ] && [ -s /etc/sysconfig/Xserver ] &&
		   [ ! -f /etc/sysconfig/text ] &&
		   grep -q xxri-login "$H/.profile" 2>/dev/null &&
		   grep -q xxri-login "$H/.xsession" 2>/dev/null; then
			: > /tmp/.xxri-login
			clear
			exec /bin/login -f "$XXRI_USER"
		fi
		exit
	fi
else
	if [ ! -f /etc/sysconfig/superuser ]; then 
		rm -f /tmp/.xxri-login
		clear
		exec /bin/login -f "$XXRI_USER"
	fi
fi
