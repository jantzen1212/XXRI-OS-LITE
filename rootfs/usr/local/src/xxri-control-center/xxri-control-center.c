/* xxri-control-center - the XXRI quick-settings panel (Phase 10).
 *
 * Lives in the bottom-right corner of the desktop, exactly where the mockups
 * put it (assets/mockup/mockup1.jpg, mockup3.jpg).  It is a pure front-end:
 * every reading and every action goes through the Phase 7 xxri-* backends, so
 * the panel can never disagree with Settings about what the hardware is doing.
 *
 * Two states in ONE override-redirect window:
 *   collapsed  a small status chip (network + battery + clock)
 *   expanded   the full panel: Wi-Fi and Bluetooth tiles, volume, brightness,
 *              and a bottom row with power, settings, battery and the clock
 * Clicking the chip expands; clicking it again, choosing an action, or moving
 * the pointer away collapses.  There is no pointer grab and no keyboard focus
 * to steal, which is what keeps it out of the way of the dock, Settings and
 * the Store.
 */
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <cairo.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>

#define CSS_PATH  "/usr/local/share/xxri-control-center/xxri-cc.css"
#define ICONDIR   "/usr/local/share/xxri-settings/icons"
#define EDGE_X    18      /* gap to the right screen edge, from the mockup */
#define EDGE_Y     20     /* gap to the bottom screen edge                 */
#define PANEL_W   264
#define CHIP_H     38

/* ----------------------------------------------------------- backends --- */
static char* run_cmd(const char* fmt, ...) {
    char cmd[1024]; va_list ap; va_start(ap, fmt);
    vsnprintf(cmd, sizeof cmd, fmt, ap); va_end(ap);
    char full[1100]; snprintf(full, sizeof full, "%s 2>/dev/null", cmd);
    FILE* f = popen(full, "r");
    if (!f) return g_strdup("");
    GString* s = g_string_new("");
    char buf[512]; size_t n;
    while ((n = fread(buf,1,sizeof buf,f)) > 0) g_string_append_len(s, buf, n);
    pclose(f);
    while (s->len && (s->str[s->len-1]=='\n' || s->str[s->len-1]=='\r'))
        g_string_truncate(s, s->len-1);
    return g_string_free(s, FALSE);
}
static void run_bg(const char* fmt, ...) {
    char cmd[1024]; va_list ap; va_start(ap, fmt);
    vsnprintf(cmd, sizeof cmd, fmt, ap); va_end(ap);
    char full[1100]; snprintf(full, sizeof full, "%s >/dev/null 2>&1 &", cmd);
    int r = system(full); (void)r;
}
/* the backends emit the same flat-ish JSON the Settings app reads */
static char* jget(const char* doc, const char* key) {
    char pat[128]; snprintf(pat, sizeof pat, "\"%s\":", key);
    /* The backends wrap their payload in an envelope whose key repeats inside
       it - xxri-audio emits {"volume":{"volume":65,...}}.  A plain strstr
       stopped at the envelope and parsed "{" as the number, so the Control
       Center's slider always read 0.  Skip values that open an object or an
       array and keep looking for the scalar. */
    const char* p = doc;
    for (;;) {
        p = strstr(p, pat);
        if (!p) return g_strdup("");
        const char* v = p + strlen(pat);
        while (*v == ' ') v++;
        if (*v != '{' && *v != '[') break;
        p = v;
    }
    p += strlen(pat);
    while (*p == ' ') p++;
    if (*p == '"') {
        p++; const char* e = p;
        while (*e && !(*e=='"' && e[-1] != '\\')) e++;
        return g_strndup(p, e-p);
    }
    const char* e = p;
    while (*e && *e!=',' && *e!='}' && *e!=']') e++;
    return g_strndup(p, e-p);
}
static gboolean jbool(const char* doc, const char* key) {
    char* v = jget(doc,key); gboolean b = !strcmp(v,"true"); g_free(v); return b;
}
static int jnum(const char* doc, const char* key) {
    char* v = jget(doc,key); int n = atoi(v); g_free(v); return n;
}
/* first object of "key":[{...}] */
static char* jfirst(const char* doc, const char* key) {
    char pat[128]; snprintf(pat, sizeof pat, "\"%s\":[", key);
    const char* p = strstr(doc, pat);
    if (!p) return g_strdup("");
    p += strlen(pat);
    if (*p != '{') return g_strdup("");
    const char* e = strchr(p, '}');
    if (!e) return g_strdup("");
    return g_strndup(p, e-p+1);
}

