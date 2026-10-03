/* xxri-installer - the graphical installer (GTK 3).
 *
 * Frontend only: disk work is done by /usr/sbin/xxri-install-backend (which
 * wraps install2disk). Backend stdout: P|pct|msg  L|detail  E|error  DONE|uuid
 *
 * Fullscreen ARGB window that paints nothing itself; the glass cards are
 * blended by the compositor over an opaque wallpaper window underneath, so
 * other open apps never show through. Without a compositor it is one window
 * painting the root pixmap's wallpaper.
 */
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <cairo-xlib.h>
#include <math.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "xxri-dialog.h"
#include "xxri-theme.h"

#define BACKEND_DEFAULT "/usr/sbin/xxri-install-backend"
#define DISK_ICON "/usr/local/share/icons/xxri/64x64/apps/storage.png"

static const char CSS[] =
    "@define-color xi_ink    #1C1C1E;"
    "@define-color xi_muted  #5F5F68;"
    "@define-color xi_accent #2E7CF6;"
    "@define-color xi_blue_d #1E4FC2;"
    "@define-color xi_blue_l #4A9AF5;"
    "@define-color xi_bad    #D13438;"
    "@define-color xi_good   #1E9E46;"
    "* { font-family: \"Avenir Next\", \"Helvetica\", sans-serif; outline: none; }"
    ".xi-window { background-color: transparent; color: @xi_ink; }"
    /* the glass card every page is made of */
    ".xi-card {"
    "  background-color: alpha(#FFFFFF, 0.80);"
    "  border: 1px solid alpha(#FFFFFF, 0.75); border-radius: 22px;"
    "  box-shadow: 0 12px 34px alpha(#000000, 0.18), 0 1px 3px alpha(#000000, 0.08);"
    "  padding: 26px 34px 24px 34px; }"
    ".xi-card.tight { padding: 16px 22px; border-radius: 18px; }"
    ".xi-hero { font-size: 58pt; font-weight: 800; color: @xi_ink;"
    "  text-shadow: 0 1px 16px alpha(#FFFFFF, 0.65); }"
    ".xi-hero-sub { font-size: 13.5pt; color: #3A3A40;"
    "  text-shadow: 0 1px 10px alpha(#FFFFFF, 0.70); }"
    ".xi-title { font-size: 16.5pt; font-weight: 700; color: @xi_ink; }"
    ".xi-sub   { font-size: 9.5pt; color: @xi_muted; }"
    ".xi-label { font-size: 9pt; font-weight: 600; color: @xi_ink; }"
    ".xi-hint  { font-size: 8.25pt; color: @xi_muted; }"
    ".xi-hint.bad  { color: @xi_bad; }"
    ".xi-hint.good { color: @xi_good; }"
    ".xi-note  { font-size: 8.625pt; color: @xi_muted; }"
    ".xi-note.warn { color: #9A5B00; }"
    ".xi-op    { font-size: 10.5pt; font-weight: 600; color: @xi_ink; }"
    ".xi-mono  { font-size: 7.5pt; color: @xi_muted; }"
    "entry {"
    "  border-radius: 10px; background-color: #FFFFFF; background-image: none;"
    "  border: 1px solid alpha(#000000, 0.14); padding: 7px 12px; min-height: 20px;"
    "  color: @xi_ink; caret-color: @xi_accent; font-size: 9.75pt; box-shadow: none; }"
    "entry:focus { border-color: @xi_accent; box-shadow: 0 0 0 3px alpha(@xi_accent, 0.20); }"
    "entry.bad { border-color: @xi_bad; }"
    "entry.bad:focus { box-shadow: 0 0 0 3px alpha(@xi_bad, 0.16); }"
    "selection { background-color: @xi_accent; color: #FFFFFF; }"
    /* secondary buttons are neutral, the one primary action is XXRI blue */
    "button {"
    "  border-radius: 999px; padding: 8px 22px; min-height: 20px;"
    "  background-image: none; background-color: #E6E6EA; border: none; box-shadow: none;"
    "  color: @xi_ink; font-size: 9.75pt; font-weight: 600;"
    "  transition: background-color 140ms ease; }"
    "button:hover  { background-color: #DCDCE1; }"
    "button:active { background-color: #D2D2D8; }"
    "button:disabled { color: alpha(@xi_ink, 0.40); }"
    "button.xi-primary {"
    "  background-image: linear-gradient(to right, @xi_blue_d, @xi_blue_l); color: #FFFFFF; }"
    "button.xi-primary:hover  { background-image: linear-gradient(to right, #2459D0, #5AA5F8); }"
    "button.xi-primary:active { background-image: linear-gradient(to right, #1A45AB, #3F8BE2); }"
    "button.xi-primary:disabled {"
    "  background-image: none; background-color: #D9D9DE; color: alpha(@xi_ink, 0.40); }"
    "button.xi-wide { padding: 10px 22px; font-size: 10.5pt; }"
    /* the dropdown: a white field with a chevron */
    "button.xi-drop {"
    "  border-radius: 10px; padding: 7px 10px 7px 12px; background-color: #FFFFFF;"
    "  border: 1px solid alpha(#000000, 0.14); font-weight: 500; }"
    "button.xi-drop:hover { background-color: #FAFAFB; }"
    ".xi-drop-chevron { color: @xi_muted; }"
    ".xi-drop-current label { font-weight: bold; }"
    /* segmented control: a grey track, the chosen segment lifted */
    ".xi-seg { background-color: alpha(#000000, 0.07); border-radius: 999px; padding: 3px; }"
    ".xi-seg button { background-color: transparent; color: @xi_muted; padding: 5px 18px; }"
    ".xi-seg button:hover { color: @xi_ink; }"
    ".xi-seg button.on { background-color: #FFFFFF; color: @xi_ink;"
    "  box-shadow: 0 1px 3px alpha(#000000, 0.16); }"
    /* a target disk */
    "button.xi-disk {"
    "  border-radius: 16px; padding: 10px 14px; background-color: alpha(#FFFFFF, 0.70);"
    "  border: 1.5px solid transparent; font-weight: 400; }"
    "button.xi-disk:hover { background-color: #FFFFFF; }"
    "button.xi-disk.on { background-color: #FFFFFF; border-color: @xi_accent; }"
    ".xi-disk-name { font-size: 10.125pt; font-weight: 700; color: @xi_ink; }"
    ".xi-disk-sub  { font-size: 8.625pt; color: @xi_muted; }"
    "switch { border-radius: 999px; background-image: none; background-color: #D9D9DE;"
    "  border: none; min-width: 46px; min-height: 26px; }"
    "switch:checked { background-image: linear-gradient(to right, @xi_blue_d, @xi_blue_l); }"
    "switch image { color: transparent; }"
    "switch slider { background-color: #FFFFFF; border-radius: 999px; border: none;"
    "  box-shadow: 0 1px 3px alpha(#000000, 0.25); min-width: 22px; min-height: 22px; margin: 2px; }"
    ".xi-step { font-size: 9.75pt; color: @xi_muted; }"
    ".xi-step.now  { color: @xi_ink; font-weight: 700; }"
    ".xi-step.done { color: @xi_ink; }"
    ".xi-err-title { font-size: 10.5pt; font-weight: 700; color: @xi_bad; }"
    ".xi-err-text  { font-size: 9pt; color: @xi_ink; }"
    "menu { background-color: #FFFFFF; border: 1px solid alpha(#000000, 0.10);"
    "  border-radius: 12px; padding: 6px; }"
    "menuitem { border-radius: 8px; padding: 6px 12px; color: @xi_ink; font-size: 9.75pt; }"
    "menuitem:hover { background-color: #ECECEF; color: @xi_ink; }";

/* ------------------------------------------------------------------ state */
typedef struct { char dev[64], model[128], fs[224]; long mib; int removable; } Disk;

static GtkWidget *win, *stack;
static GtkWidget *dr_region, *dr_lang, *bt_welcome_next, *bt_restart;
static GtkWidget *en_user, *en_host, *en_pw, *en_pw2, *sw_auto;
static GtkWidget *hi_user, *hi_host, *hi_pw, *bt_acc_next;
static GtkWidget *seg_full, *seg_man, *box_disks, *lb_dest_note, *bt_install;
static GtkWidget *ring, *lb_op, *box_steps, *rv_err, *lb_err;
static GtkWidget *lb_uuid;
static GArray    *g_disks, *g_parts;
static gboolean   g_manual;
static char       g_sel[64];
static gboolean   g_host_edited;       /* the user typed a computer name */
static gboolean   g_host_setting;      /* ... as opposed to us deriving it */
static FILE      *g_backend;
static gboolean   g_composited;
static cairo_surface_t *g_wall;        /* root wallpaper copy (no compositor) */
static cairo_surface_t *g_diskicon;

