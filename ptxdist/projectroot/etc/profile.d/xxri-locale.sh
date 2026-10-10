# Locale environment of the login session.
#
# Tiny Core's /etc/profile sources /etc/sysconfig/language and exports LANG and
# LC_ALL; export the other categories the file may set.
export LC_CTYPE LC_NUMERIC LC_TIME LC_COLLATE LC_MONETARY LC_MESSAGES LC_PAPER \
	LC_NAME LC_ADDRESS LC_TELEPHONE LC_MEASUREMENT LC_IDENTIFICATION

# /etc/profile exports G_FILENAME_ENCODING=iso8859-1, so GLib (GTK, the file
# chooser) reads UTF-8 file names as Latin-1: "ção.txt" is listed as
# "Ã§Ã£o.txt", and a ç typed into a name is stored as one non-UTF-8 byte.
export G_FILENAME_ENCODING=UTF-8
