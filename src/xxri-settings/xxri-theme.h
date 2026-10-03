/* xxri-theme.h - Light or Dark, as chosen in Settings > Wallpaper & Style.
 *
 *   xxri_theme_init(dark_css, changed, data)   after the app's own stylesheet
 *   xxri_theme_dark()                          for what the app draws itself
 *
 * xxri-desktop writes the mode to ~/.config/xxri/appearance ("light" or
 * "dark"). This reads it at start and keeps watching it, so open apps switch
 * along with Settings. Dark means GTK's prefer-dark (stock widgets use the
 * xxri theme's gtk-dark.css) and the app's dark sheet above its own, which
 * redefines the app's named colours. `changed` runs after every switch, for
 * surfaces the app paints with cairo.
 *
 * Identical copies live in settings, store, media, launcher, control center,
 * installer, power menu and Files - keep them in sync. */
#ifndef XXRI_THEME_H
#define XXRI_THEME_H
#include <gtk/gtk.h>
#include <string.h>

typedef void (*XxriThemeChanged)(gboolean dark, gpointer data);

static struct {
    gboolean dark;
    char* sheet;                         /* the app's dark stylesheet */
    GtkCssProvider* prov;
    GFileMonitor* mon;
    XxriThemeChanged changed;
    gpointer data;
} xxri_theme_s;

static gboolean xxri_theme_dark(void) { return xxri_theme_s.dark; }

static char* xxri_theme_path(void) {
    return g_build_filename(g_get_user_config_dir(), "xxri", "appearance", NULL);
}
static gboolean xxri_theme_read(void) {
    char* p = xxri_theme_path();
    char* s = NULL;
    gboolean dark = FALSE;
    if (g_file_get_contents(p, &s, NULL, NULL)) dark = strstr(s, "dark") != NULL;
    g_free(s); g_free(p);
    return dark;
}
static void xxri_theme_apply(gboolean dark) {
    xxri_theme_s.dark = dark;
    g_object_set(gtk_settings_get_default(), "gtk-application-prefer-dark-theme", dark, NULL);
    GdkScreen* scr = gdk_screen_get_default();
    if (dark && !xxri_theme_s.prov && xxri_theme_s.sheet) {
        xxri_theme_s.prov = gtk_css_provider_new();
        gtk_css_provider_load_from_path(xxri_theme_s.prov, xxri_theme_s.sheet, NULL);
        /* above the app's sheet (APPLICATION or +1) */
        gtk_style_context_add_provider_for_screen(scr, GTK_STYLE_PROVIDER(xxri_theme_s.prov),
                                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 2);
    } else if (!dark && xxri_theme_s.prov) {
        gtk_style_context_remove_provider_for_screen(scr, GTK_STYLE_PROVIDER(xxri_theme_s.prov));
        g_object_unref(xxri_theme_s.prov);
        xxri_theme_s.prov = NULL;
    }
}
static void xxri_theme_event(GFileMonitor* m, GFile* f, GFile* o, GFileMonitorEvent ev, gpointer u) {
    (void)m; (void)f; (void)o; (void)u;
    if (ev != G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT && ev != G_FILE_MONITOR_EVENT_CREATED &&
        ev != G_FILE_MONITOR_EVENT_DELETED && ev != G_FILE_MONITOR_EVENT_CHANGED) return;
    gboolean dark = xxri_theme_read();
    if (dark == xxri_theme_s.dark) return;
    xxri_theme_apply(dark);
    if (xxri_theme_s.changed) xxri_theme_s.changed(dark, xxri_theme_s.data);
}
static void xxri_theme_init(const char* dark_css, XxriThemeChanged changed, gpointer data) {
    xxri_theme_s.sheet = g_strdup(dark_css);
    xxri_theme_s.changed = changed;
    xxri_theme_s.data = data;
    xxri_theme_apply(xxri_theme_read());
    char* p = xxri_theme_path();
    GFile* f = g_file_new_for_path(p);
    /* the file is replaced as a whole (rename): watch the name, not the inode */
    xxri_theme_s.mon = g_file_monitor_file(f, G_FILE_MONITOR_NONE, NULL, NULL);
    if (xxri_theme_s.mon) g_signal_connect(xxri_theme_s.mon, "changed", G_CALLBACK(xxri_theme_event), NULL);
    g_object_unref(f); g_free(p);
}
#endif