/* progress indicator */
static double  g_pct_target, g_pct_shown, g_phase;
static int     g_step = -1;
static guint   g_anim;
static gboolean g_running;

typedef struct { const char *label, *tz; } TzRegion;
static const TzRegion REGIONS[] = {
    {"Indonesia / Jakarta",    "WIB-7"},
    {"Indonesia / Makassar",   "WITA-8"},
    {"Indonesia / Jayapura",   "WIT-9"},
    {"UTC",                    "UTC0"},
    {"United States / Eastern","EST5EDT,M3.2.0,M11.1.0"},
    {"United States / Central","CST6CDT,M3.2.0,M11.1.0"},
    {"United States / Pacific","PST8PDT,M3.2.0,M11.1.0"},
    {"United Kingdom / London","GMT0BST,M3.5.0/1,M10.5.0"},
    {"Europe / Berlin",        "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe / Moscow",        "MSK-3"},
    {"India / New Delhi",      "IST-5:30"},
    {"China / Beijing",        "CST-8"},
    {"Japan / Tokyo",          "JST-9"},
    {"Singapore",              "SGT-8"},
    {"Australia / Sydney",     "AEST-10AEDT,M10.1.0,M4.1.0/3"},
    {"Brazil / Sao Paulo",     "BRT3"},
};
typedef struct { const char *label, *code; } Lang;
static const Lang LANGS[] = {
    {"English (United States)", "en_US"},
    {"English (United Kingdom)","en_GB"},
    {"Bahasa Indonesia",        "id_ID"},
};
/* backend phases; task_for_pct() must match xxri-install-backend's P| values */
static const char* TASKS[] = {
    "Prepare the disk", "Format the XXRI partition", "Copy system files",
    "Create your account", "Set name and language", "Add apps and extensions",
    "Install the boot loader", "Finish up" };
#define NTASKS ((int)G_N_ELEMENTS(TASKS))
static int task_for_pct(int p) {
    if (p < 10) return 0;
    if (p < 14) return 1;
    if (p < 80) return 2;
    if (p < 84) return 3;
    if (p < 86) return 4;
    if (p < 90) return 5;
    if (p < 97) return 6;
    return 7;
}

/* ---------------------------------------------------------------- helpers */
static void css(GtkWidget* w, const char* c) {
    gtk_style_context_add_class(gtk_widget_get_style_context(w), c);
}
static void uncss(GtkWidget* w, const char* c) {
    gtk_style_context_remove_class(gtk_widget_get_style_context(w), c);
}
static GtkWidget* label(const char* t, const char* cls, double xalign) {
    GtkWidget* l = gtk_label_new(t);
    gtk_label_set_xalign(GTK_LABEL(l), xalign);
    if (cls) css(l, cls);
    return l;
}
static GtkWidget* wrapped(const char* t, const char* cls, double xalign, int chars) {
    GtkWidget* l = label(t, cls, xalign);
    gtk_label_set_line_wrap(GTK_LABEL(l), TRUE);
    gtk_label_set_max_width_chars(GTK_LABEL(l), chars);
    gtk_label_set_justify(GTK_LABEL(l), xalign > 0.4 ? GTK_JUSTIFY_CENTER : GTK_JUSTIFY_LEFT);
    return l;
}
static GtkWidget* button(const char* t, gboolean primary, GCallback cb) {
    GtkWidget* b = gtk_button_new_with_label(t);
    if (primary) css(b, "xi-primary");
    if (cb) g_signal_connect(b, "clicked", cb, NULL);
    return b;
}
static void blue(cairo_t* cr, double x0, double y0, double x1, double y1) {
    cairo_pattern_t* p = cairo_pattern_create_linear(x0, y0, x1, y1);
    cairo_pattern_add_color_stop_rgb(p, 0, 0.118, 0.310, 0.761);   /* #1E4FC2 */
    cairo_pattern_add_color_stop_rgb(p, 1, 0.290, 0.604, 0.961);   /* #4A9AF5 */
    cairo_set_source(cr, p);
    cairo_pattern_destroy(p);
}
static void focus_page(const char* name);
static void show_page(const char* name) {
    gtk_stack_set_visible_child_name(GTK_STACK(stack), name);
    focus_page(name);
}
static const char* page(void) {
    const char* n = gtk_stack_get_visible_child_name(GTK_STACK(stack));
    return n ? n : "";
}
static const char* backend(void) {
    /* QA seam: lets a test drive the pages with a scripted backend */
    const char* b = g_getenv("XXRI_INSTALL_BACKEND");
    return b && *b ? b : BACKEND_DEFAULT;
}

/* ---- dropdown with a cairo chevron: GtkComboBox's arrow comes from the
 * icon theme, which the live system can't load */
typedef struct { GtkWidget* label; GPtrArray* labels; int active; } Drop;
static void drop_free(gpointer p) { Drop* d = p; g_ptr_array_unref(d->labels); g_free(d); }
static Drop* drop_of(GtkWidget* w) { return g_object_get_data(G_OBJECT(w), "xi-drop"); }
static gboolean chevron_draw(GtkWidget* w, cairo_t* cr, gpointer u) {
    (void)u;
    double W = gtk_widget_get_allocated_width(w), H = gtk_widget_get_allocated_height(w);
    GdkRGBA c;
    gtk_style_context_get_color(gtk_widget_get_style_context(w), gtk_widget_get_state_flags(w), &c);
    gdk_cairo_set_source_rgba(cr, &c);
    cairo_set_line_width(cr, 1.7);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND); cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    cairo_move_to(cr, W/2 - 4, H/2 - 1.5); cairo_line_to(cr, W/2, H/2 + 2.5);
    cairo_line_to(cr, W/2 + 4, H/2 - 1.5);
    cairo_stroke(cr);
    return FALSE;
}
static void drop_set(GtkWidget* w, int i) {
    Drop* d = drop_of(w);
    if (i < 0 || i >= (int)d->labels->len) return;
    d->active = i;
    gtk_label_set_text(GTK_LABEL(d->label), d->labels->pdata[i]);
}
static int drop_get(GtkWidget* w) { return drop_of(w)->active; }
static void drop_pick(GtkMenuItem* it, gpointer u) {
    drop_set(GTK_WIDGET(u), GPOINTER_TO_INT(g_object_get_data(G_OBJECT(it), "i")));
}
static void drop_clicked(GtkButton* b, gpointer u) {
    (void)u;
    GtkWidget* w = GTK_WIDGET(b); Drop* d = drop_of(w);
    if (d->labels->len < 2) return;
    GtkWidget* m = gtk_menu_new();
    for (guint i = 0; i < d->labels->len; i++) {
        GtkWidget* it = gtk_menu_item_new_with_label(d->labels->pdata[i]);
        if ((int)i == d->active) css(it, "xi-drop-current");
        g_object_set_data(G_OBJECT(it), "i", GINT_TO_POINTER(i));
        g_signal_connect(it, "activate", G_CALLBACK(drop_pick), w);
        gtk_menu_shell_append(GTK_MENU_SHELL(m), it);
    }
    gtk_widget_show_all(m);
    gtk_widget_set_size_request(m, gtk_widget_get_allocated_width(w), -1);
    gtk_menu_attach_to_widget(GTK_MENU(m), w, NULL);
    gtk_menu_popup_at_widget(GTK_MENU(m), w, GDK_GRAVITY_SOUTH_WEST, GDK_GRAVITY_NORTH_WEST, NULL);
}
static GtkWidget* drop_new(void) {
    GtkWidget* b = gtk_button_new(); css(b, "xi-drop");
    Drop* d = g_new0(Drop, 1);
    d->labels = g_ptr_array_new_with_free_func(g_free);
    GtkWidget* hb = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    d->label = label("", NULL, 0);
    gtk_label_set_ellipsize(GTK_LABEL(d->label), PANGO_ELLIPSIZE_END);
    gtk_box_pack_start(GTK_BOX(hb), d->label, TRUE, TRUE, 0);
    GtkWidget* ch = gtk_drawing_area_new();
    gtk_widget_set_size_request(ch, 14, 12); gtk_widget_set_valign(ch, GTK_ALIGN_CENTER);
    css(ch, "xi-drop-chevron");
    g_signal_connect(ch, "draw", G_CALLBACK(chevron_draw), NULL);
    gtk_box_pack_end(GTK_BOX(hb), ch, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(b), hb);
    g_object_set_data_full(G_OBJECT(b), "xi-drop", d, drop_free);
    g_signal_connect(b, "clicked", G_CALLBACK(drop_clicked), NULL);
    return b;
}
static void drop_add(GtkWidget* w, const char* text) {
    Drop* d = drop_of(w);
    g_ptr_array_add(d->labels, g_strdup(text));
    if (d->labels->len == 1) drop_set(w, 0);
}

