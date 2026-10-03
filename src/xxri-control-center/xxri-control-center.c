/* xxri-control-center - quick-settings panel, bottom right.
 *
 * Frontend only: everything goes through the xxri-* backends, same as
 * Settings. One override-redirect window that is either the status chip or
 * the expanded panel. No grabs, never takes focus. */
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <cairo.h>
#include <cairo-xlib.h>
#include <X11/Xatom.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include "xxri-theme.h"

#define CSS_PATH  "/usr/local/share/xxri-control-center/xxri-cc.css"
#define CSS_DARK  "/usr/local/share/xxri-control-center/xxri-cc-dark.css"
#define EDGE_X    18      /* gaps to the screen edges, from the mockup */
#define EDGE_Y     20
#define PANEL_W   264
#define SHADOW     12     /* shadow ring outside the panel, see draw_shadow() */
#define CHIP_H     38

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
    /* skip object/array values: xxri-audio emits {"volume":{"volume":65,...}}
       and the envelope key would match first */
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

/* XXRI_CC_DEBUG=1 appends a trace to ~/xxri-cc.log (no terminal at session start) */
static void trace(const char* fmt, ...) {
    static int on = -1;
    if (on < 0) on = g_getenv("XXRI_CC_DEBUG") ? 1 : 0;
    if (!on) return;
    char* path = g_strdup_printf("%s/xxri-cc.log", g_get_home_dir() ? g_get_home_dir() : "/tmp");
    FILE* f = fopen(path, "a"); g_free(path);
    if (!f) return;
    va_list ap; va_start(ap, fmt);
    char buf[512]; vsnprintf(buf, sizeof buf, fmt, ap); va_end(ap);
    time_t t = time(NULL); struct tm* tm = localtime(&t);
    fprintf(f, "%02d:%02d:%02d %s\n", tm->tm_hour, tm->tm_min, tm->tm_sec, buf);
    fclose(f);
}

typedef struct {
    gboolean net_up;  char* net_name;  char* net_kind;   /* wifi / ethernet */
    gboolean bt_ok;   char* bt_reason;   gboolean bt_on;   /* adapter usable / switched on */
    gboolean audio_ok; int volume; gboolean muted;
    gboolean bright_ok; int bright;
    gboolean batt_ok; int batt; gboolean charging;
} State;
static State S;
static GtkWidget *g_win, *g_root;
static gboolean g_expanded = FALSE;
static gboolean g_composited = FALSE;   /* a compositing manager is running */
static gboolean g_over_window = FALSE;  /* panel is not over the desktop */
static int g_chip_w = 120, g_chip_h = 38;   /* last collapsed size, for the reveal */
static guint g_reveal_timer = 0;
static int  g_reveal_step = 0;
static guint g_retract_timer = 0;
static int   g_retract_step = 0;
#define REVEAL_STEPS 7
static guint g_collapse_timer = 0;
static GtkWidget *g_vol_scale, *g_bright_scale;
static GtkWidget *g_chip_clock;      /* updated in place by the tick */
static char      *g_state_sig;       /* rebuild only when the state changes */
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
        char* st = jget(obj, "state");
        char* type  = jget(obj, "type");
        gboolean up = !strcmp(st, "up");
        gboolean wifi = !strcmp(type, "wifi");
        if (up && (!best || (wifi && !best_wifi))) { best = obj; best_wifi = wifi; }
        g_free(st); g_free(type);
        p += 12;
    }
    if (best) {
        S.net_up = TRUE;
        char* ssid  = jget(best, "ssid");
        char* iface = jget(best, "interface");
        char* ip    = jget(best, "ip");
        S.net_kind = g_strdup(best_wifi ? "Wi-Fi" : "Ethernet");
        /* SSID on Wi-Fi, the address on a wire; the interface name last */
        if (*ssid)      S.net_name = g_strdup(ssid);
        else if (*ip)   S.net_name = g_strdup(ip);
        else            S.net_name = g_strdup(iface);
        g_free(ssid); g_free(iface); g_free(ip);
    } else {
        S.net_up = FALSE;
        S.net_kind = g_strdup("Network");
        S.net_name = g_strdup("Not connected");
    }
    g_free(net);

    char* bt = run_cmd("xxri-bluetooth status --json");
    S.bt_ok = jbool(bt, "available");
    S.bt_on = S.bt_ok && jbool(bt, "enabled");
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