/* -------------------------------------------------------------- state --- */
typedef struct {
    gboolean net_up;  char* net_name;  char* net_kind;   /* wifi / ethernet */
    gboolean bt_ok;   char* bt_reason;
    gboolean audio_ok; int volume; gboolean muted;
    gboolean bright_ok; int bright;
    gboolean batt_ok; int batt; gboolean charging;
} State;
static State S;
static GtkWidget *g_win, *g_chip, *g_panel, *g_root;
static gboolean g_expanded = FALSE;
static guint g_collapse_timer = 0;
static GtkWidget *g_vol_scale, *g_bright_scale;
static gboolean g_setting = FALSE;      /* suppress feedback loops */

static void state_free(void) {
    g_free(S.net_name); g_free(S.net_kind); g_free(S.bt_reason);
    memset(&S, 0, sizeof S);
}
static void state_read(void) {
    state_free();
    /* network: prefer a connected Wi-Fi interface, else any interface that is up */
    char* net = run_cmd("xxri-network status --json");
    const char* p = net; const char* best = NULL; gboolean best_wifi = FALSE;
    while ((p = strstr(p, "\"interface\":")) != NULL) {
        const char* obj = p;
        char* state = jget(obj, "state");
        char* type  = jget(obj, "type");
        gboolean up = !strcmp(state, "up");
        gboolean wifi = !strcmp(type, "wifi");
        if (up && (!best || (wifi && !best_wifi))) { best = obj; best_wifi = wifi; }
        g_free(state); g_free(type);
        p += 12;
    }
    if (best) {
        S.net_up = TRUE;
        char* ssid = jget(best, "ssid");
        char* iface = jget(best, "interface");
        S.net_kind = g_strdup(best_wifi ? "Wi-Fi" : "Ethernet");
        S.net_name = (*ssid) ? g_strdup(ssid) : g_strdup(iface);
        g_free(ssid); g_free(iface);
    } else {
        S.net_up = FALSE;
        S.net_kind = g_strdup("Network");
        S.net_name = g_strdup("Not connected");
    }
    g_free(net);

    char* bt = run_cmd("xxri-bluetooth status --json");
    S.bt_ok = jbool(bt, "available");
    { char* r = jget(bt, "reason"); S.bt_reason = *r ? r : g_strdup("Not available"); }
    g_free(bt);

    char* au = run_cmd("xxri-audio volume --json");
    S.audio_ok = (strstr(au, "\"volume\":") != NULL) && !strstr(au, "\"alsa\":false");
    S.volume = jnum(au, "volume");
    S.muted  = jbool(au, "muted");
    g_free(au);

    char* br = run_cmd("xxri-power brightness --json");
    S.bright_ok = jbool(br, "supported") || (strstr(br, "\"percent\":") != NULL);
    S.bright = jnum(br, "percent");
    if (S.bright_ok && S.bright == 0 && !strstr(br, "\"percent\":0")) S.bright_ok = FALSE;
    g_free(br);

    char* pw = run_cmd("xxri-power info --json");
    S.batt_ok = jbool(pw, "present");
    if (S.batt_ok) {
        char* b0 = jfirst(pw, "batteries");
        S.batt = jnum(b0, "capacity");
        char* st = jget(b0, "status");
        S.charging = (strstr(st, "Charging") != NULL) || jbool(pw, "online");
        g_free(st); g_free(b0);
    }
    g_free(pw);
}

/* --------------------------------------------------------------- draw --- */
/* GTK takes one class name per call - a "a b" string would add a single,
   never-matching class, which is how the tiles lost their background. */
static void css_class(GtkWidget* w, const char* c) {
    gchar** parts = g_strsplit(c, " ", -1);
    for (int i = 0; parts[i]; i++)
        if (*parts[i]) gtk_style_context_add_class(gtk_widget_get_style_context(w), parts[i]);
    g_strfreev(parts);
}
static GtkWidget* lbl(const char* t, const char* cls, gfloat xalign) {
    GtkWidget* l = gtk_label_new(t);
    gtk_label_set_xalign(GTK_LABEL(l), xalign);
    if (cls) css_class(l, cls);
    return l;
}
/* The device has no icon theme, so every glyph is drawn with cairo - the same
 * rule the Store follows.  `kind` picks the pictogram. */
