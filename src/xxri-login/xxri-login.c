/* xxri-login - the login screen. ~/.xsession runs it before anything else of
 * the session when automatic login is off (see root's .profile): exit 0 once
 * the password is right, anything else ends the X session.
 *
 * Checks the password itself: /etc/shadow is root:staff 0660 on Tiny Core
 * and the desktop user is in staff. */
#include <gtk/gtk.h>
#define GETTEXT_PACKAGE "xxri-login"
#include "xxri-i18n.h"
#include <gdk/gdkx.h>
#include <cairo-xlib.h>
#include <X11/Xatom.h>
#include <math.h>
#include <pwd.h>
#include <shadow.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define CSS_PATH "/usr/local/share/xxri-login/xxri-login.css"
#define LOGO     "/usr/local/share/xxri-settings/icons/logo-36.png"
#define AVATAR   120
#define FAIL_MS  2000    /* the field stays locked this long after a wrong password */

static GtkWidget *g_win, *g_entry, *g_row, *g_hint, *g_clock;
static cairo_surface_t* g_wall;
static char* g_name;
static gboolean g_locked, g_error;
static int g_shake;

static void css_class(GtkWidget* w, const char* c) {
    gtk_style_context_add_class(gtk_widget_get_style_context(w), c);
}
static GtkWidget* lbl(const char* t, const char* cls) {
    GtkWidget* l = gtk_label_new(t);
    css_class(l, cls);
    return l;
}

/* ---- the account */
/* 1 right, 0 wrong, -1 can't tell (shadow unreadable) */
static int password_ok(const char* pw) {
    struct passwd* p = getpwuid(getuid());
    struct spwd* s = p ? getspnam(p->pw_name) : NULL;
    if (!s) return -1;
    const char* h = s->sp_pwdp;
    if (!*h) return !*pw;                       /* no password set */
    if (*h == '!' || *h == '*') return 0;       /* locked */
    const char* c = crypt(pw, h);
    if (!c || strlen(c) != strlen(h)) return 0;
    unsigned char d = 0;
    for (size_t i = 0; h[i]; i++) d |= h[i] ^ c[i];
    return d == 0;
}
/* the name from the GECOS field, unless it is adduser's "Linux User" */
static char* display_name(void) {
    struct passwd* p = getpwuid(getuid());
    if (!p) return g_strdup("");
    char* full = g_strdup(p->pw_gecos ? p->pw_gecos : "");
    char* comma = strchr(full, ',');
    if (comma) *comma = 0;
    g_strstrip(full);
    if (*full && strcmp(full, "Linux User")) return full;
    g_free(full);
    char* n = g_strdup(p->pw_name);
    n[0] = g_ascii_toupper(n[0]);
    return n;
}

/* ---- the backdrop: the wallpaper .xsession already put on the root window */
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
static gboolean win_draw(GtkWidget* w, cairo_t* cr, gpointer d) {
    (void)d;
    int h = gtk_widget_get_allocated_height(w);
    if (g_wall) cairo_set_source_surface(cr, g_wall, 0, 0);
    else        cairo_set_source_rgb(cr, 0.20, 0.13, 0.33);
    cairo_paint(cr);
    cairo_pattern_t* g = cairo_pattern_create_linear(0, 0, 0, h);
    cairo_pattern_add_color_stop_rgba(g, 0, 0, 0, 0, 0.10);
    cairo_pattern_add_color_stop_rgba(g, 1, 0, 0, 0, 0.40);
    cairo_set_source(cr, g);
    cairo_paint(cr);
    cairo_pattern_destroy(g);
    return FALSE;
}

