/* xxri-power-menu - the XXRI session/power dialog (Phase 10).
 *
 * Replaces Tiny Core's "TC Exit Options" window, which was the last
 * Tiny Core-branded dialog a user could reach from the dock.  Every action
 * goes through the Phase 7 power backend (xxri-power) or the session itself;
 * nothing here talks to the hardware directly.
 */
#include <gtk/gtk.h>
#include <cairo.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

#define CSS_PATH "/usr/local/share/xxri-control-center/xxri-cc.css"

static void css_class(GtkWidget* w, const char* c) {
    gchar** parts = g_strsplit(c, " ", -1);
    for (int i = 0; parts[i]; i++)
        if (*parts[i]) gtk_style_context_add_class(gtk_widget_get_style_context(w), parts[i]);
    g_strfreev(parts);
}
static char* run_cmd(const char* cmd) {
    char full[512]; snprintf(full, sizeof full, "%s 2>/dev/null", cmd);
    FILE* f = popen(full, "r"); if (!f) return g_strdup("");
    GString* s = g_string_new(""); char b[256]; size_t n;
    while ((n = fread(b,1,sizeof b,f)) > 0) g_string_append_len(s, b, n);
    pclose(f);
    return g_string_free(s, FALSE);
}
typedef struct { const char* kind; } Glyph;
static gboolean glyph_draw(GtkWidget* w, cairo_t* cr, gpointer data) {
    Glyph* g = data;
    int W = gtk_widget_get_allocated_width(w), H = gtk_widget_get_allocated_height(w);
    double s = MIN(W,H), cx = W/2.0, cy = H/2.0;
    cairo_pattern_t* pat = cairo_pattern_create_linear(cx-s/2, cy-s/2, cx+s/2, cy+s/2);
    cairo_pattern_add_color_stop_rgb(pat, 0, 0.51, 0.24, 0.93);
    cairo_pattern_add_color_stop_rgb(pat, 1, 0.93, 0.16, 0.78);
    cairo_set_source(cr, pat); cairo_pattern_destroy(pat);
    double r = s*0.30, x = cx-s/2, y = cy-s/2;
    cairo_new_sub_path(cr);
    cairo_arc(cr, x+s-r, y+r,   r, -G_PI/2, 0);
    cairo_arc(cr, x+s-r, y+s-r, r, 0, G_PI/2);
    cairo_arc(cr, x+r,   y+s-r, r, G_PI/2, G_PI);
    cairo_arc(cr, x+r,   y+r,   r, G_PI, 1.5*G_PI);
    cairo_close_path(cr); cairo_fill(cr);
    cairo_set_source_rgb(cr,1,1,1);
    cairo_set_line_width(cr, s*0.08); cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    if (!strcmp(g->kind,"power")) {
        cairo_arc(cr, cx, cy+s*0.03, s*0.22, -1.05, G_PI+1.05); cairo_stroke(cr);
        cairo_move_to(cr, cx, cy-s*0.28); cairo_line_to(cr, cx, cy-s*0.02); cairo_stroke(cr);
    } else if (!strcmp(g->kind,"restart")) {
        /* circular arrow: an arc with a gap, and a real arrowhead on the tangent */
        double R = s*0.22, a0 = -0.30*G_PI, a1 = 1.15*G_PI;
        cairo_arc(cr, cx, cy, R, a0, a1); cairo_stroke(cr);
        double ax = cx + cos(a0)*R, ay = cy + sin(a0)*R;
        double t  = a0 - G_PI/2;                  /* clockwise tangent */
        double hw = s*0.11;
        cairo_move_to(cr, ax + cos(t)*hw*1.5,       ay + sin(t)*hw*1.5);
        cairo_line_to(cr, ax + cos(t+2.4)*hw,       ay + sin(t+2.4)*hw);
        cairo_line_to(cr, ax + cos(t-2.4)*hw,       ay + sin(t-2.4)*hw);
        cairo_close_path(cr); cairo_fill(cr);
    } else if (!strcmp(g->kind,"sleep")) {
        cairo_arc(cr, cx+s*0.06, cy, s*0.24, 0.6, 4.2); cairo_stroke(cr);
    } else {   /* logout */
        cairo_rectangle(cr, cx-s*0.26, cy-s*0.22, s*0.30, s*0.44); cairo_stroke(cr);
        cairo_move_to(cr, cx+s*0.02, cy); cairo_line_to(cr, cx+s*0.28, cy); cairo_stroke(cr);
        cairo_move_to(cr, cx+s*0.16, cy-s*0.12); cairo_line_to(cr, cx+s*0.28, cy);
        cairo_line_to(cr, cx+s*0.16, cy+s*0.12); cairo_stroke(cr);
    }
    return TRUE;
}
static void do_action(GtkWidget* w, gpointer d) {
    (void)w;
    const char* a = d;
    if      (!strcmp(a,"shutdown")) { g_free(run_cmd("xxri-power shutdown")); }
    else if (!strcmp(a,"reboot"))   { g_free(run_cmd("xxri-power restart")); }
    else if (!strcmp(a,"suspend"))  { g_free(run_cmd("xxri-power suspend")); }
    else if (!strcmp(a,"logout"))   { g_free(run_cmd("pkill -x flwm")); }
    gtk_main_quit();
}
static GtkWidget* action(const char* kind, const char* title, const char* sub, const char* act) {
    GtkWidget* b = gtk_button_new();
    css_class(b, "xxri-pm-action");
    GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget* ic = gtk_drawing_area_new();
    gtk_widget_set_size_request(ic, 34, 34);
    Glyph* g = g_new0(Glyph,1); g->kind = kind;
    g_signal_connect_data(ic, "draw", G_CALLBACK(glyph_draw), g, (GClosureNotify)g_free, 0);
    gtk_box_pack_start(GTK_BOX(row), ic, FALSE, FALSE, 0);
    GtkWidget* tv = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
    gtk_widget_set_valign(tv, GTK_ALIGN_CENTER);
    GtkWidget* t = gtk_label_new(title); gtk_label_set_xalign(GTK_LABEL(t), 0);
    css_class(t, "xxri-pm-title");
    GtkWidget* s = gtk_label_new(sub); gtk_label_set_xalign(GTK_LABEL(s), 0);
    css_class(s, "xxri-pm-sub");
    gtk_box_pack_start(GTK_BOX(tv), t, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(tv), s, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), tv, TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(b), row);
    g_signal_connect(b, "clicked", G_CALLBACK(do_action), (gpointer)act);
    return b;
}
static gboolean on_key(GtkWidget* w, GdkEventKey* e, gpointer d) {
    (void)w; (void)d;
    if (e->keyval == GDK_KEY_Escape) gtk_main_quit();
    return FALSE;
}
int main(int argc, char** argv) {
    if (!gtk_init_check(&argc, &argv)) return 1;
    GtkCssProvider* p = gtk_css_provider_new();
    gtk_css_provider_load_from_path(p, CSS_PATH, NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(p), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(p);

    GtkWidget* win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "Power");
    gtk_window_set_position(GTK_WINDOW(win), GTK_WIN_POS_CENTER);
    gtk_window_set_resizable(GTK_WINDOW(win), FALSE);
    gtk_window_set_type_hint(GTK_WINDOW(win), GDK_WINDOW_TYPE_HINT_DIALOG);
    g_signal_connect(win, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(win, "key-press-event", G_CALLBACK(on_key), NULL);

    GtkWidget* col = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    css_class(col, "xxri-pm");
    gtk_container_add(GTK_CONTAINER(win), col);
    GtkWidget* h = gtk_label_new("Power");
    gtk_label_set_xalign(GTK_LABEL(h), 0); css_class(h, "xxri-pm-head");
    gtk_box_pack_start(GTK_BOX(col), h, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(col), action("power","Shut down","Save state and power off","shutdown"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(col), action("restart","Restart","Reboot this computer","reboot"), FALSE, FALSE, 0);
    /* only offer sleep when the kernel really supports it */
    char* caps = run_cmd("xxri-power capabilities --json");
    if (strstr(caps, "\"suspend\":true"))
        gtk_box_pack_start(GTK_BOX(col), action("sleep","Sleep","Suspend to RAM","suspend"), FALSE, FALSE, 0);
    g_free(caps);
    gtk_box_pack_start(GTK_BOX(col), action("logout","Log out","End this desktop session","logout"), FALSE, FALSE, 0);

    GtkWidget* cancel = gtk_button_new_with_label("Cancel");
    css_class(cancel, "xxri-pm-cancel");
    g_signal_connect(cancel, "clicked", G_CALLBACK(gtk_main_quit), NULL);
    gtk_box_pack_start(GTK_BOX(col), cancel, FALSE, FALSE, 0);

    gtk_widget_show_all(win);
    gtk_main();
    return 0;
}
