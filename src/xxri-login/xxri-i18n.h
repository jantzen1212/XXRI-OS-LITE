/* xxri-i18n.h - gettext setup shared by the GTK programs.
 *
 * Define GETTEXT_PACKAGE (the program's name, also its catalog name), include
 * this file and call xxri_i18n_init() first thing in main().  Catalogs are
 * /usr/local/share/locale/<lang>/LC_MESSAGES/<GETTEXT_PACKAGE>.mo, see
 * docs/translating.md.  Use N_() for strings in static tables and _() on them
 * where they are shown.
 *
 * The copies in the programs' directories must stay identical
 * (tools/xxri-i18n check). */
#ifndef XXRI_I18N_H
#define XXRI_I18N_H
#include <locale.h>
#include <glib/gi18n-lib.h>

#ifndef GETTEXT_PACKAGE
#error "define GETTEXT_PACKAGE before including xxri-i18n.h"
#endif

/* gi18n-lib.h leaves ngettext() on the default domain */
#undef ngettext
#define ngettext(s, p, n) g_dngettext(GETTEXT_PACKAGE, s, p, n)

#define XXRI_LOCALEDIR "/usr/local/share/locale"

static inline void xxri_i18n_init(void) {
    /* a locale that is not installed leaves "C" */
    setlocale(LC_ALL, "");
    bindtextdomain(GETTEXT_PACKAGE, XXRI_LOCALEDIR);
    bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
}
#endif