typedef struct { const char* kind; int size; gboolean on; } Glyph;
static gboolean glyph_draw(GtkWidget* w, cairo_t* cr, gpointer data) {
    Glyph* g = data;
    int W = gtk_widget_get_allocated_width(w), H = gtk_widget_get_allocated_height(w);
    double s = g->size, cx = W/2.0, cy = H/2.0;
    /* the XXRI brand tile: rounded square with the pink->purple gradient */
    if (g->on) {
        cairo_pattern_t* pat = cairo_pattern_create_linear(cx-s/2, cy-s/2, cx+s/2, cy+s/2);
        cairo_pattern_add_color_stop_rgb(pat, 0, 0.51, 0.24, 0.93);
        cairo_pattern_add_color_stop_rgb(pat, 1, 0.93, 0.16, 0.78);
        cairo_set_source(cr, pat);
        cairo_pattern_destroy(pat);
    } else {
        cairo_set_source_rgb(cr, 0.80, 0.79, 0.86);
    }
    double r = s*0.32, x = cx-s/2, y = cy-s/2;
    cairo_new_sub_path(cr);
    cairo_arc(cr, x+s-r, y+r,   r, -G_PI/2, 0);
    cairo_arc(cr, x+s-r, y+s-r, r, 0, G_PI/2);
    cairo_arc(cr, x+r,   y+s-r, r, G_PI/2, G_PI);
    cairo_arc(cr, x+r,   y+r,   r, G_PI, 1.5*G_PI);
    cairo_close_path(cr); cairo_fill(cr);

    cairo_set_source_rgb(cr, 1,1,1);
    cairo_set_line_width(cr, s*0.075);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    if (!strcmp(g->kind, "wifi")) {
        for (int i = 0; i < 3; i++) {
            double rr = s*0.16 + i*s*0.13;
            cairo_arc(cr, cx, cy+s*0.20, rr, -2.5, -0.65);
            cairo_stroke(cr);
        }
        cairo_arc(cr, cx, cy+s*0.20, s*0.05, 0, 2*G_PI); cairo_fill(cr);
    } else if (!strcmp(g->kind, "eth")) {
        cairo_rectangle(cr, cx-s*0.26, cy-s*0.22, s*0.52, s*0.30);
        cairo_stroke(cr);
        cairo_move_to(cr, cx, cy+s*0.08); cairo_line_to(cr, cx, cy+s*0.22); cairo_stroke(cr);
        cairo_move_to(cr, cx-s*0.28, cy+s*0.22); cairo_line_to(cr, cx+s*0.28, cy+s*0.22); cairo_stroke(cr);
    } else if (!strcmp(g->kind, "bt")) {
        cairo_move_to(cr, cx-s*0.16, cy-s*0.16);
        cairo_line_to(cr, cx+s*0.16, cy+s*0.16);
        cairo_line_to(cr, cx,        cy+s*0.30);
        cairo_line_to(cr, cx,        cy-s*0.30);
        cairo_line_to(cr, cx+s*0.16, cy-s*0.16);
        cairo_line_to(cr, cx-s*0.16, cy+s*0.16);
        cairo_stroke(cr);
    } else if (!strcmp(g->kind, "vol")) {
        cairo_move_to(cr, cx-s*0.22, cy-s*0.10);
        cairo_line_to(cr, cx-s*0.10, cy-s*0.10);
        cairo_line_to(cr, cx+s*0.04, cy-s*0.24);
        cairo_line_to(cr, cx+s*0.04, cy+s*0.24);
        cairo_line_to(cr, cx-s*0.10, cy+s*0.10);
        cairo_line_to(cr, cx-s*0.22, cy+s*0.10);
        cairo_close_path(cr); cairo_fill(cr);
        if (g->on) {
            cairo_arc(cr, cx+s*0.06, cy, s*0.20, -1.0, 1.0); cairo_stroke(cr);
        } else {                      /* muted: a slash through the speaker */
            cairo_move_to(cr, cx+s*0.10, cy-s*0.16);
            cairo_line_to(cr, cx+s*0.28, cy+s*0.16); cairo_stroke(cr);
            cairo_move_to(cr, cx+s*0.28, cy-s*0.16);
            cairo_line_to(cr, cx+s*0.10, cy+s*0.16); cairo_stroke(cr);
        }
    } else if (!strcmp(g->kind, "sun")) {
        cairo_arc(cr, cx, cy, s*0.14, 0, 2*G_PI); cairo_fill(cr);
        for (int i = 0; i < 8; i++) {
            double a = i*G_PI/4;
            cairo_move_to(cr, cx+cos(a)*s*0.22, cy+sin(a)*s*0.22);
            cairo_line_to(cr, cx+cos(a)*s*0.32, cy+sin(a)*s*0.32);
            cairo_stroke(cr);
        }
    } else if (!strcmp(g->kind, "gear")) {
        /* sliders, not a cog: at 24px a cog reads as the brightness sun */
        for (int i = 0; i < 3; i++) {
            double y = cy + (i-1)*s*0.20;
            cairo_move_to(cr, cx-s*0.28, y); cairo_line_to(cr, cx+s*0.28, y);
            cairo_stroke(cr);
            double kx = cx + (i==1 ? s*0.12 : -s*0.10);
            cairo_arc(cr, kx, y, s*0.075, 0, 2*G_PI); cairo_fill(cr);
        }
    } else if (!strcmp(g->kind, "power")) {
        cairo_arc(cr, cx, cy+s*0.03, s*0.22, -1.05, G_PI+1.05); cairo_stroke(cr);
        cairo_move_to(cr, cx, cy-s*0.28); cairo_line_to(cr, cx, cy-s*0.02); cairo_stroke(cr);
    }
    return TRUE;
}
static GtkWidget* glyph(const char* kind, int box, int size, gboolean on) {
    GtkWidget* d = gtk_drawing_area_new();
    gtk_widget_set_size_request(d, box, box);
    Glyph* g = g_new0(Glyph,1); g->kind = kind; g->size = size; g->on = on;
    g_signal_connect_data(d, "draw", G_CALLBACK(glyph_draw), g, (GClosureNotify)g_free, 0);
    return d;
}
/* battery ring, as in the mockup's bottom row */
static gboolean batt_draw(GtkWidget* w, cairo_t* cr, gpointer data) {
    (void)data;
    int W = gtk_widget_get_allocated_width(w), H = gtk_widget_get_allocated_height(w);
    double cx = W/2.0, cy = H/2.0, r = MIN(W,H)/2.0 - 2;
    cairo_set_source_rgb(cr, 0.13,0.12,0.16);
    cairo_arc(cr, cx, cy, r, 0, 2*G_PI); cairo_fill(cr);
    double frac = CLAMP(S.batt, 0, 100) / 100.0;
    cairo_set_line_width(cr, 2.5);
    cairo_set_source_rgb(cr, 0.25,0.30,0.42);
    cairo_arc(cr, cx, cy, r-3, 0, 2*G_PI); cairo_stroke(cr);
    if (S.charging) cairo_set_source_rgb(cr, 0.16,0.80,0.60);
    else if (S.batt <= 15) cairo_set_source_rgb(cr, 0.95,0.30,0.42);
    else cairo_set_source_rgb(cr, 0.18,0.55,0.96);
    cairo_arc(cr, cx, cy, r-3, -G_PI/2, -G_PI/2 + frac*2*G_PI); cairo_stroke(cr);
    cairo_set_source_rgb(cr, 1,1,1);
    cairo_rectangle(cr, cx-r*0.42, cy-r*0.20, r*0.72, r*0.40); cairo_fill(cr);
    cairo_rectangle(cr, cx+r*0.32, cy-r*0.09, r*0.10, r*0.18); cairo_fill(cr);
    return TRUE;
}