/* ---- the session
 * The installer owns the screen: dock and Control Center chip are stopped,
 * and only come back on "Continue live session". */
static gboolean wbar_kill_cb(gpointer u) {
    (void)u;
    system("pkill -x wbar >/dev/null 2>&1");
    return G_SOURCE_REMOVE;
}
static void session_exclusive(void) {
    system("pkill -x wbar >/dev/null 2>&1");
    system("xxri-control-center --quit >/dev/null 2>&1");
    g_timeout_add(2500, wbar_kill_cb, NULL);   /* catch a late wbar spawn from .xsession */
    g_timeout_add(5000, wbar_kill_cb, NULL);
}
static void session_restore(void) {
    system("U=$(cat /etc/sysconfig/tcuser 2>/dev/null); [ -n \"$U\" ] && "
           "su - \"$U\" -c 'export DISPLAY=:0.0; nohup wbar.sh >/dev/null 2>&1 & "
           "nohup xxri-control-center >/dev/null 2>&1 &'");
}

/* ------------------------------------------------------------ the backdrop */
static cairo_surface_t* root_wallpaper(int w, int h) {
    Display* xd = GDK_DISPLAY_XDISPLAY(gdk_display_get_default());
    Atom prop = XInternAtom(xd, "_XROOTPMAP_ID", True);
    if (prop == None) return NULL;
    Atom type; int fmt; unsigned long n = 0, after = 0; unsigned char* p = NULL;
    if (XGetWindowProperty(xd, DefaultRootWindow(xd), prop, 0, 1, False, XA_PIXMAP,
                           &type, &fmt, &n, &after, &p) != Success || !p || n < 1) {
        if (p) XFree(p);
        return NULL;
    }
    Pixmap pm = *(Pixmap*)p;
    XFree(p);
    if (!pm) return NULL;
    cairo_surface_t* src = cairo_xlib_surface_create(xd, pm,
                               DefaultVisual(xd, DefaultScreen(xd)), w, h);
    cairo_surface_t* out = cairo_image_surface_create(CAIRO_FORMAT_RGB24, w, h);
    cairo_t* c = cairo_create(out);
    cairo_set_source_surface(c, src, 0, 0);
    cairo_paint(c);
    cairo_destroy(c);
    cairo_surface_destroy(src);
    return out;
}
/* drawing colours that follow the Mode: the ink, and a faint mark (black on
 * the light glass, white on the dark) */
static void ink(cairo_t* cr, double a) {
    if (xxri_theme_dark()) cairo_set_source_rgba(cr, 0.949, 0.949, 0.957, a);
    else cairo_set_source_rgba(cr, 0.11, 0.11, 0.118, a);
}
static void faint(cairo_t* cr, double a) {
    if (xxri_theme_dark()) cairo_set_source_rgba(cr, 1, 1, 1, a * 1.25);
    else cairo_set_source_rgba(cr, 0, 0, 0, a);
}
static void paint_wall(cairo_t* cr) {
    cairo_save(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    if (g_wall) cairo_set_source_surface(cr, g_wall, 0, 0);
    else if (xxri_theme_dark()) cairo_set_source_rgb(cr, 0.141, 0.129, 0.141);
    else        cairo_set_source_rgb(cr, 0.91, 0.93, 0.95);
    cairo_paint(cr);
    cairo_restore(cr);
}
static gboolean back_draw(GtkWidget* w, cairo_t* cr, gpointer u) {
    (void)w; (void)u;
    paint_wall(cr);
    return TRUE;
}
static gboolean win_draw(GtkWidget* w, cairo_t* cr, gpointer u) {
    (void)w; (void)u;
    if (!g_composited) { paint_wall(cr); return FALSE; }
    cairo_save(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, 0, 0, 0, 0);          /* the backdrop window shows */
    cairo_paint(cr);
    cairo_restore(cr);
    return FALSE;
}

/* ---------------------------------------------------------------- welcome */
static void cb_welcome_next(GtkWidget* w, gpointer u) { (void)w; (void)u; show_page("account"); }

static GtkWidget* form_row(const char* name, GtkWidget* field) {
    GtkWidget* r = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 14);
    GtkWidget* l = label(name, "xi-label", 0);
    gtk_widget_set_size_request(l, 84, -1);
    gtk_box_pack_start(GTK_BOX(r), l, FALSE, FALSE, 0);
    gtk_widget_set_size_request(field, 250, -1);
    gtk_box_pack_start(GTK_BOX(r), field, TRUE, TRUE, 0);
    return r;
}
static GtkWidget* build_welcome(void) {
    GtkWidget* v = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_halign(v, GTK_ALIGN_CENTER); gtk_widget_set_valign(v, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(v), label("Welcome", "xi-hero", 0.5), FALSE, FALSE, 0);
    GtkWidget* s = label(NULL, "xi-hero-sub", 0.5);
    gtk_label_set_markup(GTK_LABEL(s),
        "to <span weight=\"bold\" foreground=\"#1E4FC2\">XXRI OS Lite</span>");
    gtk_box_pack_start(GTK_BOX(v), s, FALSE, FALSE, 0);

    GtkWidget* card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    css(card, "xi-card"); css(card, "tight");
    gtk_widget_set_margin_top(card, 54);
    dr_region = drop_new();
    for (guint i = 0; i < G_N_ELEMENTS(REGIONS); i++) drop_add(dr_region, REGIONS[i].label);
    dr_lang = drop_new();
    for (guint i = 0; i < G_N_ELEMENTS(LANGS); i++) drop_add(dr_lang, LANGS[i].label);
    GtkWidget* kbd = drop_new(); drop_add(kbd, "US (default)");
    gtk_box_pack_start(GTK_BOX(card), form_row("Region", dr_region), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(card), form_row("Language", dr_lang), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(card), form_row("Keyboard", kbd), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(v), card, FALSE, FALSE, 0);

    GtkWidget* nx = bt_welcome_next = button("Continue", TRUE, G_CALLBACK(cb_welcome_next));
    css(nx, "xi-wide");
    gtk_widget_set_halign(nx, GTK_ALIGN_CENTER);
    gtk_widget_set_size_request(nx, 170, -1);
    gtk_widget_set_margin_top(nx, 26);
    gtk_box_pack_start(GTK_BOX(v), nx, FALSE, FALSE, 0);
    return v;
}