/* ---- glyphs, white on a translucent disc */
static gboolean glyph_draw(GtkWidget* w, cairo_t* cr, gpointer data) {
    const char* kind = data;
    int W = gtk_widget_get_allocated_width(w), H = gtk_widget_get_allocated_height(w);
    double s = MIN(W, H), cx = W / 2.0, cy = H / 2.0;
    /* the button's hover state reaches its children */
    gboolean hot = (gtk_widget_get_state_flags(w) & GTK_STATE_FLAG_PRELIGHT) != 0;
    cairo_set_source_rgba(cr, 1, 1, 1, hot ? 0.32 : 0.18);
    cairo_arc(cr, cx, cy, s / 2, 0, 2 * G_PI);
    cairo_fill(cr);
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_set_line_width(cr, s * 0.055);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    if (!strcmp(kind, "shutdown")) {
        cairo_arc(cr, cx, cy + s * 0.02, s * 0.18, -1.05, G_PI + 1.05); cairo_stroke(cr);
        cairo_move_to(cr, cx, cy - s * 0.22); cairo_line_to(cr, cx, cy - s * 0.02); cairo_stroke(cr);
    } else if (!strcmp(kind, "restart")) {
        double R = s * 0.18, a0 = -0.30 * G_PI;
        cairo_arc(cr, cx, cy, R, a0, 1.15 * G_PI); cairo_stroke(cr);
        double ax = cx + cos(a0) * R, ay = cy + sin(a0) * R, t = a0 - G_PI / 2, hw = s * 0.08;
        cairo_move_to(cr, ax + cos(t) * hw * 1.5, ay + sin(t) * hw * 1.5);
        cairo_line_to(cr, ax + cos(t + 2.4) * hw, ay + sin(t + 2.4) * hw);
        cairo_line_to(cr, ax + cos(t - 2.4) * hw, ay + sin(t - 2.4) * hw);
        cairo_close_path(cr); cairo_fill(cr);
    } else if (!strcmp(kind, "suspend")) {
        cairo_arc(cr, cx, cy, s * 0.19, 0, 2 * G_PI);
        cairo_arc_negative(cr, cx + s * 0.10, cy - s * 0.07, s * 0.15, 2 * G_PI, 0);
        cairo_fill(cr);
    } else {
        cairo_move_to(cr, cx - s * 0.20, cy); cairo_line_to(cr, cx + s * 0.18, cy);
        cairo_move_to(cr, cx + s * 0.02, cy - s * 0.17); cairo_line_to(cr, cx + s * 0.19, cy);
        cairo_line_to(cr, cx + s * 0.02, cy + s * 0.17); cairo_stroke(cr);
    }
    return TRUE;
}
static GtkWidget* glyph(const char* kind, int size) {
    GtkWidget* d = gtk_drawing_area_new();
    gtk_widget_set_size_request(d, size, size);
    g_signal_connect(d, "draw", G_CALLBACK(glyph_draw), (gpointer)kind);
    return d;
}

/* ~/.face if there is one, else the initial on the brand gradient */
static gboolean avatar_draw(GtkWidget* w, cairo_t* cr, gpointer d) {
    (void)d;
    double s = gtk_widget_get_allocated_width(w), r = s / 2;
    cairo_arc(cr, r, r, r, 0, 2 * G_PI);
    cairo_clip(cr);
    char* face = g_build_filename(g_get_home_dir(), ".face", NULL);
    GdkPixbuf* pb = gdk_pixbuf_new_from_file_at_scale(face, (int)s, (int)s, FALSE, NULL);
    g_free(face);
    if (pb) {
        gdk_cairo_set_source_pixbuf(cr, pb, 0, 0);
        cairo_paint(cr);
        g_object_unref(pb);
    } else {
        cairo_pattern_t* g = cairo_pattern_create_linear(0, 0, s, s);
        cairo_pattern_add_color_stop_rgb(g, 0, 0.51, 0.24, 0.93);
        cairo_pattern_add_color_stop_rgb(g, 1, 0.93, 0.16, 0.78);
        cairo_set_source(cr, g);
        cairo_paint(cr);
        cairo_pattern_destroy(g);
        char initial[8] = "";
        if (*g_name) g_unichar_to_utf8(g_unichar_toupper(g_utf8_get_char(g_name)), initial);
        PangoLayout* l = gtk_widget_create_pango_layout(w, initial);
        PangoFontDescription* f = pango_font_description_from_string("Avenir Next Demi-Bold 44");
        pango_layout_set_font_description(l, f);
        int tw, th;
        pango_layout_get_pixel_size(l, &tw, &th);
        cairo_set_source_rgb(cr, 1, 1, 1);
        cairo_move_to(cr, r - tw / 2.0, r - th / 2.0);
        pango_cairo_show_layout(cr, l);
        pango_font_description_free(f);
        g_object_unref(l);
    }
    cairo_reset_clip(cr);
    cairo_new_path(cr);
    cairo_set_source_rgba(cr, 1, 1, 1, 0.45);
    cairo_set_line_width(cr, 2);
    cairo_arc(cr, r, r, r - 1, 0, 2 * G_PI);
    cairo_stroke(cr);
    return TRUE;
}