/* --------------------------------------------------------- interaction -- */
static void rebuild(void);
static void collapse(void);

static gboolean on_leave(GtkWidget* w, GdkEventCrossing* e, gpointer d) {
    (void)w; (void)d;
    if (!g_expanded || e->detail == GDK_NOTIFY_INFERIOR) return FALSE;
    if (g_collapse_timer) g_source_remove(g_collapse_timer);
    g_collapse_timer = g_timeout_add(1400, (GSourceFunc)(void*)collapse, NULL);
    return FALSE;
}
static gboolean on_enter(GtkWidget* w, GdkEventCrossing* e, gpointer d) {
    (void)w; (void)e; (void)d;
    if (g_collapse_timer) { g_source_remove(g_collapse_timer); g_collapse_timer = 0; }
    return FALSE;
}
/* Rounded corners without a compositor.
 * The desktop is not composited, so a transparent GTK background paints black.
 * Instead the window is CLIPPED to a rounded rectangle with the X Shape
 * extension: the corners are genuinely not part of the window, so the
 * wallpaper shows through them. */
static void apply_shape(int w, int h, int radius) {
    GdkWindow* gw = gtk_widget_get_window(g_win);
    if (!gw || w <= 0 || h <= 0) return;
    cairo_surface_t* m = cairo_image_surface_create(CAIRO_FORMAT_A8, w, h);
    cairo_t* cr = cairo_create(m);
    cairo_set_source_rgba(cr, 0,0,0,0); cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_paint(cr);
    cairo_set_source_rgba(cr, 0,0,0,1);
    double r = radius;
    cairo_new_sub_path(cr);
    cairo_arc(cr, w-r, r,   r, -G_PI/2, 0);
    cairo_arc(cr, w-r, h-r, r, 0, G_PI/2);
    cairo_arc(cr, r,   h-r, r, G_PI/2, G_PI);
    cairo_arc(cr, r,   r,   r, G_PI, 1.5*G_PI);
    cairo_close_path(cr); cairo_fill(cr);
    cairo_destroy(cr);
    cairo_region_t* reg = gdk_cairo_region_create_from_surface(m);
    cairo_surface_destroy(m);
    gdk_window_shape_combine_region(gw, reg, 0, 0);
    cairo_region_destroy(reg);
}
static void place_window(void) {
    GdkScreen* sc = gtk_widget_get_screen(g_win);
    int sw = gdk_screen_get_width(sc), sh = gdk_screen_get_height(sc);
    GtkRequisition nat;
    gtk_widget_get_preferred_size(g_root, NULL, &nat);
    int w = g_expanded ? PANEL_W : nat.width;
    int h = nat.height;
    gtk_window_resize(GTK_WINDOW(g_win), w, h);
    gtk_window_move(GTK_WINDOW(g_win), sw - EDGE_X - w, sh - EDGE_Y - h);
    /* collapsed the chip is a pill, expanded the panel keeps the 16px radius
       the mockup uses */
    apply_shape(w, h, g_expanded ? 16 : h/2);
    GdkWindow* gw = gtk_widget_get_window(g_win);   /* NULL before realize */
    if (gw) gdk_window_raise(gw);
}
static void expand(void) {
    if (g_expanded) return;
    state_read();
    g_expanded = TRUE;
    rebuild();
}
static void collapse(void) {
    if (g_collapse_timer) { g_source_remove(g_collapse_timer); g_collapse_timer = 0; }
    if (!g_expanded) return;
    g_expanded = FALSE;
    rebuild();
}
static gboolean on_chip_click(GtkWidget* w, GdkEventButton* e, gpointer d) {
    (void)w; (void)e; (void)d;
    if (g_expanded) collapse(); else expand();
    return TRUE;
}
static void act_open(const char* cmd) { run_bg("%s", cmd); collapse(); }
static gboolean on_wifi(GtkWidget* w, GdkEventButton* e, gpointer d) {
    (void)w;(void)e;(void)d; act_open("xxri-settings wifi"); return TRUE; }