/* ---------------------------------------------------------------- account */
/* the rules the backend's adduser and hostname accept */
static gboolean name_ok(const char* s, gboolean user) {
    if (!*s) return FALSE;
    for (const char* p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (user) { if (!(islower(c) || isdigit(c) || c == '-' || c == '_')) return FALSE; }
        else      { if (!(isalnum(c) || c == '-')) return FALSE; }
    }
    return !user || !isdigit((unsigned char)*s);
}
static void hint(GtkWidget* l, const char* text, const char* state) {
    gtk_label_set_text(GTK_LABEL(l), text);
    uncss(l, "bad"); uncss(l, "good");
    if (state) css(l, state);
}
static gboolean account_valid(void) {
    const char* u  = gtk_entry_get_text(GTK_ENTRY(en_user));
    const char* h  = gtk_entry_get_text(GTK_ENTRY(en_host));
    const char* p1 = gtk_entry_get_text(GTK_ENTRY(en_pw));
    const char* p2 = gtk_entry_get_text(GTK_ENTRY(en_pw2));
    return name_ok(u, TRUE) && name_ok(h, FALSE) && !strcmp(p1, p2);
}
/* hint under each field: what it's for, or what to fix */
static void account_refresh(void) {
    const char* u  = gtk_entry_get_text(GTK_ENTRY(en_user));
    const char* h  = gtk_entry_get_text(GTK_ENTRY(en_host));
    const char* p1 = gtk_entry_get_text(GTK_ENTRY(en_pw));
    const char* p2 = gtk_entry_get_text(GTK_ENTRY(en_pw2));

    gboolean uok = name_ok(u, TRUE);
    if (uok) { hint(hi_user, "You sign in with this name. It is also the name of your home folder.", NULL); uncss(en_user, "bad"); }
    else {
        hint(hi_user, !*u ? "Type a name to sign in with, for example your first name."
                     : isdigit((unsigned char)*u) ? "The name cannot start with a number."
                     : "Use lowercase letters and numbers only, without spaces.", "bad");
        css(en_user, "bad");
    }
    gboolean hok = name_ok(h, FALSE);
    if (hok) { hint(hi_host, "How this computer is called on a network.", NULL); uncss(en_host, "bad"); }
    else {
        hint(hi_host, !*h ? "Give this computer a name, for example pc-xxri."
                          : "Use letters, numbers and hyphens only, without spaces.", "bad");
        css(en_host, "bad");
    }
    size_t n = strlen(p1);
    if (strcmp(p1, p2)) {
        hint(hi_pw, !*p2 ? "Type the same password again to confirm it."
                         : "The two passwords are not the same yet.", *p2 ? "bad" : NULL);
        if (*p2) css(en_pw2, "bad"); else uncss(en_pw2, "bad");
    } else {
        uncss(en_pw2, "bad");
        if (!n)          hint(hi_pw, "Optional. Leave both empty to use this computer without a password.", NULL);
        else if (n < 6)  hint(hi_pw, "The passwords match. Short password - 6 or more characters is safer.", NULL);
        else             hint(hi_pw, n < 10 ? "The passwords match. Good password."
                                            : "The passwords match. Strong password.", "good");
    }
    gtk_widget_set_sensitive(bt_acc_next, uok && hok && !strcmp(p1, p2));
}
static void cb_user_changed(GtkEditable* e, gpointer u) {
    (void)u;
    const char* t = gtk_entry_get_text(GTK_ENTRY(e));
    /* capitals are never what is meant here: take them as lowercase */
    gboolean upper = FALSE;
    for (const char* p = t; *p; p++) if (isupper((unsigned char)*p)) upper = TRUE;
    if (upper) {
        char* low = g_ascii_strdown(t, -1);
        int pos = gtk_editable_get_position(e);
        gtk_entry_set_text(GTK_ENTRY(e), low);         /* re-enters this handler */
        gtk_editable_set_position(e, pos);
        g_free(low);
        return;
    }
    /* until the user names the computer, name it after them */
    if (!g_host_edited && name_ok(t, TRUE)) {
        char* h = g_strdup_printf("pc-%s", t);
        for (char* p = h; *p; p++) if (*p == '_') *p = '-';
        g_host_setting = TRUE;
        gtk_entry_set_text(GTK_ENTRY(en_host), h);
        g_host_setting = FALSE;
        g_free(h);
    }
    account_refresh();
}
static void cb_host_changed(GtkEditable* e, gpointer u) {
    (void)e; (void)u;
    if (!g_host_setting) g_host_edited = TRUE;
    account_refresh();
}
static void cb_pw_changed(GtkEditable* e, gpointer u) { (void)e; (void)u; account_refresh(); }
static void cb_account_next(GtkWidget* w, gpointer u) {
    (void)w; (void)u;
    if (!account_valid()) { account_refresh(); return; }
    show_page("dest");
}
static void cb_account_back(GtkWidget* w, gpointer u) { (void)w; (void)u; show_page("welcome"); }

static GtkWidget* field(const char* name, GtkWidget* entry) {
    GtkWidget* v = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_pack_start(GTK_BOX(v), label(name, "xi-label", 0), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(v), entry, FALSE, FALSE, 0);
    return v;
}
static GtkWidget* build_account(void) {
    GtkWidget* card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    css(card, "xi-card");
    gtk_widget_set_halign(card, GTK_ALIGN_CENTER); gtk_widget_set_valign(card, GTK_ALIGN_CENTER);
    gtk_widget_set_size_request(card, 520, -1);

    gtk_box_pack_start(GTK_BOX(card), label("Create your account", "xi-title", 0.5), FALSE, FALSE, 0);
    GtkWidget* s = wrapped("This is the account you will use on this computer.", "xi-sub", 0.5, 60);
    gtk_widget_set_margin_top(s, 4); gtk_widget_set_margin_bottom(s, 18);
    gtk_box_pack_start(GTK_BOX(card), s, FALSE, FALSE, 0);

    en_user = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(en_user), "xxri");
    gtk_entry_set_max_length(GTK_ENTRY(en_user), 32);
    gtk_box_pack_start(GTK_BOX(card), field("Username", en_user), FALSE, FALSE, 0);
    hi_user = wrapped("", "xi-hint", 0, 70);
    gtk_widget_set_margin_top(hi_user, 4); gtk_widget_set_margin_bottom(hi_user, 12);
    gtk_box_pack_start(GTK_BOX(card), hi_user, FALSE, FALSE, 0);

    en_host = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(en_host), "pc-xxri");
    gtk_entry_set_max_length(GTK_ENTRY(en_host), 63);
    gtk_box_pack_start(GTK_BOX(card), field("Computer name", en_host), FALSE, FALSE, 0);
    hi_host = wrapped("", "xi-hint", 0, 70);
    gtk_widget_set_margin_top(hi_host, 4); gtk_widget_set_margin_bottom(hi_host, 12);
    gtk_box_pack_start(GTK_BOX(card), hi_host, FALSE, FALSE, 0);

    GtkWidget* pwrow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 14);
    gtk_box_set_homogeneous(GTK_BOX(pwrow), TRUE);
    en_pw = gtk_entry_new();  gtk_entry_set_visibility(GTK_ENTRY(en_pw), FALSE);
    en_pw2 = gtk_entry_new(); gtk_entry_set_visibility(GTK_ENTRY(en_pw2), FALSE);
    gtk_entry_set_input_purpose(GTK_ENTRY(en_pw), GTK_INPUT_PURPOSE_PASSWORD);
    gtk_entry_set_input_purpose(GTK_ENTRY(en_pw2), GTK_INPUT_PURPOSE_PASSWORD);
    gtk_box_pack_start(GTK_BOX(pwrow), field("Password", en_pw), TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(pwrow), field("Confirm password", en_pw2), TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(card), pwrow, FALSE, FALSE, 0);
    hi_pw = wrapped("", "xi-hint", 0, 70);
    gtk_widget_set_margin_top(hi_pw, 4); gtk_widget_set_margin_bottom(hi_pw, 14);
    gtk_box_pack_start(GTK_BOX(card), hi_pw, FALSE, FALSE, 0);

    GtkWidget* ar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget* av = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_box_pack_start(GTK_BOX(av), label("Sign in automatically", "xi-label", 0), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(av), label("Go straight to the desktop when the computer starts.", "xi-hint", 0), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(ar), av, TRUE, TRUE, 0);
    sw_auto = gtk_switch_new();
    gtk_switch_set_active(GTK_SWITCH(sw_auto), TRUE);
    gtk_widget_set_valign(sw_auto, GTK_ALIGN_CENTER);
    gtk_box_pack_end(GTK_BOX(ar), sw_auto, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(card), ar, FALSE, FALSE, 0);

    GtkWidget* br = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_margin_top(br, 22);
    gtk_box_pack_start(GTK_BOX(br), button("Back", FALSE, G_CALLBACK(cb_account_back)), FALSE, FALSE, 0);
    bt_acc_next = button("Continue", TRUE, G_CALLBACK(cb_account_next));
    gtk_widget_set_size_request(bt_acc_next, 130, -1);
    gtk_box_pack_end(GTK_BOX(br), bt_acc_next, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(card), br, FALSE, FALSE, 0);

    g_signal_connect(en_user, "changed", G_CALLBACK(cb_user_changed), NULL);
    g_signal_connect(en_host, "changed", G_CALLBACK(cb_host_changed), NULL);
    g_signal_connect(en_pw,   "changed", G_CALLBACK(cb_pw_changed), NULL);
    g_signal_connect(en_pw2,  "changed", G_CALLBACK(cb_pw_changed), NULL);
    account_refresh();
    return card;
}