/* PNG through cairo, as the launcher does: gdk-pixbuf's loader cache is
 * written by xxri-hw-init and may not exist yet */
static gboolean logo_draw(GtkWidget* w, cairo_t* cr, gpointer d) {
    (void)d;
    cairo_surface_t* s = cairo_image_surface_create_from_png(LOGO);
    if (cairo_surface_status(s) == CAIRO_STATUS_SUCCESS) {
        cairo_set_source_surface(cr, s, 0, (gtk_widget_get_allocated_height(w) - 36) / 2.0);
        cairo_paint(cr);
    }
    cairo_surface_destroy(s);
    return TRUE;
}

/* ---- status, top right: keyboard layout, battery, Wi-Fi, clock */
static char* read_line(const char* path) {
    char* s = NULL;
    if (!g_file_get_contents(path, &s, NULL, NULL)) return NULL;
    return g_strstrip(s);
}
static int battery_percent(void) {
    GDir* d = g_dir_open("/sys/class/power_supply", 0, NULL);
    int pc = -1;
    for (const char* e; d && pc < 0 && (e = g_dir_read_name(d)); ) {
        char* p = g_strdup_printf("/sys/class/power_supply/%s/type", e);
        char* t = read_line(p); g_free(p);
        if (t && !strcmp(t, "Battery")) {
            p = g_strdup_printf("/sys/class/power_supply/%s/capacity", e);
            char* c = read_line(p); g_free(p);
            if (c) pc = CLAMP(atoi(c), 0, 100);
            g_free(c);
        }
        g_free(t);
    }
    if (d) g_dir_close(d);
    return pc;
}
static gboolean wifi_up(void) {
    GDir* d = g_dir_open("/sys/class/net", 0, NULL);
    gboolean up = FALSE;
    for (const char* e; d && !up && (e = g_dir_read_name(d)); ) {
        char* w = g_strdup_printf("/sys/class/net/%s/wireless", e);
        char* o = g_strdup_printf("/sys/class/net/%s/operstate", e);
        char* s = g_file_test(w, G_FILE_TEST_IS_DIR) ? read_line(o) : NULL;
        up = s && !strcmp(s, "up");
        g_free(s); g_free(w); g_free(o);
    }
    if (d) g_dir_close(d);
    return up;
}
static char* xkb_layout(void) {                 /* "XKB=pt:nodeadkeys" -> "PT" */
    char* s = NULL;
    char* l = NULL;
    if (g_file_get_contents("/etc/sysconfig/xkb", &s, NULL, NULL) && g_str_has_prefix(s, "XKB="))
        l = g_strndup(s + 4, strcspn(s + 4, ":,\n"));
    g_free(s);
    if (!l || !*l) { g_free(l); l = g_strdup("us"); }
    char* u = g_ascii_strup(l, -1);
    g_free(l);
    return u;
}
static gboolean status_draw(GtkWidget* w, cairo_t* cr, gpointer data) {
    int pc = GPOINTER_TO_INT(data);
    double H = gtk_widget_get_allocated_height(w), cy = H / 2.0;
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_set_line_width(cr, 1.5);
    if (pc >= 0) {
        cairo_rectangle(cr, 1.5, cy - 5, 20, 10); cairo_stroke(cr);
        cairo_rectangle(cr, 22, cy - 2.5, 2, 5); cairo_fill(cr);
        cairo_rectangle(cr, 3.5, cy - 3, 16 * pc / 100.0, 6); cairo_fill(cr);
    } else {
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        for (int i = 0; i < 3; i++) {
            cairo_arc(cr, 10, cy + 5, 3.5 + i * 3.5, -2.4, -0.74);
            cairo_stroke(cr);
        }
    }
    return TRUE;
}
static gboolean clock_tick(gpointer d) {
    (void)d;
    char day[32], t[32], all[64]; time_t now = time(NULL);
    struct tm* tm = localtime(&now);
    strftime(day, sizeof day, "%a", tm);
    strftime(t, sizeof t, "%l:%M %p", tm);      /* 12h, like the Control Center */
    g_snprintf(all, sizeof all, "%s %s", day, g_strstrip(t));
    gtk_label_set_text(GTK_LABEL(g_clock), all);
    return G_SOURCE_CONTINUE;
}
static GtkWidget* build_status(void) {
    GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 14);
    css_class(row, "xl-status");
    char* kb = xkb_layout();
    gtk_box_pack_start(GTK_BOX(row), lbl(kb, "xl-status-text"), FALSE, FALSE, 0);
    g_free(kb);
    int pc = battery_percent();
    if (pc >= 0) {
        GtkWidget* b = gtk_drawing_area_new();
        gtk_widget_set_size_request(b, 25, 16);
        g_signal_connect(b, "draw", G_CALLBACK(status_draw), GINT_TO_POINTER(pc));
        gtk_box_pack_start(GTK_BOX(row), b, FALSE, FALSE, 0);
    }
    if (wifi_up()) {
        GtkWidget* f = gtk_drawing_area_new();
        gtk_widget_set_size_request(f, 20, 16);
        g_signal_connect(f, "draw", G_CALLBACK(status_draw), GINT_TO_POINTER(-1));
        gtk_box_pack_start(GTK_BOX(row), f, FALSE, FALSE, 0);
    }
    g_clock = lbl("", "xl-status-text");
    gtk_box_pack_start(GTK_BOX(row), g_clock, FALSE, FALSE, 0);
    clock_tick(NULL);
    g_timeout_add_seconds(1, clock_tick, NULL);
    return row;
}