static gboolean on_bt(GtkWidget* w, GdkEventButton* e, gpointer d) {
    (void)w;(void)e;(void)d; act_open("xxri-settings bluetooth"); return TRUE; }
static gboolean on_settings(GtkWidget* w, GdkEventButton* e, gpointer d) {
    (void)w;(void)e;(void)d; act_open("xxri-settings"); return TRUE; }
static gboolean on_power(GtkWidget* w, GdkEventButton* e, gpointer d) {
    (void)w;(void)e;(void)d; act_open("xxri-power-menu"); return TRUE; }
static gboolean on_mute(GtkWidget* w, GdkEventButton* e, gpointer d) {
    (void)w;(void)e;(void)d;
    if (!S.audio_ok) return TRUE;
    run_cmd("xxri-audio toggle --json");
    state_read(); rebuild();
    return TRUE;
}
static void on_vol(GtkRange* r, gpointer d) {
    (void)d; if (g_setting) return;
    int v = (int)gtk_range_get_value(r);
    run_bg("xxri-audio volume set %d", v);
    S.volume = v;
}
static void on_bright(GtkRange* r, gpointer d) {
    (void)d; if (g_setting) return;
    int v = (int)gtk_range_get_value(r);
    run_bg("xxri-power brightness set %d", v);
    S.bright = v;
}