/* ------------------------------------------------------------ destination */
static void load_devices(void) {
    g_array_set_size(g_disks, 0); g_array_set_size(g_parts, 0);
    char cmd[512], l[512];
    snprintf(cmd, sizeof cmd, "%s list-disks", backend());
    FILE* f = popen(cmd, "r");
    if (f) {
        while (fgets(l, sizeof l, f)) {
            Disk d; char fs[128]; memset(&d, 0, sizeof d);
            if (sscanf(l, "DISK|%63[^|]|%127[^|]|%ld|%d|%127[^|\n]", d.dev, d.model, &d.mib, &d.removable, fs) == 5) {
                snprintf(d.fs, sizeof d.fs, "Current content: %s", fs);
                g_array_append_val(g_disks, d);
            }
        }
        pclose(f);
    }
    snprintf(cmd, sizeof cmd, "%s list-parts", backend());
    f = popen(cmd, "r");
    if (f) {
        while (fgets(l, sizeof l, f)) {
            Disk d; char fs[64], lab[64]; memset(&d, 0, sizeof d); lab[0] = 0;
            int k = sscanf(l, "PART|%63[^|]|%ld|%63[^|]|%63[^|\n]", d.dev, &d.mib, fs, lab);
            if (k >= 3) {
                g_strlcpy(d.model, d.dev, sizeof d.model);
                if (lab[0]) snprintf(d.fs, sizeof d.fs, "%s  label %s  -  will be formatted ext4", fs, lab);
                else        snprintf(d.fs, sizeof d.fs, "%s  -  will be formatted ext4", fs);
                g_array_append_val(g_parts, d);
            }
        }
        pclose(f);
    }
}
static gboolean icon_draw(GtkWidget* w, cairo_t* cr, gpointer u) {
    (void)u;
    double S = gtk_widget_get_allocated_width(w);
    if (!g_diskicon) return FALSE;
    double iw = cairo_image_surface_get_width(g_diskicon);
    cairo_scale(cr, S / iw, S / iw);
    cairo_set_source_surface(cr, g_diskicon, 0, 0);
    cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_GOOD);
    cairo_paint(cr);
    return FALSE;
}
/* the radio mark at the end of a disk row */
static gboolean radio_draw(GtkWidget* w, cairo_t* cr, gpointer u) {
    const char* dev = u;
    double W = gtk_widget_get_allocated_width(w), H = gtk_widget_get_allocated_height(w);
    double cx = W/2, cy = H/2, r = 9;
    if (!strcmp(dev, g_sel)) {
        blue(cr, cx - r, cy - r, cx + r, cy + r);
        cairo_arc(cr, cx, cy, r, 0, 2*G_PI); cairo_fill(cr);
        cairo_set_source_rgb(cr, 1, 1, 1);
        cairo_arc(cr, cx, cy, 3.4, 0, 2*G_PI); cairo_fill(cr);
    } else {
        faint(cr, 0.26);
        cairo_set_line_width(cr, 1.5);
        cairo_arc(cr, cx, cy, r - 0.75, 0, 2*G_PI); cairo_stroke(cr);
    }
    return FALSE;
}
static void dest_note(void) {
    GArray* v = g_manual ? g_parts : g_disks;
    uncss(lb_dest_note, "warn");
    if (!v->len) { gtk_label_set_text(GTK_LABEL(lb_dest_note), ""); }
    else if (g_manual)
        gtk_label_set_text(GTK_LABEL(lb_dest_note),
            "Only the selected partition is formatted. The rest of the disk is left as it is.");
    else {
        gtk_label_set_text(GTK_LABEL(lb_dest_note),
            "Everything on the selected disk will be erased.");
        css(lb_dest_note, "warn");
    }
    gtk_widget_set_sensitive(bt_install, g_sel[0] != 0);
}
static void cb_diskrow(GtkWidget* b, gpointer u) {
    (void)u;
    g_strlcpy(g_sel, g_object_get_data(G_OBJECT(b), "dev"), sizeof g_sel);
    GList* kids = gtk_container_get_children(GTK_CONTAINER(box_disks));
    for (GList* l = kids; l; l = l->next) {
        const char* d = g_object_get_data(G_OBJECT(l->data), "dev");
        if (!d) continue;
        if (!strcmp(d, g_sel)) css(l->data, "on"); else uncss(l->data, "on");
        gtk_widget_queue_draw(l->data);
    }
    g_list_free(kids);
    dest_note();
}
static void rebuild_disklist(void) {
    GList* kids = gtk_container_get_children(GTK_CONTAINER(box_disks));
    for (GList* l = kids; l; l = l->next) gtk_widget_destroy(l->data);
    g_list_free(kids);

    GArray* v = g_manual ? g_parts : g_disks;
    GdkScreen* sc = gdk_screen_get_default();
    /* as many rows as the screen has room for under the heading */
    guint maxrows = (guint)MAX(1, (gdk_screen_get_height(sc) - 430) / 76);
    guint shown = MIN(v->len, maxrows);
    for (guint i = 0; i < shown; i++) {
        Disk* d = &g_array_index(v, Disk, i);
        GtkWidget* b = gtk_button_new(); css(b, "xi-disk");
        g_object_set_data_full(G_OBJECT(b), "dev", g_strdup(d->dev), g_free);
        GtkWidget* h = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 14);
        GtkWidget* ic = gtk_drawing_area_new();
        gtk_widget_set_size_request(ic, 40, 40); gtk_widget_set_valign(ic, GTK_ALIGN_CENTER);
        g_signal_connect(ic, "draw", G_CALLBACK(icon_draw), NULL);
        gtk_box_pack_start(GTK_BOX(h), ic, FALSE, FALSE, 0);
        GtkWidget* tv = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
        gtk_widget_set_valign(tv, GTK_ALIGN_CENTER);
        char l1[200];
        snprintf(l1, sizeof l1, "%s   %.1f GiB%s", d->model, d->mib / 1024.0,
                 d->removable ? "   (removable)" : "");
        GtkWidget* n = label(l1, "xi-disk-name", 0);
        gtk_label_set_ellipsize(GTK_LABEL(n), PANGO_ELLIPSIZE_END);
        GtkWidget* s = label(d->fs, "xi-disk-sub", 0);
        gtk_label_set_ellipsize(GTK_LABEL(s), PANGO_ELLIPSIZE_END);
        gtk_box_pack_start(GTK_BOX(tv), n, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(tv), s, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(h), tv, TRUE, TRUE, 0);
        GtkWidget* rd = gtk_drawing_area_new();
        gtk_widget_set_size_request(rd, 22, 22); gtk_widget_set_valign(rd, GTK_ALIGN_CENTER);
        g_signal_connect_data(rd, "draw", G_CALLBACK(radio_draw), g_strdup(d->dev),
                              (GClosureNotify)(void (*)(void))g_free, 0);
        gtk_box_pack_end(GTK_BOX(h), rd, FALSE, FALSE, 0);
        gtk_container_add(GTK_CONTAINER(b), h);
        if (!strcmp(d->dev, g_sel)) css(b, "on");
        g_signal_connect(b, "clicked", G_CALLBACK(cb_diskrow), NULL);
        gtk_box_pack_start(GTK_BOX(box_disks), b, FALSE, FALSE, 0);
    }
    if (v->len > shown) {
        char more[64];
        snprintf(more, sizeof more, "...and %u more device(s)", v->len - shown);
        gtk_box_pack_start(GTK_BOX(box_disks), label(more, "xi-note", 0.5), FALSE, FALSE, 0);
    }
    if (!v->len)
        gtk_box_pack_start(GTK_BOX(box_disks),
            wrapped(g_manual ? "No partitions were found. Choose \"Erase disk\" to let XXRI prepare a disk."
                             : "No disk to install on was detected.", "xi-note", 0.5, 56),
            FALSE, FALSE, 8);
    gtk_widget_show_all(box_disks);
    dest_note();
}
static void cb_seg(GtkWidget* w, gpointer u) {
    (void)u;
    gboolean man = (w == seg_man);
    if (man == g_manual) return;
    g_manual = man;
    if (g_manual) { css(seg_man, "on"); uncss(seg_full, "on"); }
    else          { css(seg_full, "on"); uncss(seg_man, "on"); }
    g_sel[0] = 0;
    GArray* v = g_manual ? g_parts : g_disks;
    if (v->len) g_strlcpy(g_sel, g_array_index(v, Disk, 0).dev, sizeof g_sel);
    rebuild_disklist();
}
static void start_install(void);
static void cb_dest_next(GtkWidget* w, gpointer u) { (void)w; (void)u; if (g_sel[0]) start_install(); }
static void cb_dest_back(GtkWidget* w, gpointer u) { (void)w; (void)u; show_page("account"); }