/* ---- signing in */
static void hint(const char* text, gboolean error) {
    gtk_label_set_text(GTK_LABEL(g_hint), text);
    g_error = error;
}
static void caps_hint(void) {
    GdkKeymap* km = gdk_keymap_get_for_display(gdk_display_get_default());
    hint(gdk_keymap_get_caps_lock_state(km) ? _("Caps Lock is on") : "", FALSE);
}
static void on_keymap(GdkKeymap* km, gpointer d) { (void)km; (void)d; if (!g_error) caps_hint(); }

static gboolean shake_tick(gpointer d) {
    (void)d;
    double t = ++g_shake / 18.0;
    int off = (int)(sin(t * 6 * G_PI) * 10 * (1 - t));
    gtk_widget_set_margin_start(g_row, 12 + off);
    gtk_widget_set_margin_end(g_row, 12 - off);
    if (g_shake < 18) return G_SOURCE_CONTINUE;
    g_shake = 0;
    return G_SOURCE_REMOVE;
}
static gboolean unlock(gpointer d) {
    (void)d;
    g_locked = FALSE;
    gtk_widget_set_sensitive(g_entry, TRUE);
    gtk_widget_grab_focus(g_entry);
    return G_SOURCE_REMOVE;
}
static void submit(void) {
    if (g_locked) return;
    int ok = password_ok(gtk_entry_get_text(GTK_ENTRY(g_entry)));
    gtk_entry_set_text(GTK_ENTRY(g_entry), "");
    if (ok == 1) { gtk_main_quit(); return; }
    hint(_("Incorrect password"), TRUE);
    g_locked = TRUE;
    gtk_widget_set_sensitive(g_entry, FALSE);
    if (!g_shake) g_timeout_add(16, shake_tick, NULL);
    g_timeout_add(FAIL_MS, unlock, NULL);
}
static void on_activate(GtkEntry* e, gpointer d) { (void)e; (void)d; submit(); }
static void on_arrow(GtkButton* b, gpointer d) { (void)b; (void)d; submit(); }
static void on_changed(GtkEditable* e, gpointer d) { (void)e; (void)d; if (g_error) caps_hint(); }
static gboolean on_key(GtkWidget* w, GdkEventKey* e, gpointer d) {
    (void)w; (void)d;
    if (e->keyval != GDK_KEY_Escape) return FALSE;
    gtk_entry_set_text(GTK_ENTRY(g_entry), "");
    caps_hint();
    return TRUE;
}