/* ------------------------------------------------------------- layout --- */
static GtkWidget* clickable(GtkWidget* child, GCallback cb) {
    GtkWidget* ev = gtk_event_box_new();
    gtk_event_box_set_visible_window(GTK_EVENT_BOX(ev), FALSE);
    gtk_container_add(GTK_CONTAINER(ev), child);
    if (cb) g_signal_connect(ev, "button-press-event", cb, NULL);
    return ev;
}
static GtkWidget* build_chip(void) {
    GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    css_class(row, "xxri-cc-chip");
    gtk_widget_set_size_request(row, -1, CHIP_H);
    gtk_box_pack_start(GTK_BOX(row), glyph(S.net_up && !strcmp(S.net_kind,"Wi-Fi") ? "wifi" : "eth",
                                           22, 20, S.net_up), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), glyph("bt", 22, 20, S.bt_ok), FALSE, FALSE, 0);
    if (S.batt_ok) {
        GtkWidget* b = gtk_drawing_area_new();
        gtk_widget_set_size_request(b, 20, 20);
        g_signal_connect(b, "draw", G_CALLBACK(batt_draw), NULL);
        gtk_box_pack_start(GTK_BOX(row), b, FALSE, FALSE, 0);
    }
    char t[32]; time_t now = time(NULL); struct tm* tm = localtime(&now);
    strftime(t, sizeof t, "%H:%M", tm);
    gtk_box_pack_start(GTK_BOX(row), lbl(t, "xxri-cc-clock", 0.5), FALSE, FALSE, 2);
    return clickable(row, G_CALLBACK(on_chip_click));
}
/* one of the two big state tiles at the top of the panel */
static GtkWidget* build_tile(const char* kind, const char* title, const char* sub,
                             gboolean on, GCallback cb) {
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    css_class(box, on ? "xxri-cc-tile on" : "xxri-cc-tile");
    gtk_widget_set_size_request(box, (PANEL_W-24-8)/2, 44);
    gtk_box_pack_start(GTK_BOX(box), glyph(kind, 30, 28, on), FALSE, FALSE, 0);
    GtkWidget* tv = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_valign(tv, GTK_ALIGN_CENTER);
    GtkWidget* t = lbl(title, "xxri-cc-tile-title", 0);
    GtkWidget* s = lbl(sub, on ? "xxri-cc-tile-sub" : "xxri-cc-tile-sub off", 0);
    gtk_label_set_ellipsize(GTK_LABEL(s), PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars(GTK_LABEL(s), 12);
    gtk_box_pack_start(GTK_BOX(tv), t, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(tv), s, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), tv, TRUE, TRUE, 0);
    return clickable(box, cb);
}
static GtkWidget* build_slider(const char* kind, int value, gboolean on,
                               GCallback icon_cb, GCallback change_cb, GtkWidget** out) {
    GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    css_class(row, "xxri-cc-slider-row");
    GtkWidget* ic = glyph(kind, 26, 24, on);
    gtk_box_pack_start(GTK_BOX(row), icon_cb ? clickable(ic, icon_cb) : ic, FALSE, FALSE, 0);
    GtkWidget* sc = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 100, 1);
    gtk_scale_set_draw_value(GTK_SCALE(sc), FALSE);
    gtk_range_set_value(GTK_RANGE(sc), CLAMP(value,0,100));
    gtk_widget_set_sensitive(sc, on);
    css_class(sc, "xxri-cc-slider");
    if (change_cb) g_signal_connect(sc, "value-changed", change_cb, NULL);
    gtk_box_pack_start(GTK_BOX(row), sc, TRUE, TRUE, 0);
    if (out) *out = sc;
    return row;
}
static GtkWidget* build_panel(void) {
    GtkWidget* col = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    css_class(col, "xxri-cc-panel");
    gtk_widget_set_size_request(col, PANEL_W, -1);

    GtkWidget* tiles = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_pack_start(GTK_BOX(tiles),
        build_tile(S.net_up && !strcmp(S.net_kind,"Wi-Fi") ? "wifi" : "eth",
                   S.net_kind, S.net_name, S.net_up, G_CALLBACK(on_wifi)), TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(tiles),
        build_tile("bt", "Bluetooth", S.bt_ok ? "On" : "Unavailable",
                   S.bt_ok, G_CALLBACK(on_bt)), TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(col), tiles, FALSE, FALSE, 0);

    if (S.audio_ok) {
        gtk_box_pack_start(GTK_BOX(col),
            build_slider("vol", S.volume, !S.muted, G_CALLBACK(on_mute),
                         G_CALLBACK(on_vol), &g_vol_scale), FALSE, FALSE, 0);
    } else {
        /* A dead slider reads as a broken control; name the reason instead. */
        GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
        css_class(row, "xxri-cc-slider-row");
        gtk_box_pack_start(GTK_BOX(row), glyph("vol", 26, 24, FALSE), FALSE, FALSE, 0);
        GtkWidget* l = lbl("No audio device", "xxri-cc-unavail", 0);
        gtk_widget_set_valign(l, GTK_ALIGN_CENTER);
        gtk_box_pack_start(GTK_BOX(row), l, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(col), row, FALSE, FALSE, 0);
    }
    if (S.bright_ok)
        gtk_box_pack_start(GTK_BOX(col),
            build_slider("sun", S.bright, TRUE, NULL, G_CALLBACK(on_bright),
                         &g_bright_scale), FALSE, FALSE, 0);

    /* bottom row: actions on the left, battery + clock on the right */
    GtkWidget* bot = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    css_class(bot, "xxri-cc-bottom");
    gtk_box_pack_start(GTK_BOX(bot), clickable(glyph("gear", 26, 24, TRUE),
                                               G_CALLBACK(on_settings)), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(bot), clickable(glyph("power", 26, 24, TRUE),
                                               G_CALLBACK(on_power)), FALSE, FALSE, 0);
    GtkWidget* spacer = gtk_label_new("");
    gtk_box_pack_start(GTK_BOX(bot), spacer, TRUE, TRUE, 0);
    /* pack_end lays out right-to-left, so this reads clock | battery on
       screen - the order the mockup uses. */
    char t1[32], t2[32]; time_t now = time(NULL); struct tm* tm = localtime(&now);
    strftime(t1, sizeof t1, "%I:%M %p", tm);
    strftime(t2, sizeof t2, "%a, %d %b", tm);
    GtkWidget* cl = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start(GTK_BOX(cl), lbl(t1, "xxri-cc-time", 1), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(cl), lbl(t2, "xxri-cc-date", 1), FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(bot), cl, FALSE, FALSE, 0);
    GtkWidget* sep = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
    css_class(sep, "xxri-cc-sep");
    gtk_box_pack_end(GTK_BOX(bot), sep, FALSE, FALSE, 5);
    if (S.batt_ok) {
        char pc[16]; snprintf(pc, sizeof pc, "%d%%", S.batt);
        gtk_box_pack_end(GTK_BOX(bot), lbl(pc, "xxri-cc-batt", 1), FALSE, FALSE, 0);
        GtkWidget* b = gtk_drawing_area_new();
        gtk_widget_set_size_request(b, 26, 26);
        g_signal_connect(b, "draw", G_CALLBACK(batt_draw), NULL);
        gtk_box_pack_end(GTK_BOX(bot), b, FALSE, FALSE, 0);
    }
    gtk_box_pack_start(GTK_BOX(col), bot, FALSE, FALSE, 0);
    return col;
}
static void rebuild(void) {
    if (g_root) gtk_widget_destroy(g_root);
    g_root = g_expanded ? build_panel() : build_chip();
    gtk_container_add(GTK_CONTAINER(g_win), g_root);
    gtk_widget_show_all(g_root);
    place_window();
}