static GtkWidget* build_dest(void) {
    GtkWidget* card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    css(card, "xi-card");
    gtk_widget_set_halign(card, GTK_ALIGN_CENTER); gtk_widget_set_valign(card, GTK_ALIGN_CENTER);
    gtk_widget_set_size_request(card, 540, -1);

    gtk_box_pack_start(GTK_BOX(card), label("Where should XXRI be installed?", "xi-title", 0.5), FALSE, FALSE, 0);
    GtkWidget* s = wrapped("Erase a whole disk, or choose a partition you prepared yourself.", "xi-sub", 0.5, 60);
    gtk_widget_set_margin_top(s, 4); gtk_widget_set_margin_bottom(s, 16);
    gtk_box_pack_start(GTK_BOX(card), s, FALSE, FALSE, 0);

    GtkWidget* seg = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    css(seg, "xi-seg"); gtk_widget_set_halign(seg, GTK_ALIGN_CENTER);
    gtk_box_set_homogeneous(GTK_BOX(seg), TRUE);
    seg_full = button("Erase disk", FALSE, G_CALLBACK(cb_seg));
    seg_man  = button("Choose partition", FALSE, G_CALLBACK(cb_seg));
    css(seg_full, "on");
    gtk_box_pack_start(GTK_BOX(seg), seg_full, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(seg), seg_man, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(card), seg, FALSE, FALSE, 0);

    box_disks = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_top(box_disks, 16);
    gtk_box_pack_start(GTK_BOX(card), box_disks, FALSE, FALSE, 0);

    lb_dest_note = wrapped("", "xi-note", 0.5, 64);
    gtk_widget_set_margin_top(lb_dest_note, 14);
    gtk_box_pack_start(GTK_BOX(card), lb_dest_note, FALSE, FALSE, 0);

    GtkWidget* br = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_margin_top(br, 18);
    gtk_box_pack_start(GTK_BOX(br), button("Back", FALSE, G_CALLBACK(cb_dest_back)), FALSE, FALSE, 0);
    bt_install = button("Install", TRUE, G_CALLBACK(cb_dest_next));
    gtk_widget_set_size_request(bt_install, 130, -1);
    gtk_box_pack_end(GTK_BOX(br), bt_install, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(card), br, FALSE, FALSE, 0);
    return card;
}

/* ---- progress ring
 * The arc eases to each new value and a highlight travels along it while the
 * backend works, so a long step doesn't look stalled. The 40ms timer only
 * runs during an install. */
#define RING 164
static gboolean ring_draw(GtkWidget* w, cairo_t* cr, gpointer u) {
    (void)u;
    double W = gtk_widget_get_allocated_width(w), H = gtk_widget_get_allocated_height(w);
    double cx = W/2, cy = H/2, lw = 11, r = MIN(W, H)/2 - lw/2 - 3;
    const double A0 = -G_PI/2;
    double span = 2*G_PI * CLAMP(g_pct_shown, 0, 100) / 100.0;

    cairo_set_line_width(cr, lw);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    faint(cr, 0.08);
    cairo_arc(cr, cx, cy, r, 0, 2*G_PI); cairo_stroke(cr);

    if (span > 0.002) {
        blue(cr, cx - r, cy + r, cx + r, cy - r);
        cairo_arc(cr, cx, cy, r, A0, A0 + span); cairo_stroke(cr);
        if (g_running && span > 0.5) {          /* the travelling highlight */
            double head = A0 + span * g_phase, len = MIN(0.55, span * 0.5);
            cairo_pattern_t* p = cairo_pattern_create_radial(
                cx + r*cos(head), cy + r*sin(head), 0, cx + r*cos(head), cy + r*sin(head), r*0.9);
            cairo_pattern_add_color_stop_rgba(p, 0, 1, 1, 1, 0.42);
            cairo_pattern_add_color_stop_rgba(p, 1, 1, 1, 1, 0.0);
            cairo_set_source(cr, p);
            cairo_arc(cr, cx, cy, r, MAX(A0, head - len), MIN(A0 + span, head + len*0.35));
            cairo_stroke(cr);
            cairo_pattern_destroy(p);
        }
    }

    char t[16]; snprintf(t, sizeof t, "%d", (int)(g_pct_shown + 0.5));
    PangoLayout* lay = gtk_widget_create_pango_layout(w, t);
    PangoFontDescription* fd = pango_font_description_from_string("Avenir Next Bold 25");
    pango_layout_set_font_description(lay, fd);
    int tw, th; pango_layout_get_pixel_size(lay, &tw, &th);
    PangoLayout* pc = gtk_widget_create_pango_layout(w, "%");
    PangoFontDescription* fp = pango_font_description_from_string("Avenir Next Bold 11");
    pango_layout_set_font_description(pc, fp);
    int pw, ph; pango_layout_get_pixel_size(pc, &pw, &ph);
    ink(cr, 1.0);
    cairo_move_to(cr, cx - (tw + pw + 2)/2.0, cy - th/2.0);
    pango_cairo_show_layout(cr, lay);
    ink(cr, 0.62);
    cairo_move_to(cr, cx - (tw + pw + 2)/2.0 + tw + 2, cy - th/2.0 + (th - ph) * 0.68);
    pango_cairo_show_layout(cr, pc);
    pango_font_description_free(fd); pango_font_description_free(fp);
    g_object_unref(lay); g_object_unref(pc);
    return FALSE;
}
static gboolean anim_tick(gpointer u) {
    (void)u;
    double d = g_pct_target - g_pct_shown;
    if (fabs(d) > 0.05) g_pct_shown += d * 0.16; else g_pct_shown = g_pct_target;
    g_phase += 0.022; if (g_phase > 1.25) g_phase = -0.15;
    gtk_widget_queue_draw(ring);
    if (!g_running && g_pct_shown == g_pct_target) { g_anim = 0; return G_SOURCE_REMOVE; }
    return G_SOURCE_CONTINUE;
}
static void anim_start(void) { if (!g_anim) g_anim = g_timeout_add(40, anim_tick, NULL); }