static void on_power(GtkButton* b, gpointer d) {
    (void)b;
    char* cmd = g_strdup_printf("xxri-power %s", (const char*)d);
    g_spawn_command_line_async(cmd, NULL);
    g_free(cmd);
}
static GtkWidget* power_button(const char* kind, const char* title) {
    GtkWidget* b = gtk_button_new();
    gtk_button_set_relief(GTK_BUTTON(b), GTK_RELIEF_NONE);
    css_class(b, "xl-action");
    GtkWidget* v = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget* g = glyph(kind, 40);
    gtk_widget_set_halign(g, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(v), g, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(v), lbl(title, "xl-action-title"), FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(b), v);
    g_signal_connect(b, "clicked", G_CALLBACK(on_power), (gpointer)kind);
    return b;
}
static gboolean can_suspend(void) {
    char* s = read_line("/sys/power/state");
    gboolean ok = s && strstr(s, "mem");
    g_free(s);
    return ok;
}

/* No window manager yet, so nothing gives the window the keyboard: take it,
 * and the pointer, until the session starts. */
static gboolean grab(gpointer d) {
    (void)d;
    static int tries;
    GdkSeat* seat = gdk_display_get_default_seat(gdk_display_get_default());
    GdkWindow* gw = gtk_widget_get_window(g_win);
    if (gdk_seat_grab(seat, gw, GDK_SEAT_CAPABILITY_ALL, TRUE, NULL, NULL, NULL, NULL)
            != GDK_GRAB_SUCCESS && ++tries < 50)
        return G_SOURCE_CONTINUE;
    gdk_window_focus(gw, GDK_CURRENT_TIME);
    gtk_widget_grab_focus(g_entry);
    return G_SOURCE_REMOVE;
}