/* ---------------------------------------------------------- lifecycle --- */
static gboolean tick(gpointer d) {          /* clock + cheap state refresh */
    (void)d;
    static int n = 0;
    if (!g_expanded) {
        if (++n % 4 == 0) state_read();     /* every ~2 min when collapsed */
        rebuild();
    }
    return G_SOURCE_CONTINUE;
}
/* A second launch does not start a second panel: it drops a one-word command
 * next to the pid file and pokes the running instance, which is how the dock,
 * the desktop menu and QA scripts drive it. */
volatile sig_atomic_t g_signalled = 0;
static void sig_cmd(int s) { (void)s; g_signalled++; }
static char* cmd_path(void) {
    return g_strdup_printf("%s/.cache/xxri-cc.cmd",
                           g_get_home_dir() ? g_get_home_dir() : "/tmp");
}
static gboolean check_cmd(gpointer d) {
    (void)d;
    static int last = 0;
    if (g_signalled == last) return G_SOURCE_CONTINUE;
    last = g_signalled;
    char* cp = cmd_path(); char* c = NULL;
    if (!g_file_get_contents(cp, &c, NULL, NULL)) c = g_strdup("toggle");
    g_strstrip(c);
    if      (!strcmp(c, "open"))   expand();
    else if (!strcmp(c, "close"))  collapse();
    else if (!strcmp(c, "quit"))   gtk_main_quit();
    else                           { if (g_expanded) collapse(); else expand(); }
    g_free(c); g_free(cp);
    return G_SOURCE_CONTINUE;
}