/* a step's mark: done (blue disc, check), in hand (blue ring), to do (grey dot) */
static gboolean step_draw(GtkWidget* w, cairo_t* cr, gpointer u) {
    int i = GPOINTER_TO_INT(u);
    double W = gtk_widget_get_allocated_width(w), H = gtk_widget_get_allocated_height(w);
    double cx = W/2, cy = H/2;
    if (i < g_step) {
        blue(cr, cx - 8, cy - 8, cx + 8, cy + 8);
        cairo_arc(cr, cx, cy, 8, 0, 2*G_PI); cairo_fill(cr);
        cairo_set_source_rgb(cr, 1, 1, 1);
        cairo_set_line_width(cr, 1.8);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND); cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
        cairo_move_to(cr, cx - 3.6, cy + 0.2); cairo_line_to(cr, cx - 1.0, cy + 2.8);
        cairo_line_to(cr, cx + 3.8, cy - 2.6); cairo_stroke(cr);
    } else if (i == g_step) {
        cairo_set_source_rgb(cr, 0.180, 0.486, 0.965);
        cairo_set_line_width(cr, 2);
        cairo_arc(cr, cx, cy, 7, 0, 2*G_PI); cairo_stroke(cr);
        cairo_arc(cr, cx, cy, 3, 0, 2*G_PI); cairo_fill(cr);
    } else {
        faint(cr, 0.20);
        cairo_arc(cr, cx, cy, 3, 0, 2*G_PI); cairo_fill(cr);
    }
    return FALSE;
}
static void steps_refresh(void) {
    GList* kids = gtk_container_get_children(GTK_CONTAINER(box_steps));
    int i = 0;
    for (GList* l = kids; l; l = l->next, i++) {
        GtkWidget* lb = g_object_get_data(G_OBJECT(l->data), "label");
        uncss(lb, "now"); uncss(lb, "done");
        if (i < g_step) css(lb, "done"); else if (i == g_step) css(lb, "now");
        gtk_widget_queue_draw(l->data);
    }
    g_list_free(kids);
}
static void cb_err_close(GtkWidget* w, gpointer u) {
    (void)w; (void)u;
    gtk_revealer_set_reveal_child(GTK_REVEALER(rv_err), FALSE);
    show_page("dest");
}
static GtkWidget* build_prog(void) {
    GtkWidget* v = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_widget_set_halign(v, GTK_ALIGN_CENTER); gtk_widget_set_valign(v, GTK_ALIGN_CENTER);

    GtkWidget* card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    css(card, "xi-card");
    gtk_widget_set_size_request(card, 500, -1);
    gtk_box_pack_start(GTK_BOX(card), label("Installing XXRI OS Lite", "xi-title", 0.5), FALSE, FALSE, 0);
    GtkWidget* s = label("This takes a few minutes. You can leave the computer to it.", "xi-sub", 0.5);
    gtk_widget_set_margin_top(s, 4);
    gtk_box_pack_start(GTK_BOX(card), s, FALSE, FALSE, 0);

    ring = gtk_drawing_area_new();
    gtk_widget_set_size_request(ring, RING, RING);
    gtk_widget_set_halign(ring, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_top(ring, 20);
    g_signal_connect(ring, "draw", G_CALLBACK(ring_draw), NULL);
    gtk_box_pack_start(GTK_BOX(card), ring, FALSE, FALSE, 0);

    lb_op = label("Preparing...", "xi-op", 0.5);
    gtk_label_set_ellipsize(GTK_LABEL(lb_op), PANGO_ELLIPSIZE_END);
    gtk_widget_set_margin_top(lb_op, 12); gtk_widget_set_margin_bottom(lb_op, 14);
    gtk_box_pack_start(GTK_BOX(card), lb_op, FALSE, FALSE, 0);

    box_steps = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_halign(box_steps, GTK_ALIGN_CENTER);
    for (int i = 0; i < NTASKS; i++) {
        GtkWidget* r = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
        GtkWidget* m = gtk_drawing_area_new();
        gtk_widget_set_size_request(m, 18, 18); gtk_widget_set_valign(m, GTK_ALIGN_CENTER);
        g_signal_connect(m, "draw", G_CALLBACK(step_draw), GINT_TO_POINTER(i));
        GtkWidget* lb = label(TASKS[i], "xi-step", 0);
        gtk_widget_set_size_request(lb, 220, -1);
        gtk_box_pack_start(GTK_BOX(r), m, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(r), lb, FALSE, FALSE, 0);
        g_object_set_data(G_OBJECT(r), "label", lb);
        gtk_box_pack_start(GTK_BOX(box_steps), r, FALSE, FALSE, 0);
    }
    gtk_box_pack_start(GTK_BOX(card), box_steps, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(v), card, FALSE, FALSE, 0);

    rv_err = gtk_revealer_new();
    gtk_revealer_set_transition_type(GTK_REVEALER(rv_err), GTK_REVEALER_TRANSITION_TYPE_CROSSFADE);
    gtk_revealer_set_transition_duration(GTK_REVEALER(rv_err), 180);
    GtkWidget* ec = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    css(ec, "xi-card"); css(ec, "tight");
    gtk_widget_set_size_request(ec, 500, -1);
    gtk_box_pack_start(GTK_BOX(ec), label("Something went wrong", "xi-err-title", 0), FALSE, FALSE, 0);
    lb_err = wrapped("", "xi-err-text", 0, 66);
    gtk_label_set_selectable(GTK_LABEL(lb_err), FALSE);
    gtk_box_pack_start(GTK_BOX(ec), lb_err, FALSE, FALSE, 0);
    GtkWidget* cl = button("Close", FALSE, G_CALLBACK(cb_err_close));
    gtk_widget_set_halign(cl, GTK_ALIGN_END);
    gtk_box_pack_start(GTK_BOX(ec), cl, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(rv_err), ec);
    gtk_box_pack_start(GTK_BOX(v), rv_err, FALSE, FALSE, 0);
    return v;
}

/* ----------------------------------------------------------------- finish */
static void cb_restart(GtkWidget* w, gpointer u)  { (void)w; (void)u; sync(); execlp("/sbin/reboot", "reboot", (char*)0); }
static void cb_poweroff(GtkWidget* w, gpointer u) { (void)w; (void)u; sync(); execlp("/sbin/poweroff", "poweroff", (char*)0); }
static void cb_live(GtkWidget* w, gpointer u)     { (void)w; (void)u; session_restore(); exit(0); }
static gboolean badge_draw(GtkWidget* w, cairo_t* cr, gpointer u) {
    (void)u;
    double S = gtk_widget_get_allocated_width(w), c = S/2;
    blue(cr, 0, S, S, 0);
    cairo_arc(cr, c, c, c - 1, 0, 2*G_PI); cairo_fill(cr);
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_set_line_width(cr, 4.4);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND); cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    cairo_move_to(cr, c - 11, c + 1); cairo_line_to(cr, c - 3.5, c + 8.5);
    cairo_line_to(cr, c + 12, c - 8); cairo_stroke(cr);
    return FALSE;
}
static GtkWidget* build_finish(void) {
    GtkWidget* card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    css(card, "xi-card");
    gtk_widget_set_halign(card, GTK_ALIGN_CENTER); gtk_widget_set_valign(card, GTK_ALIGN_CENTER);
    gtk_widget_set_size_request(card, 460, -1);

    GtkWidget* bd = gtk_drawing_area_new();
    gtk_widget_set_size_request(bd, 58, 58); gtk_widget_set_halign(bd, GTK_ALIGN_CENTER);
    g_signal_connect(bd, "draw", G_CALLBACK(badge_draw), NULL);
    gtk_box_pack_start(GTK_BOX(card), bd, FALSE, FALSE, 0);
    GtkWidget* t = label("All done", "xi-title", 0.5);
    gtk_widget_set_margin_top(t, 14);
    gtk_box_pack_start(GTK_BOX(card), t, FALSE, FALSE, 0);
    GtkWidget* m = wrapped("XXRI OS Lite is installed on this computer. Remove the installation "
                           "medium, then restart to begin using it.", "xi-sub", 0.5, 52);
    gtk_widget_set_margin_top(m, 6); gtk_widget_set_margin_bottom(m, 20);
    gtk_box_pack_start(GTK_BOX(card), m, FALSE, FALSE, 0);

    GtkWidget* col = gtk_box_new(GTK_ORIENTATION_VERTICAL, 9);
    gtk_widget_set_halign(col, GTK_ALIGN_CENTER);
    gtk_widget_set_size_request(col, 270, -1);
    GtkWidget* r = bt_restart = button("Restart now", TRUE, G_CALLBACK(cb_restart)); css(r, "xi-wide");
    gtk_box_pack_start(GTK_BOX(col), r, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(col), button("Shut down", FALSE, G_CALLBACK(cb_poweroff)), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(col), button("Continue live session", FALSE, G_CALLBACK(cb_live)), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(card), col, FALSE, FALSE, 0);

    lb_uuid = label("", "xi-mono", 0.5);
    gtk_label_set_ellipsize(GTK_LABEL(lb_uuid), PANGO_ELLIPSIZE_MIDDLE);
    gtk_widget_set_margin_top(lb_uuid, 18);
    gtk_box_pack_start(GTK_BOX(card), lb_uuid, FALSE, FALSE, 0);
    return card;
}