int main(int argc, char** argv) {
    xxri_i18n_init();
    if (!gtk_init_check(&argc, &argv)) return 1;
    GtkCssProvider* css = gtk_css_provider_new();
    gtk_css_provider_load_from_path(css, CSS_PATH, NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css);

    /* can't check passwords here: end X, tty1 asks at the text prompt */
    if (password_ok("") < 0) return 1;

    /* the primary display, which xxri-display keeps at 0,0: with displays
       extended the screen spans them all and the card sat in the gap */
    GdkScreen* scr = gdk_screen_get_default();
    int W = gdk_screen_get_width(scr), H = gdk_screen_get_height(scr);
    GdkMonitor* pm = gdk_display_get_primary_monitor(gdk_display_get_default());
    if (pm) {
        GdkRectangle pa; gdk_monitor_get_geometry(pm, &pa);
        if (pa.x == 0 && pa.y == 0 && pa.width > 0 && pa.height > 0) { W = pa.width; H = pa.height; }
    }
    g_wall = root_wallpaper(W, H);
    g_name = display_name();

    /* override-redirect: nothing manages windows yet */
    g_win = gtk_window_new(GTK_WINDOW_POPUP);
    gtk_widget_set_size_request(g_win, W, H);
    gtk_window_move(GTK_WINDOW(g_win), 0, 0);
    gtk_widget_set_app_paintable(g_win, TRUE);
    css_class(g_win, "xl-window");
    g_signal_connect(g_win, "draw", G_CALLBACK(win_draw), NULL);
    g_signal_connect(g_win, "key-press-event", G_CALLBACK(on_key), NULL);

    GtkWidget* page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(g_win), page);

    GtkWidget* top = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    css_class(top, "xl-top");
    GtkWidget* logo = gtk_drawing_area_new();
    gtk_widget_set_size_request(logo, 36, 36);
    g_signal_connect(logo, "draw", G_CALLBACK(logo_draw), NULL);
    gtk_box_pack_start(GTK_BOX(top), logo, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(top), lbl("XXRI OS Lite", "xl-brand"), FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(top), build_status(), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(page), top, FALSE, FALSE, 0);

    GtkWidget* mid = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_halign(mid, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(mid, GTK_ALIGN_CENTER);
    GtkWidget* av = gtk_drawing_area_new();
    gtk_widget_set_size_request(av, AVATAR, AVATAR);
    gtk_widget_set_halign(av, GTK_ALIGN_CENTER);
    g_signal_connect(av, "draw", G_CALLBACK(avatar_draw), NULL);
    gtk_box_pack_start(GTK_BOX(mid), av, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(mid), lbl(g_name, "xl-name"), FALSE, FALSE, 0);

    g_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(g_row, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_start(g_row, 12);
    gtk_widget_set_margin_end(g_row, 12);
    g_entry = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(g_entry), FALSE);
    gtk_entry_set_input_purpose(GTK_ENTRY(g_entry), GTK_INPUT_PURPOSE_PASSWORD);
    gtk_entry_set_placeholder_text(GTK_ENTRY(g_entry), _("Enter Password"));
    gtk_entry_set_width_chars(GTK_ENTRY(g_entry), 18);
    /* its warning icon needs an icon theme, which the device doesn't have */
    g_object_set(g_entry, "caps-lock-warning", FALSE, NULL);
    css_class(g_entry, "xl-password");
    g_signal_connect(g_entry, "activate", G_CALLBACK(on_activate), NULL);
    g_signal_connect(g_entry, "changed", G_CALLBACK(on_changed), NULL);
    gtk_box_pack_start(GTK_BOX(g_row), g_entry, FALSE, FALSE, 0);
    GtkWidget* go = gtk_button_new();
    gtk_button_set_relief(GTK_BUTTON(go), GTK_RELIEF_NONE);
    css_class(go, "xl-go");
    gtk_container_add(GTK_CONTAINER(go), glyph("go", 26));
    g_signal_connect(go, "clicked", G_CALLBACK(on_arrow), NULL);
    gtk_box_pack_start(GTK_BOX(g_row), go, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(mid), g_row, FALSE, FALSE, 0);
    g_hint = lbl("", "xl-hint");
    gtk_box_pack_start(GTK_BOX(mid), g_hint, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(page), mid, TRUE, TRUE, 0);

    GtkWidget* bot = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 36);
    gtk_widget_set_halign(bot, GTK_ALIGN_CENTER);
    css_class(bot, "xl-actions");
    gtk_box_pack_start(GTK_BOX(bot), power_button("shutdown", _("Shut Down")), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(bot), power_button("restart", _("Restart")), FALSE, FALSE, 0);
    if (can_suspend())
        gtk_box_pack_start(GTK_BOX(bot), power_button("suspend", _("Sleep")), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(page), bot, FALSE, FALSE, 0);

    g_signal_connect(gdk_keymap_get_for_display(gdk_display_get_default()), "state-changed",
                     G_CALLBACK(on_keymap), NULL);
    gtk_widget_show_all(g_win);
    caps_hint();
    if (grab(NULL) == G_SOURCE_CONTINUE) g_timeout_add(100, grab, NULL);
    gtk_main();
    return 0;
}