static void load_css(void) {
    GtkCssProvider* p = gtk_css_provider_new();
    if (g_file_test(CSS_PATH, G_FILE_TEST_EXISTS))
        gtk_css_provider_load_from_path(p, CSS_PATH, NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(p), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(p);
}
int main(int argc, char** argv) {
    /* single instance: a second launch toggles the running one instead of
       stacking a duplicate panel on the desktop */
    char pidpath[256];
    snprintf(pidpath, sizeof pidpath, "%s/.cache/xxri-cc.pid",
             g_get_home_dir() ? g_get_home_dir() : "/tmp");
    const char* want = "toggle";
    if (argc > 1) {
        if      (!strcmp(argv[1], "--open"))                       want = "open";
        else if (!strcmp(argv[1], "--close") ||
                 !strcmp(argv[1], "--collapse"))                   want = "close";
        else if (!strcmp(argv[1], "--quit"))                       want = "quit";
        else if (!strcmp(argv[1], "--toggle"))                     want = "toggle";
    }
    char* prev = NULL; gsize plen = 0;
    if (g_file_get_contents(pidpath, &prev, &plen, NULL)) {
        int op = atoi(prev); g_free(prev);
        if (op > 0 && kill(op, 0) == 0) {
            char* cp = cmd_path();
            g_file_set_contents(cp, want, -1, NULL);
            g_free(cp);
            kill(op, SIGUSR1);
            return 0;
        }
    }
    /* Nothing is running.  A close/quit request has nothing to do - and must
       NOT fall through and start a panel, which is exactly what made a QA hook
       hang waiting for a command that never returned. */
    if (!strcmp(want, "close") || !strcmp(want, "quit")) return 0;
    if (!strcmp(want, "open")) g_expanded = TRUE;
    if (!gtk_init_check(&argc, &argv)) { fprintf(stderr, "no display\n"); return 1; }
    { char buf[32]; snprintf(buf,sizeof buf,"%d",(int)getpid());
      g_file_set_contents(pidpath, buf, -1, NULL); }
    signal(SIGUSR1, sig_cmd);
    load_css();

    g_win = gtk_window_new(GTK_WINDOW_POPUP);       /* override-redirect:
        never in the task list, never takes the keyboard, always over the
        managed windows - and the window manager cannot decorate it */
    gtk_window_set_type_hint(GTK_WINDOW(g_win), GDK_WINDOW_TYPE_HINT_DOCK);
    gtk_window_set_keep_above(GTK_WINDOW(g_win), TRUE);
    gtk_window_set_accept_focus(GTK_WINDOW(g_win), FALSE);
    gtk_window_set_focus_on_map(GTK_WINDOW(g_win), FALSE);
    gtk_window_set_resizable(GTK_WINDOW(g_win), FALSE);
    gtk_widget_set_app_paintable(g_win, TRUE);
    css_class(g_win, "xxri-cc-window");
    gtk_widget_add_events(g_win, GDK_ENTER_NOTIFY_MASK|GDK_LEAVE_NOTIFY_MASK|GDK_BUTTON_PRESS_MASK);
    g_signal_connect(g_win, "leave-notify-event", G_CALLBACK(on_leave), NULL);
    g_signal_connect(g_win, "enter-notify-event", G_CALLBACK(on_enter), NULL);
    g_signal_connect(g_win, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    state_read();
    rebuild();
    gtk_widget_show_all(g_win);
    place_window();
    g_timeout_add_seconds(30, tick, NULL);
    g_timeout_add(250, check_cmd, NULL);
    gtk_main();
    unlink(pidpath);
    return 0;
}