/* one class per gtk call, so split "a b" */
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
/* no icon theme on the device, so glyphs are drawn with cairo (like the Store) */
typedef struct { const char* kind; int size; gboolean on; } Glyph;
static gboolean glyph_draw(GtkWidget* w, cairo_t* cr, gpointer data) {
    Glyph* g = data;
    int W = gtk_widget_get_allocated_width(w), H = gtk_widget_get_allocated_height(w);
    double s = g->size, cx = W/2.0, cy = H/2.0;
    /* brand tile, purple -> pink */
    if (g->on) {
        cairo_pattern_t* pat = cairo_pattern_create_linear(cx-s/2, cy-s/2, cx+s/2, cy+s/2);
        cairo_pattern_add_color_stop_rgb(pat, 0, 0.51, 0.24, 0.93);
        cairo_pattern_add_color_stop_rgb(pat, 1, 0.93, 0.16, 0.78);
        cairo_set_source(cr, pat);
        cairo_pattern_destroy(pat);
    } else if (xxri_theme_dark()) {
        cairo_set_source_rgb(cr, 0.33, 0.33, 0.37);
    } else {
        cairo_set_source_rgb(cr, 0.80, 0.80, 0.83);
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

static void rebuild(void);
static void collapse(void);
static void begin_collapse(void);

static gboolean on_leave(GtkWidget* w, GdkEventCrossing* e, gpointer d) {
    (void)w; (void)d;
    trace("leave detail=%d expanded=%d", e->detail, g_expanded);
    if (!g_expanded || e->detail == GDK_NOTIFY_INFERIOR) return FALSE;
    /* the shape is still growing during the reveal, X reports a bogus leave */
    if (g_reveal_timer) return FALSE;
    if (g_collapse_timer) g_source_remove(g_collapse_timer);
    g_collapse_timer = g_timeout_add(650, (GSourceFunc)(void*)begin_collapse, NULL);
    return FALSE;
}
static void expand(void);
static gboolean on_enter(GtkWidget* w, GdkEventCrossing* e, gpointer d) {
    (void)w; (void)d;
    if (g_collapse_timer) { g_source_remove(g_collapse_timer); g_collapse_timer = 0; }
    /* hover opens it, no click needed */
    if (!g_expanded && e->detail != GDK_NOTIFY_INFERIOR) expand();
    return FALSE;
}

/* Corner hotspot: the chip is inset from the edges, so a pointer shoved into
 * the corner misses it. Polls the root pointer; an input-only window there
 * never got its crossing events. */
#define HOT_W 26
#define HOT_H 26

static gboolean hotspot_poll(gpointer d) {
    (void)d;
    Display* xd = GDK_DISPLAY_XDISPLAY(gdk_display_get_default());
    if (!xd) return TRUE;
    Window root = DefaultRootWindow(xd), r, c;
    int rx = 0, ry = 0, wx, wy; unsigned int mask;
    if (!XQueryPointer(xd, root, &r, &c, &rx, &ry, &wx, &wy, &mask)) return TRUE;
    GdkScreen* sc = gdk_screen_get_default();
    int sw = gdk_screen_get_width(sc), sh = gdk_screen_get_height(sc);
    gboolean in_corner = (rx >= sw - HOT_W && ry >= sh - HOT_H);
    if (in_corner) {
        if (g_collapse_timer) { g_source_remove(g_collapse_timer); g_collapse_timer = 0; }
        if (!g_expanded) { trace("hotspot enter at %d,%d", rx, ry); expand(); }
        return TRUE;
    }

    /* retract from here too: opened from the hotspot there was no enter on the
     * panel, so no leave will ever come */
    if (g_expanded && !g_retract_timer) {
        GdkWindow* gw = gtk_widget_get_window(g_win);
        int wx = 0, wy = 0, ww = 0, wh = 0;
        if (gw) {
            gdk_window_get_origin(gw, &wx, &wy);
            ww = gtk_widget_get_allocated_width(g_win);
            wh = gtk_widget_get_allocated_height(g_win);
        }
        const int SLACK = 10;
        gboolean on_panel = ww > 0 &&
            rx >= wx - SLACK && rx < wx + ww + SLACK &&
            ry >= wy - SLACK && ry < wy + wh + SLACK;
        if (on_panel) {
            if (g_collapse_timer) { g_source_remove(g_collapse_timer); g_collapse_timer = 0; }
        } else if (!g_collapse_timer) {
            trace("pointer left panel at %d,%d (panel %dx%d+%d+%d)", rx, ry, ww, wh, wx, wy);
            g_collapse_timer = g_timeout_add(500, (GSourceFunc)(void*)begin_collapse, NULL);
        }
    }
    return TRUE;
}

/* ---- glass without a compositor
 * Pseudo-transparency: copy the area behind the panel out of the root pixmap
 * (_XROOTPMAP_ID), blur it, tint it. Only sees the wallpaper, not windows, so
 * it backs off to a flat surface when a window is behind (something_behind).
 * With xxri-compositor running none of this is used, see on_win_draw(). */
static cairo_surface_t* g_glass = NULL;
static int g_gx, g_gy, g_gw, g_gh;

static void glass_invalidate(void) {
    if (g_glass) { cairo_surface_destroy(g_glass); g_glass = NULL; }
    g_gw = g_gh = 0;
}

/* the root pixmap as a cairo surface, NULL if none is published */
static cairo_surface_t* wallpaper_surface(void) {
    Display* xd = GDK_DISPLAY_XDISPLAY(gdk_display_get_default());
    if (!xd) return NULL;
    Window root = DefaultRootWindow(xd);
    Atom prop = XInternAtom(xd, "_XROOTPMAP_ID", True);
    if (prop == None) return NULL;
    Atom type; int fmt; unsigned long n = 0, after = 0; unsigned char* p = NULL;
    if (XGetWindowProperty(xd, root, prop, 0, 1, False, XA_PIXMAP,
                           &type, &fmt, &n, &after, &p) != Success || !p || n < 1) {
        if (p) XFree(p);
        return NULL;
    }
    Pixmap pm = *(Pixmap*)p;
    XFree(p);
    if (!pm) return NULL;
    GdkScreen* sc = gdk_screen_get_default();
    cairo_surface_t* wall = cairo_xlib_surface_create(
        xd, pm, DefaultVisual(xd, DefaultScreen(xd)),
        gdk_screen_get_width(sc), gdk_screen_get_height(sc));
    if (cairo_surface_status(wall) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(wall); return NULL;
    }
    return wall;
}

/* WM_STATE on the window or below it (clients sit inside the WM's frame) */
static gboolean has_wm_state(Display* xd, Window w, Atom wm_state, int depth) {
    if (wm_state == None) return FALSE;
    Atom type; int fmt; unsigned long n = 0, after = 0; unsigned char* p = NULL;
    if (XGetWindowProperty(xd, w, wm_state, 0, 1, False, AnyPropertyType,
                           &type, &fmt, &n, &after, &p) == Success && p) {
        XFree(p);
        if (type != None) return TRUE;
    }
    if (depth <= 0) return FALSE;
    Window r, parent, *kids = NULL; unsigned int nk = 0, i;
    if (!XQueryTree(xd, w, &r, &parent, &kids, &nk)) return FALSE;
    gboolean found = FALSE;
    for (i = 0; i < nk && !found; i++)
        found = has_wm_state(xd, kids[i], wm_state, depth - 1);
    if (kids) XFree(kids);
    return found;
}

/* Is an app window behind the panel? Then the wallpaper glass and shadow
 * would be lies (wallpaper "showing through" a dark window), go solid. */
static gboolean something_behind(int x, int y, int w, int h) {
    Display* xd = GDK_DISPLAY_XDISPLAY(gdk_display_get_default());
    if (!xd) return FALSE;
    Window root = DefaultRootWindow(xd), r, parent, *kids = NULL;
    unsigned int nk = 0, i;
    if (!XQueryTree(xd, root, &r, &parent, &kids, &nk)) return FALSE;
    Window self = None;
    GdkWindow* gw = gtk_widget_get_window(g_win);
    if (gw) self = GDK_WINDOW_XID(gw);
    GdkScreen* sc = gdk_screen_get_default();
    long area = (long)gdk_screen_get_width(sc) * gdk_screen_get_height(sc);
    gboolean hit = FALSE;
    Atom wm_state = XInternAtom(xd, "WM_STATE", True);
    for (i = 0; i < nk && !hit; i++) {
        if (kids[i] == self) continue;
        XWindowAttributes a;
        if (!XGetWindowAttributes(xd, kids[i], &a)) continue;
        if (a.map_state != IsViewable || a.class == InputOnly) continue;
        /* managed windows only. Don't test override_redirect: flwm's frames
         * are override-redirect too */
        if (!has_wm_state(xd, kids[i], wm_state, 2)) continue;
        if ((long)a.width * a.height >= area) continue;  /* root-sized */
        int rx, ry; Window child;
        XTranslateCoordinates(xd, kids[i], root, 0, 0, &rx, &ry, &child);
        if (rx < x + w && rx + a.width  > x &&
            ry < y + h && ry + a.height > y) {
            hit = TRUE;
            trace("behind: win 0x%lx %dx%d+%d+%d overlaps panel %dx%d+%d+%d",
                  (unsigned long)kids[i], a.width, a.height, rx, ry, w, h, x, y);
        }
    }
    if (kids) XFree(kids);
    return hit;
}

static gboolean have_wallpaper(void) {
    cairo_surface_t* w = wallpaper_surface();
    if (!w) return FALSE;
    cairo_surface_destroy(w);
    return TRUE;
}

/* unblurred wallpaper under the window, for the shadow ring */
static cairo_surface_t* g_raw = NULL;
static int g_rx, g_ry, g_rw, g_rh;

static void raw_invalidate(void) {
    if (g_raw) { cairo_surface_destroy(g_raw); g_raw = NULL; }
    g_rw = g_rh = 0;
}

static cairo_surface_t* wallpaper_slice(int x, int y, int w, int h) {
    if (g_raw && x == g_rx && y == g_ry && w == g_rw && h == g_rh) return g_raw;
    raw_invalidate();
    if (w <= 0 || h <= 0) return NULL;
    cairo_surface_t* wall = wallpaper_surface();
    if (!wall) return NULL;
    cairo_surface_t* out = cairo_image_surface_create(CAIRO_FORMAT_RGB24, w, h);
    cairo_t* c = cairo_create(out);
    cairo_set_source_surface(c, wall, -x, -y);
    cairo_pattern_set_extend(cairo_get_source(c), CAIRO_EXTEND_PAD);
    cairo_paint(c);
    cairo_destroy(c);
    cairo_surface_destroy(wall);
    g_raw = out; g_rx = x; g_ry = y; g_rw = w; g_rh = h;
    return g_raw;
}

static cairo_surface_t* glass_for(int x, int y, int w, int h) {
    if (g_glass && x == g_gx && y == g_gy && w == g_gw && h == g_gh) return g_glass;
    glass_invalidate();
    if (w <= 0 || h <= 0) return NULL;

    cairo_surface_t* wall = wallpaper_surface();
    if (!wall) return NULL;

    /* cheap blur: scale down and back up */
    const int DOWN = 7;
    int bw = w / DOWN + 2, bh = h / DOWN + 2;
    cairo_surface_t* small = cairo_image_surface_create(CAIRO_FORMAT_RGB24, bw, bh);
    cairo_t* s2 = cairo_create(small);
    cairo_scale(s2, 1.0 / DOWN, 1.0 / DOWN);
    cairo_set_source_surface(s2, wall, -x, -y);
    cairo_pattern_set_extend(cairo_get_source(s2), CAIRO_EXTEND_PAD);
    cairo_paint(s2);
    cairo_destroy(s2);

    cairo_surface_t* out = cairo_image_surface_create(CAIRO_FORMAT_RGB24, w, h);
    cairo_t* oc = cairo_create(out);
    cairo_save(oc);
    cairo_scale(oc, DOWN, DOWN);
    cairo_set_source_surface(oc, small, 0, 0);
    cairo_pattern_set_filter(cairo_get_source(oc), CAIRO_FILTER_BILINEAR);
    cairo_pattern_set_extend(cairo_get_source(oc), CAIRO_EXTEND_PAD);
    cairo_paint(oc);
    cairo_restore(oc);
    /* surface tint: light glass, or Settings' dark window colour */
    if (xxri_theme_dark()) cairo_set_source_rgba(oc, 0.141, 0.129, 0.141, 0.72);
    else cairo_set_source_rgba(oc, 0.945, 0.945, 0.955, 0.68);
    cairo_paint(oc);
    cairo_destroy(oc);
    cairo_surface_destroy(small);
    cairo_surface_destroy(wall);

    g_glass = out; g_gx = x; g_gy = y; g_gw = w; g_gh = h;
    return g_glass;
}

static void rrect(cairo_t* cr, double x, double y, double w, double h, double r) {
    if (r > w/2) r = w/2;
    if (r > h/2) r = h/2;
    cairo_new_sub_path(cr);
    cairo_arc(cr, x+w-r, y+r,   r, -G_PI/2, 0);
    cairo_arc(cr, x+w-r, y+h-r, r, 0, G_PI/2);
    cairo_arc(cr, x+r,   y+h-r, r, G_PI/2, G_PI);
    cairo_arc(cr, x+r,   y+r,   r, G_PI, 1.5*G_PI);
    cairo_close_path(cr);
}

/* Shadow without a compositor: the shape mask is 1-bit, so the margin is
 * painted with the wallpaper itself, darkened next to the panel and fading to
 * the untouched wallpaper at the edge, where the hard mask edge can't be seen. */
static void draw_shadow(cairo_t* cr, int ww, int wh, int radius) {
    const int steps = SHADOW;
    for (int i = steps; i >= 1; i--) {
        double t = (double)i / steps;          /* 1 at the outside, 0 inside */
        double a = 0.055 * (1.0 - t) * (1.0 - t) * 3.2;
        if (a <= 0.0) continue;
        cairo_set_source_rgba(cr, 0, 0, 0, a);
        rrect(cr, SHADOW - i, SHADOW - i + 2.0,
              ww - 2*(SHADOW - i), wh - 2*(SHADOW - i), radius + i);
        cairo_fill(cr);
    }
}

/* runs before the widgets, the CSS panel gradient is translucent on top */
static gboolean on_win_draw(GtkWidget* w, cairo_t* cr, gpointer d) {
    (void)d;
    GdkWindow* gw = gtk_widget_get_window(w);
    int ww = gtk_widget_get_allocated_width(w);
    int wh = gtk_widget_get_allocated_height(w);
    int wx = 0, wy = 0;
    if (gw) gdk_window_get_origin(gw, &wx, &wy);
    int pw = ww - 2*SHADOW, ph = wh - 2*SHADOW;
    int radius = g_expanded ? 16 : ph/2;

    /* compositor: real alpha, none of the wallpaper tricks below */
    if (g_composited) {
        cairo_save(cr);
        cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
        cairo_set_source_rgba(cr, 0, 0, 0, 0);
        cairo_paint(cr);
        cairo_restore(cr);
        for (int i = SHADOW; i >= 1; i--) {
            double t = (double)i / SHADOW;
            double a = 0.10 * (1.0 - t) * (1.0 - t);
            cairo_set_source_rgba(cr, 0, 0, 0, a);
            rrect(cr, SHADOW - i, SHADOW - i + 2.0,
                  ww - 2*(SHADOW - i), wh - 2*(SHADOW - i), radius + i);
            cairo_fill(cr);
        }
        return FALSE;      /* the CSS gradient paints the panel */
    }

    /* 1. plain wallpaper under everything (unless a window is behind us) */
    cairo_surface_t* raw = g_over_window ? NULL
                         : wallpaper_slice(wx, wy, ww, wh);
    cairo_save(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    gboolean dk = xxri_theme_dark();
    if (raw) { cairo_set_source_surface(cr, raw, 0, 0); cairo_paint(cr); }
    else if (dk) { cairo_set_source_rgb(cr, 0.141, 0.129, 0.141); cairo_paint(cr); }
    else     { cairo_set_source_rgb(cr, 0.93, 0.93, 0.94); cairo_paint(cr); }
    cairo_restore(cr);

    /* 2. shadow ring */
    if (raw) draw_shadow(cr, ww, wh, radius);

    /* 3. frosted panel */
    cairo_save(cr);
    rrect(cr, SHADOW, SHADOW, pw, ph, radius);
    cairo_clip(cr);
    cairo_surface_t* g = g_over_window ? NULL
                       : glass_for(wx + SHADOW, wy + SHADOW, pw, ph);
    if (g) { cairo_set_source_surface(cr, g, SHADOW, SHADOW); cairo_paint(cr); }
    else if (dk) { cairo_set_source_rgb(cr, 0.165, 0.169, 0.200); cairo_paint(cr); }
    else   { cairo_set_source_rgb(cr, 0.93, 0.93, 0.94); cairo_paint(cr); }
    cairo_restore(cr);

    /* 4. rim, same hairline as the dock */
    cairo_save(cr);
    cairo_set_line_width(cr, 1.0);
    cairo_set_source_rgba(cr, 1, 1, 1, dk ? 0.12 : 0.55);
    rrect(cr, SHADOW + 0.5, SHADOW + 0.5, pw - 1, ph - 1, radius);
    cairo_stroke(cr);
    cairo_restore(cr);
    return FALSE;
}

/* No compositor: clip the fw x fh window to a rounded rect at ox,oy with the
 * Shape extension. Also animates the reveal. */
static void apply_shape_at(int ox, int oy, int w, int h, int radius, int fw, int fh) {
    GdkWindow* gw = gtk_widget_get_window(g_win);
    if (!gw || w <= 0 || h <= 0) return;
    cairo_surface_t* m = cairo_image_surface_create(CAIRO_FORMAT_A8, fw, fh);
    cairo_t* cr = cairo_create(m);
    cairo_set_source_rgba(cr, 0,0,0,0); cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_paint(cr);
    cairo_set_source_rgba(cr, 0,0,0,1);
    rrect(cr, ox, oy, w, h, radius);
    cairo_fill(cr);
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
    if (!g_expanded) { g_chip_w = w; g_chip_h = h; }
    /* panel plus the shadow ring */
    int fw = w + 2*SHADOW, fh = h + 2*SHADOW;
    trace("place expanded=%d nat=%dx%d -> %dx%d at %d,%d (screen %dx%d)",
          g_expanded, nat.width, nat.height, w, h, sw-EDGE_X-w, sh-EDGE_Y-h, sw, sh);
    glass_invalidate();
    raw_invalidate();
    gtk_window_resize(GTK_WINDOW(g_win), fw, fh);
    gtk_window_move(GTK_WINDOW(g_win), sw - EDGE_X - w - SHADOW, sh - EDGE_Y - h - SHADOW);
    gboolean was_over = g_over_window;
    g_over_window = something_behind(sw - EDGE_X - w, sh - EDGE_Y - h, w, h);
    trace("place: panel %dx%d+%d+%d over_window=%d", w, h,
          sw - EDGE_X - w, sh - EDGE_Y - h, g_over_window);
    /* the surface depends on what's behind, repaint when that changes */
    if (was_over != g_over_window) { glass_invalidate(); raw_invalidate(); gtk_widget_queue_draw(g_win); }
    /* chip = pill, panel = 16px radius. The shadow ring is painted with the
     * wallpaper, so without one (or over a window) shape it away */
    if (g_composited) {
        /* alpha does the shape, a mask would only alias the corners */
        GdkWindow* gwin = gtk_widget_get_window(g_win);
        if (gwin) gdk_window_shape_combine_region(gwin, NULL, 0, 0);
    } else if (have_wallpaper() && !g_over_window) {
        apply_shape_at(0, 0, fw, fh, (g_expanded ? 16 : h/2) + SHADOW, fw, fh);
    } else {
        apply_shape_at(SHADOW, SHADOW, w, h, g_expanded ? 16 : h/2, fw, fh);
    }
    GdkWindow* gw = gtk_widget_get_window(g_win);   /* NULL before realize */
    if (gw) gdk_window_raise(gw);
}
/* Reveal: only the shape mask animates, from the chip's pill to the full
 * panel. No relayout, no resize, nothing to tear without a compositor. */
static gboolean reveal_tick(gpointer d) {
    (void)d;
    if (!g_expanded) { g_reveal_step = 0; g_reveal_timer = 0; return FALSE; }
    GtkRequisition nat;
    gtk_widget_get_preferred_size(g_root, NULL, &nat);
    int pw = PANEL_W, ph = nat.height;
    int fw = pw + 2*SHADOW, fh = ph + 2*SHADOW;

    g_reveal_step++;
    double t = (double)g_reveal_step / REVEAL_STEPS;
    if (t > 1.0) t = 1.0;
    double e = 1.0 - (1.0 - t) * (1.0 - t);        /* ease out */

    int cw = g_chip_w, ch = g_chip_h;
    int w = (int)(cw + (pw - cw) * e);
    int h = (int)(ch + (ph - ch) * e);
    /* anchored bottom-right, where the chip is */
    int ox = SHADOW + (pw - w), oy = SHADOW + (ph - h);
    int radius = (int)(ch/2 + (16 - ch/2) * e);
    if (radius < 4) radius = 4;
    apply_shape_at(ox, oy, w, h, radius, fw, fh);

    if (g_reveal_step >= REVEAL_STEPS) {
        g_reveal_timer = 0;
        place_window();          /* final shape, with the shadow */
        return FALSE;
    }
    return TRUE;
}

/* retract: the reveal played backwards */
static gboolean retract_tick(gpointer d) {
    (void)d;
    if (!g_expanded) { g_retract_step = 0; g_retract_timer = 0; return FALSE; }
    GtkRequisition nat;
    gtk_widget_get_preferred_size(g_root, NULL, &nat);
    int pw = PANEL_W, ph = nat.height;
    int fw = pw + 2*SHADOW, fh = ph + 2*SHADOW;

    g_retract_step++;
    double t = (double)g_retract_step / REVEAL_STEPS;
    if (t > 1.0) t = 1.0;
    double e = 1.0 - t * t;                       /* accelerate away */

    int cw = g_chip_w, ch = g_chip_h;
    int w = (int)(cw + (pw - cw) * e);
    int h = (int)(ch + (ph - ch) * e);
    int ox = SHADOW + (pw - w), oy = SHADOW + (ph - h);
    int radius = (int)(ch/2 + (16 - ch/2) * e);
    if (radius < 4) radius = 4;
    apply_shape_at(ox, oy, w, h, radius, fw, fh);

    if (g_retract_step >= REVEAL_STEPS) {
        g_retract_timer = 0;
        g_retract_step = 0;
        collapse();
        return FALSE;
    }
    return TRUE;
}

/* leave timer: animate, then collapse */
static void begin_collapse(void) {
    if (!g_expanded) return;
    if (g_reveal_timer) { g_source_remove(g_reveal_timer); g_reveal_timer = 0; }
    if (g_retract_timer) return;                   /* udah on its way out */
    g_retract_step = 0;
    g_retract_timer = g_timeout_add(16, retract_tick, NULL);
}

static void start_reveal(void) {
    if (g_retract_timer) { g_source_remove(g_retract_timer); g_retract_timer = 0; g_retract_step = 0; }
    if (g_reveal_timer) g_source_remove(g_reveal_timer);
    g_reveal_step = 0;
    g_reveal_timer = g_timeout_add(18, reveal_tick, NULL);
}

static void expand(void) {
    trace("expand (expanded=%d)", g_expanded);
    if (g_expanded) return;
    state_read();
    g_expanded = TRUE;
    rebuild();
    start_reveal();
}
static void collapse(void) {
    trace("collapse (expanded=%d)", g_expanded);
    if (g_collapse_timer) { g_source_remove(g_collapse_timer); g_collapse_timer = 0; }
    if (g_reveal_timer) { g_source_remove(g_reveal_timer); g_reveal_timer = 0; }
    if (g_retract_timer) { g_source_remove(g_retract_timer); g_retract_timer = 0; }
    g_reveal_step = 0; g_retract_step = 0;
    if (!g_expanded) return;
    g_expanded = FALSE;
    rebuild();
}
static gboolean on_chip_click(GtkWidget* w, GdkEventButton* e, gpointer d) {
    (void)w; (void)d;
    trace("chip click button=%d at %.0f,%.0f expanded=%d", e->button, e->x, e->y, g_expanded);
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
    gtk_box_pack_start(GTK_BOX(row), glyph("bt", 22, 20, S.bt_on), FALSE, FALSE, 0);
    if (S.batt_ok) {
        GtkWidget* b = gtk_drawing_area_new();
        gtk_widget_set_size_request(b, 20, 20);
        g_signal_connect(b, "draw", G_CALLBACK(batt_draw), NULL);
        gtk_box_pack_start(GTK_BOX(row), b, FALSE, FALSE, 0);
    }
    /* 12h clock, same as the panel */
    char t[32]; time_t now = time(NULL); struct tm* tm = localtime(&now);
    strftime(t, sizeof t, "%l:%M %p", tm);
    g_chip_clock = lbl(g_strstrip(t), "xxri-cc-clock", 0.5);
    gtk_box_pack_start(GTK_BOX(row), g_chip_clock, FALSE, FALSE, 2);
    return clickable(row, G_CALLBACK(on_chip_click));
}
/* Wi-Fi / Bluetooth tile */
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
    char pc[16]; snprintf(pc, sizeof pc, "%d%%", CLAMP(value,0,100));
    GtkWidget* v = lbl(pc, on ? "xxri-cc-val" : "xxri-cc-val-off", 1);
    gtk_widget_set_size_request(v, 30, -1);
    gtk_widget_set_valign(v, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(row), v, FALSE, FALSE, 0);
    if (out) *out = sc;
    return row;
}
static GtkWidget* build_panel(void) {
    /* 6px base spacing, groups add their own margins */
    GtkWidget* col = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    css_class(col, "xxri-cc-panel");
    gtk_widget_set_size_request(col, PANEL_W, -1);

    GtkWidget* tiles = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_pack_start(GTK_BOX(tiles),
        build_tile(S.net_up && !strcmp(S.net_kind,"Wi-Fi") ? "wifi" : "eth",
                   S.net_kind, S.net_name, S.net_up, G_CALLBACK(on_wifi)), TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(tiles),
        build_tile("bt", "Bluetooth", S.bt_on ? "On" : S.bt_ok ? "Off" : "Unavailable",
                   S.bt_on, G_CALLBACK(on_bt)), TRUE, TRUE, 0);
    gtk_widget_set_margin_bottom(tiles, 4);
    gtk_box_pack_start(GTK_BOX(col), tiles, FALSE, FALSE, 0);

    if (S.audio_ok) {
        gtk_box_pack_start(GTK_BOX(col),
            build_slider("vol", S.volume, !S.muted, G_CALLBACK(on_mute),
                         G_CALLBACK(on_vol), &g_vol_scale), FALSE, FALSE, 0);
    } else {
        /* say why instead of showing a dead slider */
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

    /* hairline before the session row */
    GtkWidget* rule = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    css_class(rule, "xxri-cc-rule");
    gtk_widget_set_margin_top(rule, 5);
    gtk_widget_set_margin_bottom(rule, 3);
    gtk_box_pack_start(GTK_BOX(col), rule, FALSE, FALSE, 0);

    /* bottom row: actions on the left, battery + clock on the right */
    GtkWidget* bot = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    css_class(bot, "xxri-cc-bottom");
    gtk_box_pack_start(GTK_BOX(bot), clickable(glyph("gear", 26, 24, TRUE),
                                               G_CALLBACK(on_settings)), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(bot), clickable(glyph("power", 26, 24, TRUE),
                                               G_CALLBACK(on_power)), FALSE, FALSE, 0);
    GtkWidget* spacer = gtk_label_new("");
    gtk_box_pack_start(GTK_BOX(bot), spacer, TRUE, TRUE, 0);
    /* pack_end goes right to left: on screen battery | clock, as in the mockup */
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
    /* keep GTK out of the shadow margin */
    gtk_container_set_border_width(GTK_CONTAINER(g_win), SHADOW);
    gtk_container_add(GTK_CONTAINER(g_win), g_root);
    gtk_widget_show_all(g_root);
    place_window();
}

/* what the chip shows; rebuild only when it changes (rebuilding every tick
   made the chip flicker) */
static char* state_sig(void) {
    return g_strdup_printf("%d|%s|%d|%d|%d|%d",
        S.net_up, S.net_name ? S.net_name : "", S.bt_ok, S.bt_on, S.batt_ok, S.batt);
}
static gboolean tick(gpointer d) {          /* clock + cheap state refresh */
    (void)d;
    static int n = 0;
    if (g_expanded) return G_SOURCE_CONTINUE;

    if (g_chip_clock && GTK_IS_LABEL(g_chip_clock)) {
        char t[32]; time_t now = time(NULL); struct tm* tm = localtime(&now);
        strftime(t, sizeof t, "%l:%M %p", tm);
        gtk_label_set_text(GTK_LABEL(g_chip_clock), g_strstrip(t));
    }
    if (++n % 4 == 0) {                     /* re-read hardware every ~2 min */
        state_read();
        char* sig = state_sig();
        if (!g_state_sig || strcmp(sig, g_state_sig)) {
            g_free(g_state_sig); g_state_sig = sig;
            trace("state changed -> rebuild: %s", sig);
            rebuild();
        } else g_free(sig);
    }
    return G_SOURCE_CONTINUE;
}
/* A second launch writes a one-word command next to the pid file and sends
 * SIGUSR1 to the running panel (dock, desktop menu and QA drive it so). */
volatile sig_atomic_t g_signalled = 0;
static void sig_cmd(int s) { (void)s; g_signalled++; }
/* Must be boot-local (tmpfs). In ~/.cache a pid from the last boot survived a
 * power-off, and if a stranger had that pid now the panel signalled it and
 * exited - no chip on ~1 boot in 4. */
static char* cc_runtime_path(const char* leaf) {
    const char* rt = g_getenv("XDG_RUNTIME_DIR");
    if (rt && *rt) return g_strdup_printf("%s/xxri-cc.%s", rt, leaf);
    return g_strdup_printf("/tmp/xxri-cc-%u.%s", (unsigned)getuid(), leaf);
}
static char* cmd_path(void) {
    return cc_runtime_path("cmd");
}
static gboolean check_cmd(gpointer d) {
    (void)d;
    static int last = 0;
    if (g_signalled == last) return G_SOURCE_CONTINUE;
    last = g_signalled;
    char* path = cmd_path(); char* cmd = NULL;
    if (!g_file_get_contents(path, &cmd, NULL, NULL)) cmd = g_strdup("toggle");
    g_strstrip(cmd);
    if      (!strcmp(cmd, "open"))   expand();
    else if (!strcmp(cmd, "close"))  collapse();
    else if (!strcmp(cmd, "quit"))   gtk_main_quit();
    else                             { if (g_expanded) collapse(); else expand(); }
    g_free(cmd); g_free(path);
    return G_SOURCE_CONTINUE;
}

/* Light/Dark switched in Settings: the drawn frost is cached, so drop it */
static void theme_changed(gboolean dark, gpointer d) {
    (void)dark; (void)d;
    glass_invalidate();
    GList* tops = gtk_window_list_toplevels();
    for (GList* l = tops; l; l = l->next) gtk_widget_queue_draw(GTK_WIDGET(l->data));
    g_list_free(tops);
}
static void load_css(void) {
    GtkCssProvider* css = gtk_css_provider_new();
    if (g_file_test(CSS_PATH, G_FILE_TEST_EXISTS))
        gtk_css_provider_load_from_path(css, CSS_PATH, NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css);
    xxri_theme_init(CSS_DARK, theme_changed, NULL);
}
static gboolean first_state_read(gpointer d) {
    (void)d;
    state_read();
    g_free(g_state_sig);
    g_state_sig = state_sig();
    rebuild();
    return G_SOURCE_REMOVE;
}

int main(int argc, char** argv) {
    /* single instance: a second launch controls the running one */
    char pidpath[256];
    { char* rp = cc_runtime_path("pid");
      snprintf(pidpath, sizeof pidpath, "%s", rp);
      g_free(rp); }
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
        int pid = atoi(prev); g_free(prev);
        if (pid > 0 && kill(pid, 0) == 0) {
            char* path = cmd_path();
            g_file_set_contents(path, want, -1, NULL);
            g_free(path);
            kill(pid, SIGUSR1);
            return 0;
        }
    }
    /* nothing running: close/quit must not start a panel (a QA hook hung on it) */
    if (!strcmp(want, "close") || !strcmp(want, "quit")) return 0;
    if (!strcmp(want, "open")) g_expanded = TRUE;
    if (!gtk_init_check(&argc, &argv)) { fprintf(stderr, "no display\n"); return 1; }
    { char buf[32]; snprintf(buf,sizeof buf,"%d",(int)getpid());
      g_file_set_contents(pidpath, buf, -1, NULL); }
    signal(SIGUSR1, sig_cmd);
    load_css();

    /* override-redirect: no task list entry, no focus, no decorations */
    g_win = gtk_window_new(GTK_WINDOW_POPUP);
    gtk_window_set_type_hint(GTK_WINDOW(g_win), GDK_WINDOW_TYPE_HINT_DOCK);
    gtk_window_set_keep_above(GTK_WINDOW(g_win), TRUE);
    gtk_window_set_accept_focus(GTK_WINDOW(g_win), FALSE);
    gtk_window_set_focus_on_map(GTK_WINDOW(g_win), FALSE);
    gtk_window_set_resizable(GTK_WINDOW(g_win), FALSE);
    gtk_widget_set_app_paintable(g_win, TRUE);
    {
        GdkScreen* sc0 = gtk_widget_get_screen(g_win);
        GdkVisual* rgba = gdk_screen_get_rgba_visual(sc0);
        g_composited = gdk_screen_is_composited(sc0) && rgba != NULL;
        if (g_composited) gtk_widget_set_visual(g_win, rgba);
        trace("composited=%d rgba_visual=%p", g_composited, (void*)rgba);
    }
    css_class(g_win, "xxri-cc-window");
    gtk_widget_add_events(g_win, GDK_ENTER_NOTIFY_MASK|GDK_LEAVE_NOTIFY_MASK|GDK_BUTTON_PRESS_MASK);
    g_signal_connect(g_win, "leave-notify-event", G_CALLBACK(on_leave), NULL);
    g_signal_connect(g_win, "enter-notify-event", G_CALLBACK(on_enter), NULL);
    g_signal_connect(g_win, "draw", G_CALLBACK(on_win_draw), NULL);
    g_signal_connect(g_win, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    /* Map the chip before asking the backends. At boot they are slow and
     * sometimes never return, and the chip used to wait for them (missing on
     * ~1 boot in 3). The real state comes in from an idle callback. */
    g_state_sig = state_sig();
    rebuild();
    gtk_widget_show_all(g_win);
    place_window();
    g_idle_add(first_state_read, NULL);
    g_timeout_add(150, hotspot_poll, NULL);
    g_timeout_add_seconds(30, tick, NULL);
    g_timeout_add(250, check_cmd, NULL);
    gtk_main();
    unlink(pidpath);
    return 0;
}