/* ---------------------------------------------------------------- backend */
static void backend_line(const char* line) {
    if (!strncmp(line, "P|", 2)) {
        int pct = atoi(line + 2);
        const char* msg = strchr(line + 2, '|'); msg = msg ? msg + 1 : "";
        g_pct_target = CLAMP(pct, 0, 100);
        char* op = g_strdup_printf("%s...", msg);
        gtk_label_set_text(GTK_LABEL(lb_op), op);
        g_free(op);
        int t = task_for_pct(pct);
        if (t != g_step) { g_step = t; steps_refresh(); }
        anim_start();
    } else if (!strncmp(line, "E|", 2)) {
        char* em = g_strdup_printf("%s\n\nOnly the target disk was touched. Close to review and retry.", line + 2);
        gtk_label_set_text(GTK_LABEL(lb_err), em);
        g_free(em);
        g_running = FALSE;
        gtk_label_set_text(GTK_LABEL(lb_op), "The installation stopped.");
        gtk_revealer_set_reveal_child(GTK_REVEALER(rv_err), TRUE);
    } else if (!strncmp(line, "DONE|", 5)) {
        char* u = g_strdup_printf("Root filesystem  LABEL=XXRIROOT  -  UUID %s", line + 5);
        gtk_label_set_text(GTK_LABEL(lb_uuid), u);
        g_free(u);
        g_running = FALSE;
        show_page("finish");
    }
}
static gboolean backend_io(GIOChannel* ch, GIOCondition cond, gpointer u) {
    (void)cond; (void)u;
    static char buf[1024]; static int len = 0;
    int n = read(g_io_channel_unix_get_fd(ch), buf + len, sizeof(buf) - 1 - len);
    if (n <= 0) {
        if (g_backend) { pclose(g_backend); g_backend = NULL; }
        g_running = FALSE;
        len = 0;
        return G_SOURCE_REMOVE;
    }
    len += n; buf[len] = 0;
    char* p = buf;
    for (char* nl; (nl = strchr(p, '\n')); p = nl + 1) { *nl = 0; backend_line(p); }
    len = strlen(p); memmove(buf, p, len + 1);
    if (len >= (int)sizeof(buf) - 1) len = 0;      /* an over-long line: drop it */
    return G_SOURCE_CONTINUE;
}
static void start_install(void) {
    FILE* c = fopen("/tmp/xxri-install.cfg", "w");
    if (!c) return;
    fprintf(c, "MODE=%s\n", g_manual ? "manual" : "auto");
    if (g_manual) fprintf(c, "PART=%s\n", g_sel);
    else          fprintf(c, "DISK=%s\n", g_sel);
    fprintf(c, "USERNAME=%s\n", gtk_entry_get_text(GTK_ENTRY(en_user)));
    fprintf(c, "PASSWORD=%s\n", gtk_entry_get_text(GTK_ENTRY(en_pw)));
    fprintf(c, "HOSTNAME=%s\n", gtk_entry_get_text(GTK_ENTRY(en_host)));
    fprintf(c, "AUTOLOGIN=%d\n", gtk_switch_get_active(GTK_SWITCH(sw_auto)) ? 1 : 0);
    fprintf(c, "TZSTR=%s\n", REGIONS[drop_get(dr_region)].tz);
    fprintf(c, "LANGUAGE=%s\n", LANGS[drop_get(dr_lang)].code);
    fprintf(c, "KEYMAP=us\n");
    fclose(c);

    gtk_revealer_set_reveal_child(GTK_REVEALER(rv_err), FALSE);
    g_pct_target = g_pct_shown = 0; g_step = 0; g_phase = 0;
    gtk_label_set_text(GTK_LABEL(lb_op), "Preparing...");
    steps_refresh();
    show_page("prog");

    char cmd[512];
    snprintf(cmd, sizeof cmd, "%s run /tmp/xxri-install.cfg 2>/tmp/xxri-install.err", backend());
    g_backend = popen(cmd, "r");
    if (g_backend) {
        GIOChannel* ch = g_io_channel_unix_new(fileno(g_backend));
        g_io_add_watch(ch, G_IO_IN | G_IO_HUP | G_IO_ERR, backend_io, NULL);
        g_io_channel_unref(ch);
        g_running = TRUE;
        anim_start();
    }
}

/* first field or primary button of each page */
static void focus_page(const char* name) {
    GtkWidget* f = !strcmp(name, "welcome") ? bt_welcome_next
                 : !strcmp(name, "account") ? en_user
                 : !strcmp(name, "dest")    ? bt_install
                 : !strcmp(name, "finish")  ? bt_restart : NULL;
    if (f && gtk_widget_get_sensitive(f)) gtk_widget_grab_focus(f);
}

/* Enter = the page's primary button, unless a button has the focus */
static gboolean on_key(GtkWidget* w, GdkEventKey* e, gpointer u) {
    (void)u;
    if (e->keyval != GDK_KEY_Return && e->keyval != GDK_KEY_KP_Enter) return FALSE;
    GtkWidget* f = gtk_window_get_focus(GTK_WINDOW(w));
    if (f && GTK_IS_BUTTON(f)) return FALSE;
    const char* p = page();
    if      (!strcmp(p, "welcome")) show_page("account");
    else if (!strcmp(p, "account")) cb_account_next(NULL, NULL);
    else if (!strcmp(p, "dest"))    cb_dest_next(NULL, NULL);
    else if (!strcmp(p, "finish"))  cb_restart(NULL, NULL);
    else return FALSE;
    return TRUE;
}

/* -------------------------------------------------------------------- main */
static void theme_changed(gboolean dark, gpointer u) {
    (void)dark; (void)u;
    GList* tops = gtk_window_list_toplevels();
    for (GList* l = tops; l; l = l->next) gtk_widget_queue_draw(GTK_WIDGET(l->data));
    g_list_free(tops);
}
int main(int argc, char** argv) {
    if (geteuid() != 0)
        execlp("sudo", "sudo", "-E", "/usr/local/bin/xxri-installer", (char*)0);
    gtk_init(&argc, &argv);

    GdkScreen* scr = gdk_screen_get_default();
    GtkCssProvider* prov = gtk_css_provider_new();
    gtk_css_provider_load_from_data(prov, CSS, -1, NULL);
    gtk_style_context_add_provider_for_screen(scr, GTK_STYLE_PROVIDER(prov),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 1);
    /* Light or Dark as the live session's Settings says (sudo -E keeps the
       user's HOME, where the mode is) */
    xxri_theme_init("/usr/local/share/xxri-installer/xxri-installer-dark.css", theme_changed, NULL);
    xxri_dialogs_init();                         /* real corners on the menus */

    int W = gdk_screen_get_width(scr), H = gdk_screen_get_height(scr);
    if (W < 640) W = 1024;
    if (H < 480) H = 768;

    g_disks = g_array_new(FALSE, TRUE, sizeof(Disk));
    g_parts = g_array_new(FALSE, TRUE, sizeof(Disk));
    g_diskicon = cairo_image_surface_create_from_png(DISK_ICON);
    if (cairo_surface_status(g_diskicon) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(g_diskicon); g_diskicon = NULL;
    }

    win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "XXRI OS Lite Installer");
    gtk_window_set_decorated(GTK_WINDOW(win), FALSE);
    gtk_window_set_default_size(GTK_WINDOW(win), W, H);
    gtk_window_move(GTK_WINDOW(win), 0, 0);
    gtk_widget_set_app_paintable(win, TRUE);
    css(win, "xi-window");
    GdkVisual* rgba = gdk_screen_get_rgba_visual(scr);
    g_composited = rgba && gdk_screen_is_composited(scr);
    g_wall = root_wallpaper(W, H);
    if (g_composited) {
        gtk_widget_set_visual(win, rgba);
        /* the wallpaper, as a window of its own under the glass */
        GtkWidget* back = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_title(GTK_WINDOW(back), "XXRI OS Lite Installer");
        gtk_window_set_decorated(GTK_WINDOW(back), FALSE);
        gtk_window_set_default_size(GTK_WINDOW(back), W, H);
        gtk_window_move(GTK_WINDOW(back), 0, 0);
        gtk_window_set_accept_focus(GTK_WINDOW(back), FALSE);
        gtk_widget_set_app_paintable(back, TRUE);
        g_signal_connect(back, "draw", G_CALLBACK(back_draw), NULL);
        g_signal_connect(back, "delete-event", G_CALLBACK(gtk_true), NULL);
        gtk_widget_show(back);
        gtk_window_fullscreen(GTK_WINDOW(back));
        gtk_window_set_transient_for(GTK_WINDOW(win), GTK_WINDOW(back));
    }
    g_signal_connect(win, "draw", G_CALLBACK(win_draw), NULL);
    g_signal_connect(win, "key-press-event", G_CALLBACK(on_key), NULL);
    g_signal_connect(win, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(stack), GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    gtk_stack_set_transition_duration(GTK_STACK(stack), 200);
    gtk_stack_add_named(GTK_STACK(stack), build_welcome(), "welcome");
    gtk_stack_add_named(GTK_STACK(stack), build_account(), "account");
    gtk_stack_add_named(GTK_STACK(stack), build_dest(),    "dest");
    gtk_stack_add_named(GTK_STACK(stack), build_prog(),    "prog");
    gtk_stack_add_named(GTK_STACK(stack), build_finish(),  "finish");
    gtk_container_add(GTK_CONTAINER(win), stack);

    load_devices();
    if (g_disks->len) g_strlcpy(g_sel, g_array_index(g_disks, Disk, 0).dev, sizeof g_sel);
    rebuild_disklist();

    gtk_widget_show_all(win);
    show_page("welcome");
    gtk_window_fullscreen(GTK_WINDOW(win));
    session_exclusive();
    gtk_main();
    return 0;
}
