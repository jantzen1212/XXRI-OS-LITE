
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>
#include <X11/extensions/XInput2.h>
#include <cairo.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include "xxri-theme.h"

#define CSS_PATH   "/usr/local/share/xxri-launcher/xxri-launcher.css"
#define CSS_DARK_PATH "/usr/local/share/xxri-launcher/xxri-launcher-dark.css"
#define WATERMARK  "/usr/local/share/pixmaps/logo.png"
#define GENERIC    "/usr/local/share/pixmaps/xxri-app-generic.png"
#define PIXMAPS    "/usr/local/share/pixmaps"
#define APPDIR     "/usr/local/share/applications"
#define DOCKLAUNCH "/usr/local/bin/xxri-dock-launch"
#define DOCKPIN    "/usr/local/bin/xxri-dock-pin"
#define PINNED_LIST "/usr/local/share/xxri-launcher/pinned.list"
#define PIDFILE    "/tmp/xxri-launcher.pid"
#define CLOSEFILE  "/tmp/xxri-launcher.closed"
#define REOPEN_MS  600     /* see stamp_closed() */
#define MAX_PINNED   8

#define SHADOW     14      /* transparent margin the drop shadow lives in */
#define RADIUS     26
#define ICON_PX    48
#define COLS        6

typedef struct {
    char *id;        /* .desktop filename without the suffix, as in pinned.list */
    char *name;
    char *lname;     /* lower-cased, for search */
    char *lextra;    /* lower-cased GenericName + Comment, also searched */
    char *exec;      /* Exec= without %-field codes */
    char *icon;      /* icon file path, or NULL */
    char *key;       /* xxri-dock-launch focus key (WM_CLASS instance) */
    gboolean pinned;
} App;

static GPtrArray *g_apps;
static GtkWidget *g_win, *g_flow, *g_search;
static GdkSeat   *g_seat;   /* the seat the drawer holds - see on_flow_button() */
static gboolean   g_composited = FALSE;
static cairo_surface_t *g_mark = NULL;   /* faint XXRI wordmark behind the grid */

/* same format xxri-dock-pin reads and writes: one id per line, # comments */
static GHashTable *read_pinned_set(void) {
    GHashTable *set = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    char *body = NULL;
    if (!g_file_get_contents(PINNED_LIST, &body, NULL, NULL)) return set;
    char **lines = g_strsplit(body, "\n", -1);
    for (int i = 0; lines[i]; i++) {
        char *l = g_strstrip(lines[i]);
        if (*l && *l != '#') g_hash_table_add(set, g_strdup(l));
    }
    g_strfreev(lines);
    g_free(body);
    return set;
}

static int count_pinned(void) {
    int n = 0;
    if (g_apps) for (guint i = 0; i < g_apps->len; i++)
        if (((App *)g_ptr_array_index(g_apps, i))->pinned) n++;
    return n;
}

/* ---------------------------------------------------------------- helpers */

static void css_class(GtkWidget *w, const char *c) {
    gtk_style_context_add_class(gtk_widget_get_style_context(w), c);
}

static void rrect(cairo_t *cr, double x, double y, double w, double h, double r) {
    if (r > w/2) r = w/2;
    if (r > h/2) r = h/2;
    cairo_new_sub_path(cr);
    cairo_arc(cr, x+w-r, y+r,   r, -G_PI/2, 0);
    cairo_arc(cr, x+w-r, y+h-r, r, 0, G_PI/2);
    cairo_arc(cr, x+r,   y+h-r, r, G_PI/2, G_PI);
    cairo_arc(cr, x+r,   y+r,   r, G_PI, 1.5*G_PI);
    cairo_close_path(cr);
}

/* ---- dock icon container
 * Every dock icon gets the same squircle, so apps can't bring their own corner
 * radius (the shipped icons range from 32% to 40%).  Full-bleed tiles are
 * scaled to fill it and clipped; a bare logo on transparency is centred on a
 * neutral plate instead.  Decided from the artwork, never per app. */
#define SQUIRCLE_N   4.0    /* superellipse exponent */
#define LOGO_INSET   0.62   /* share of the container a bare logo gets */

static void squircle_path(cairo_t *cr, double x, double y, double s) {
    const int STEPS = 96;
    double r = s / 2.0, cx = x + r, cy = y + r;
    for (int i = 0; i <= STEPS; i++) {
        double t  = 2.0 * G_PI * i / STEPS;
        double ct = cos(t), st = sin(t);
        double px = cx + r * copysign(pow(fabs(ct), 2.0 / SQUIRCLE_N), ct);
        double py = cy + r * copysign(pow(fabs(st), 2.0 / SQUIRCLE_N), st);
        if (i == 0) cairo_move_to(cr, px, py);
        else        cairo_line_to(cr, px, py);
    }
    cairo_close_path(cr);
}

/* a tile is opaque just inside its edge midpoints, a bare logo is not */
static gboolean icon_fills_tile(GdkPixbuf *pb) {
    if (!gdk_pixbuf_get_has_alpha(pb)) return TRUE;
    int w = gdk_pixbuf_get_width(pb), h = gdk_pixbuf_get_height(pb);
    if (w < 8 || h < 8) return FALSE;
    int rs = gdk_pixbuf_get_rowstride(pb), nc = gdk_pixbuf_get_n_channels(pb);
    const guchar *p = gdk_pixbuf_get_pixels(pb);
    int xs[4] = { w/2, w/2, (int)(w*0.06), (int)(w*0.94) };
    int ys[4] = { (int)(h*0.06), (int)(h*0.94), h/2, h/2 };
    int opaque = 0;
    for (int i = 0; i < 4; i++)
        if (p[ys[i]*rs + xs[i]*nc + 3] > 140) opaque++;
    return opaque >= 3;
}

/* Colour of the art nearest one of its corners (walk in along the diagonal).
 * A clip only removes pixels, so art with rounder corners than the container
 * would keep them; the container is filled with these corner colours first
 * and the art painted on top, so the gap continues the icon's gradient. */
static void sample_corner(GdkPixbuf *pb, int cx, int cy, double *r, double *g, double *b) {
    int w = gdk_pixbuf_get_width(pb), h = gdk_pixbuf_get_height(pb);
    int rs = gdk_pixbuf_get_rowstride(pb), nc = gdk_pixbuf_get_n_channels(pb);
    const guchar *p = gdk_pixbuf_get_pixels(pb);
    int dx = cx < w / 2 ? 1 : -1, dy = cy < h / 2 ? 1 : -1;
    int steps = MIN(w, h) * 2 / 5;
    /* skip the antialiased rim, it leaves a pale fringe */
    int i0 = MAX(3, MIN(w, h) / 10);
    *r = *g = *b = 1.0;
    for (int i = i0; i < steps; i++) {
        int px = CLAMP(cx + dx * i, 0, w - 1), py = CLAMP(cy + dy * i, 0, h - 1);
        const guchar *q = p + py * rs + px * nc;
        if (nc < 4 || q[3] > 200) {
            *r = q[0] / 255.0; *g = q[1] / 255.0; *b = q[2] / 255.0;
            return;
        }
    }
}

/* pb may be NULL: a missing icon still gets an empty container */
static void draw_icon_in_container(cairo_t *cr, GdkPixbuf *pb,
                                   double x, double y, double s) {
    cairo_save(cr);
    squircle_path(cr, x, y, s);
    cairo_clip(cr);
    if (pb && icon_fills_tile(pb)) {
        int iw = gdk_pixbuf_get_width(pb), ih = gdk_pixbuf_get_height(pb);
        double c[4][3];
        sample_corner(pb, 0,      0,      &c[0][0], &c[0][1], &c[0][2]);
        sample_corner(pb, iw - 1, 0,      &c[1][0], &c[1][1], &c[1][2]);
        sample_corner(pb, iw - 1, ih - 1, &c[2][0], &c[2][1], &c[2][2]);
        sample_corner(pb, 0,      ih - 1, &c[3][0], &c[3][1], &c[3][2]);
        cairo_pattern_t *m = cairo_pattern_create_mesh();
        cairo_mesh_pattern_begin_patch(m);
        cairo_mesh_pattern_move_to(m, x,     y);
        cairo_mesh_pattern_line_to(m, x + s, y);
        cairo_mesh_pattern_line_to(m, x + s, y + s);
        cairo_mesh_pattern_line_to(m, x,     y + s);
        for (int i = 0; i < 4; i++)
            cairo_mesh_pattern_set_corner_color_rgb(m, i, c[i][0], c[i][1], c[i][2]);
        cairo_mesh_pattern_end_patch(m);
        cairo_set_source(cr, m);
        cairo_paint(cr);
        cairo_pattern_destroy(m);

        double sc = MAX(s / iw, s / ih);
        cairo_translate(cr, x + (s - iw*sc)/2.0, y + (s - ih*sc)/2.0);
        cairo_scale(cr, sc, sc);
        gdk_cairo_set_source_pixbuf(cr, pb, 0, 0);
        cairo_paint(cr);
    } else {
        cairo_pattern_t *pat = cairo_pattern_create_linear(x, y, x, y + s);
        if (xxri_theme_dark()) {            /* #3C3D47 -> #2A2B33 */
            cairo_pattern_add_color_stop_rgb(pat, 0, 0.235, 0.239, 0.278);
            cairo_pattern_add_color_stop_rgb(pat, 1, 0.165, 0.169, 0.200);
        } else {
            cairo_pattern_add_color_stop_rgb(pat, 0, 1.000, 1.000, 1.000);
            cairo_pattern_add_color_stop_rgb(pat, 1, 0.925, 0.925, 0.937);
        }
        cairo_set_source(cr, pat);
        cairo_paint(cr);
        cairo_pattern_destroy(pat);
        if (pb) {
            double iw = gdk_pixbuf_get_width(pb), ih = gdk_pixbuf_get_height(pb);
            double sc = MIN(s * LOGO_INSET / iw, s * LOGO_INSET / ih);
            cairo_translate(cr, x + (s - iw*sc)/2.0, y + (s - ih*sc)/2.0);
            cairo_scale(cr, sc, sc);
            gdk_cairo_set_source_pixbuf(cr, pb, 0, 0);
            cairo_paint(cr);
        }
    }
    cairo_restore(cr);
}

/* PNG goes through cairo, not gdk-pixbuf: at boot the dock icons are framed
 * before xxri-hw-init writes gdk-pixbuf's loaders.cache, and without it every
 * icon came out blank (and got cached that way). */
static GdkPixbuf *icon_pixbuf(const char *src, int size) {
    cairo_surface_t *s = cairo_image_surface_create_from_png(src);
    if (cairo_surface_status(s) == CAIRO_STATUS_SUCCESS) {
        int w = cairo_image_surface_get_width(s), h = cairo_image_surface_get_height(s);
        GdkPixbuf *full = (w > 0 && h > 0) ? gdk_pixbuf_get_from_surface(s, 0, 0, w, h) : NULL;
        cairo_surface_destroy(s);
        if (full) {
            if (w <= size && h <= size) return full;
            double k = MIN((double)size / w, (double)size / h);
            GdkPixbuf *sc = gdk_pixbuf_scale_simple(full, MAX(1, (int)(w * k + 0.5)),
                                                    MAX(1, (int)(h * k + 0.5)), GDK_INTERP_BILINEAR);
            g_object_unref(full);
            return sc;
        }
    } else {
        cairo_surface_destroy(s);
    }
    return gdk_pixbuf_new_from_file_at_scale(src, size, size, TRUE, NULL);
}

/* --frame-icon: wbar only loads PNG paths, so xxri-dock-pin pre-renders the
 * pinned icons through the container. Runs at boot, no display needed. */
static int run_frame_icon(const char *src, const char *dst, int size) {
    GdkPixbuf *pb = icon_pixbuf(src, size);
    /* fail instead of writing an empty container, xxri-dock-pin would cache it */
    if (!pb) return 1;
    cairo_surface_t *sf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, size, size);
    cairo_t *cr = cairo_create(sf);
    draw_icon_in_container(cr, pb, 0, 0, size);
    cairo_destroy(cr);
    cairo_status_t st = cairo_surface_write_to_png(sf, dst);
    cairo_surface_destroy(sf);
    g_object_unref(pb);
    return st == CAIRO_STATUS_SUCCESS ? 0 : 1;
}

/* ---- dock separator
 * The rule between pinned and running icons is drawn into the dock background.
 * wbar lays every entry out as a full -isize square, so a separator icon cost
 * a whole slot; the background is stretched over the bar, so a rule drawn at
 * the right fraction lands in the gap after icon k.
 *
 * The background covers the icon row plus wbar's zoom headroom on each side,
 * not just the icons. Measured off the running dock at -isize 40 -idist 7
 * (pill rim at canvas x=4/635 -> screen 309/712 on 8 icons): 20.4px headroom a
 * side, 48.5px tall. Both scale with -isize for the 32/48 dock sizes. */
#define DOCK_IDIST    7.0   /* -idist, from dot.wbar */
#define DOCK_CAL     40.0   /* -isize the numbers below were measured at */
#define DOCK_BARPAD  20.4
#define DOCK_BARH    48.5
#define SEP_W         2.0   /* screen px */
#define SEP_H        24.0
/* the squircle edge starts later than its bounding box, so the math centre
 * looks ~2px off - optical correction only */
#define SEP_X_NUDGE   2.5   /* positive = toward the running icon */
/* solid: anything lighter disappears on the white pill */
#define SEP_ALPHA     1.0

static int run_dock_bg(const char *src, const char *dst, int k, int n, int isize) {
    if (k < 0 || n <= 0 || k > n) return 1;
    if (isize < 16 || isize > 256) isize = (int)DOCK_CAL;
    const double DOCK_ICON = isize, DOCK_PITCH = isize + DOCK_IDIST;
    const double barpad = DOCK_BARPAD * isize / DOCK_CAL, barh = DOCK_BARH * isize / DOCK_CAL;
    cairo_surface_t *bg = cairo_image_surface_create_from_png(src);
    if (!bg || cairo_surface_status(bg) != CAIRO_STATUS_SUCCESS) return 1;
    double cw = cairo_image_surface_get_width(bg);
    double ch = cairo_image_surface_get_height(bg);

    /* screen px: the stretched bar, and the middle of the gap after icon k */
    double barw = n * DOCK_PITCH - (DOCK_PITCH - DOCK_ICON) + 2 * barpad;
    double sepx = barpad + k * DOCK_PITCH - (DOCK_PITCH - DOCK_ICON) / 2.0
                + SEP_X_NUDGE;

    /* back to image px, so the rule has the right size after stretching */
    double x = sepx * cw / barw;
    double w = SEP_W * cw / barw;
    double h = SEP_H * ch / barh;

    cairo_t *cr = cairo_create(bg);
    cairo_set_source_rgba(cr, 1, 1, 1, SEP_ALPHA);
    cairo_rectangle(cr, x - w / 2.0, (ch - h) / 2.0, w, h);
    cairo_fill(cr);
    cairo_destroy(cr);

    cairo_status_t st = cairo_surface_write_to_png(bg, dst);
    cairo_surface_destroy(bg);
    return st == CAIRO_STATUS_SUCCESS ? 0 : 1;
}

/* we never pass documents, so drop %U %f ... instead of passing them literally */
static char *strip_field_codes(const char *exec) {
    GString *s = g_string_new("");
    for (const char *p = exec; *p; p++) {
        if (p[0] == '%' && p[1]) {
            if (p[1] == '%') { g_string_append_c(s, '%'); p++; continue; }
            if (strchr("UufFiIcCkKdDnNvm", p[1])) { p++; continue; }
        }
        g_string_append_c(s, *p);
    }
    char *out = g_strstrip(g_string_free(s, FALSE));
    return g_strdup(out);
}

/* Key xxri-dock-launch matches against WM_CLASS, so an open app is raised
 * instead of started twice. "+" = always start a new one: terminals, and Store
 * apps run through `xxri-app launch` (their class isn't xxri-app's).
 * X-XXRI-DockKey overrides it verbatim for apps whose WM_CLASS isn't their
 * binary name (the FLTK editor is "FLTK"). Keep in sync with dock_key_for()
 * in xxri-dock-pin. */
static char *focus_key_for(const char *exec, const char *override) {
    if (override && *override) return g_strdup(override);
    char **argv = NULL;
    if (!g_shell_parse_argv(exec, NULL, &argv, NULL) || !argv || !argv[0]) {
        if (argv) g_strfreev(argv);
        return g_strdup("+");
    }
    char *base = g_path_get_basename(argv[0]);
    char *key;
    if (!strcmp(base, "xxri-app") || !strcmp(base, "aterm") || !strcmp(base, "xterm"))
        key = g_strconcat("+", base, NULL);
    else
        key = g_strdup(base);
    g_free(base);
    g_strfreev(argv);
    return key;
}

/* X-FullPathIcon (XXRI's own .desktop key) first: the icon theme lookup is
 * slow on old hardware */
static char *resolve_icon(const char *full, const char *icon) {
    if (full && *full && g_file_test(full, G_FILE_TEST_EXISTS))
        return g_strdup(full);
    if (icon && *icon) {
        if (g_path_is_absolute(icon) && g_file_test(icon, G_FILE_TEST_EXISTS))
            return g_strdup(icon);
        static const char *ext[] = { "png", "svg", "xpm", NULL };
        for (int i = 0; ext[i]; i++) {
            char *p = g_strdup_printf("%s/%s.%s", PIXMAPS, icon, ext[i]);
            if (g_file_test(p, G_FILE_TEST_EXISTS)) return p;
            g_free(p);
        }
        GtkIconTheme *th = gtk_icon_theme_get_default();
        GtkIconInfo *ii = gtk_icon_theme_lookup_icon(th, icon, ICON_PX,
                                                     GTK_ICON_LOOKUP_FORCE_SIZE);
        if (ii) {
            const char *f = gtk_icon_info_get_filename(ii);
            char *p = f ? g_strdup(f) : NULL;
            g_object_unref(ii);
            if (p) return p;
        }
    }
    if (g_file_test(GENERIC, G_FILE_TEST_EXISTS)) return g_strdup(GENERIC);
    return NULL;
}

/* ------------------------------------------------------- .desktop scanning */

static char *kv(const char *body, const char *key) {
    char *needle = g_strdup_printf("\n%s=", key);
    const char *p = NULL;
    if (g_str_has_prefix(body, needle + 1)) p = body + strlen(key) + 1;
    else { const char *h = strstr(body, needle); if (h) p = h + strlen(needle); }
    g_free(needle);
    if (!p) return NULL;
    const char *e = strchr(p, '\n');
    return g_strndup(p, e ? (gsize)(e - p) : strlen(p));
}

static gboolean truthy(char *v) {
    gboolean r = v && (!g_ascii_strcasecmp(v, "true") || !strcmp(v, "1"));
    g_free(v);
    return r;
}

static int by_name(gconstpointer a, gconstpointer b) {
    const App *x = *(App * const *)a, *y = *(App * const *)b;
    return g_utf8_collate(x->name, y->name);
}

static void scan_apps(void) {
    g_apps = g_ptr_array_new();
    GHashTable *pinned = read_pinned_set();
    GDir *d = g_dir_open(APPDIR, 0, NULL);
    if (!d) { g_hash_table_destroy(pinned); return; }
    const char *fname;
    while ((fname = g_dir_read_name(d))) {
        if (!g_str_has_suffix(fname, ".desktop")) continue;
        char *path = g_build_filename(APPDIR, fname, NULL);
        char *body = NULL;
        if (!g_file_get_contents(path, &body, NULL, NULL)) { g_free(path); continue; }

        /* [Desktop Entry] only, so action groups' Name=/Exec= don't leak in */
        char *start = strstr(body, "[Desktop Entry]");
        char *scope = g_strdup(start ? start : body);
        char *next  = strstr(scope + 1, "\n[");
        if (next) *next = '\0';

        char *type = kv(scope, "Type");
        char *name = kv(scope, "Name");
        char *exec = kv(scope, "Exec");
        gboolean skip = truthy(kv(scope, "NoDisplay")) || truthy(kv(scope, "Hidden"));
        /* OnlyShowIn is ignored on purpose: Tiny Core puts OnlyShowIn=Old; on
         * aterm.desktop, which would drop the Terminal */
        if (!skip && exec && strstr(exec, "xxri-launcher")) skip = TRUE;
        if (!skip && name && *name && exec && *exec &&
            (!type || !strcmp(type, "Application"))) {
            App *a = g_new0(App, 1);
            a->id    = g_strndup(fname, strlen(fname) - strlen(".desktop"));
            a->name  = g_strdup(name);
            a->lname = g_utf8_strdown(name, -1);
            char *gen = kv(scope, "GenericName"), *com = kv(scope, "Comment");
            char *ex  = g_strdup_printf("%s %s", gen ? gen : "", com ? com : "");
            a->lextra = g_utf8_strdown(ex, -1);
            g_free(gen); g_free(com); g_free(ex);
            a->exec  = strip_field_codes(exec);
            char *override = kv(scope, "X-XXRI-DockKey");
            a->key   = focus_key_for(a->exec, override);
            g_free(override);
            char *full = kv(scope, "X-FullPathIcon"), *ic = kv(scope, "Icon");
            a->icon  = resolve_icon(full, ic);
            g_free(full); g_free(ic);
            a->pinned = g_hash_table_contains(pinned, a->id);
            g_ptr_array_add(g_apps, a);
        }
        g_free(type); g_free(name); g_free(exec);
        g_free(scope); g_free(body); g_free(path);
    }
    g_dir_close(d);
    g_hash_table_destroy(pinned);
    g_ptr_array_sort(g_apps, by_name);
}

/* -------------------------------------------------------------- launching */

static void launch(const App *a) {
    char **argv = NULL;
    GPtrArray *v = g_ptr_array_new();
    gboolean via_dock = g_file_test(DOCKLAUNCH, G_FILE_TEST_IS_EXECUTABLE);
    if (via_dock) {
        g_ptr_array_add(v, (gpointer)DOCKLAUNCH);
        g_ptr_array_add(v, a->key);
    }
    if (g_shell_parse_argv(a->exec, NULL, &argv, NULL)) {
        for (int i = 0; argv[i]; i++) g_ptr_array_add(v, argv[i]);
    } else {
        /* unbalanced quotes: let the shell have a go rather than do nothing */
        g_ptr_array_set_size(v, 0);
        g_ptr_array_add(v, (gpointer)"/bin/sh");
        g_ptr_array_add(v, (gpointer)"-c");
        g_ptr_array_add(v, a->exec);
    }
    g_ptr_array_add(v, NULL);
    g_spawn_async(NULL, (char **)v->pdata, NULL,
                  G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL |
                  G_SPAWN_STDERR_TO_DEV_NULL,
                  NULL, NULL, NULL, NULL);
    g_ptr_array_free(v, TRUE);
    if (argv) g_strfreev(argv);
}

static void on_child_activated(GtkFlowBox *fb, GtkFlowBoxChild *child, gpointer d) {
    (void)fb; (void)d;
    const App *a = g_object_get_data(G_OBJECT(child), "app");
    if (a) launch(a);
    gtk_main_quit();
}

/* ------------------------------------------------------ pin / unpin menu */

/* xxri-dock-pin does the work (it needs sudo for the root:staff tce.icons) */
static gboolean run_dock_pin(const char *verb, const char *id) {
    char *argv[] = { (char *)DOCKPIN, (char *)verb, (char *)id, NULL };
    gint status = -1;
    gboolean ok = g_spawn_sync(NULL, argv, NULL, G_SPAWN_SEARCH_PATH,
                               NULL, NULL, NULL, NULL, &status, NULL);
    return ok && g_spawn_check_exit_status(status, NULL);
}

static void on_pin_activate(GtkMenuItem *item, gpointer d) {
    (void)item;
    App *a = d;
    if (run_dock_pin("pin", a->id)) a->pinned = TRUE;
}

static void on_unpin_activate(GtkMenuItem *item, gpointer d) {
    (void)item;
    App *a = d;
    if (run_dock_pin("unpin", a->id)) a->pinned = FALSE;
}

/* A GtkMenu needs its own grab: under the drawer's seat grab it renders but
 * is dead (no "activate", clicks go underneath). So the seat is released while
 * the menu is up and taken back on "deactivate" (selection or dismissal). */
static void seat_regrab(GtkMenuShell *m, gpointer d) {
    (void)m; (void)d;
    GdkWindow *gw = gtk_widget_get_window(g_win);
    if (g_seat && gw)
        gdk_seat_grab(g_seat, gw, GDK_SEAT_CAPABILITY_ALL, TRUE,
                      NULL, NULL, NULL, NULL);
}

/* right-click a tile: Pin / Unpin (pin is greyed out at MAX_PINNED) */
static gboolean on_flow_button(GtkWidget *w, GdkEventButton *e, gpointer d) {
    (void)d;
    if (e->type != GDK_BUTTON_PRESS || e->button != GDK_BUTTON_SECONDARY) return FALSE;
    GtkFlowBoxChild *child = gtk_flow_box_get_child_at_pos(GTK_FLOW_BOX(w),
                                                           (int)e->x, (int)e->y);
    if (!child) return FALSE;
    App *a = g_object_get_data(G_OBJECT(child), "app");
    if (!a) return FALSE;

    GtkWidget *menu = gtk_menu_new();
    GtkWidget *item;
    if (a->pinned) {
        item = gtk_menu_item_new_with_label("Unpin from Dock");
        g_signal_connect(item, "activate", G_CALLBACK(on_unpin_activate), a);
    } else {
        item = gtk_menu_item_new_with_label("Pin to Dock");
        if (count_pinned() >= MAX_PINNED) {
            gtk_widget_set_sensitive(item, FALSE);
            gtk_widget_set_tooltip_text(item, "Dock is full (8/8) - unpin an app first");
        } else {
            g_signal_connect(item, "activate", G_CALLBACK(on_pin_activate), a);
        }
    }
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    g_signal_connect(menu, "deactivate", G_CALLBACK(seat_regrab), NULL);
    gtk_widget_show_all(menu);
    if (g_seat) gdk_seat_ungrab(g_seat);
    gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent *)e);
    return TRUE;
}

/* Enter runs the first match */
static void on_search_activate(GtkEntry *e, gpointer d) {
    (void)e; (void)d;
    GList *kids = gtk_container_get_children(GTK_CONTAINER(g_flow));
    for (GList *l = kids; l; l = l->next) {
        GtkWidget *c = l->data;
        if (gtk_widget_get_child_visible(c) && gtk_widget_get_visible(c)) {
            const App *a = g_object_get_data(G_OBJECT(c), "app");
            if (a) { launch(a); gtk_main_quit(); break; }
        }
    }
    g_list_free(kids);
}

/* ------------------------------------------------------------- the filter */

static gboolean filter_func(GtkFlowBoxChild *child, gpointer d) {
    (void)d;
    /* GTK runs the filter once while the grid is built, before g_search exists */
    if (!g_search) return TRUE;
    const char *q = gtk_entry_get_text(GTK_ENTRY(g_search));
    if (!q || !*q) return TRUE;
    char *lq = g_utf8_strdown(q, -1);
    const App *a = g_object_get_data(G_OBJECT(child), "app");
    gboolean hit = a && (strstr(a->lname, lq) || (a->lextra && strstr(a->lextra, lq)));
    g_free(lq);
    return hit;
}

static void on_search_changed(GtkEditable *e, gpointer d) {
    (void)e; (void)d;
    gtk_flow_box_invalidate_filter(GTK_FLOW_BOX(g_flow));
}

/* --------------------------------------------------------------- painting */

static gboolean on_draw(GtkWidget *w, cairo_t *cr, gpointer d) {
    (void)d;
    int W = gtk_widget_get_allocated_width(w);
    int H = gtk_widget_get_allocated_height(w);
    int pw = W - 2*SHADOW, ph = H - 2*SHADOW;

    cairo_save(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    /* no compositor: an ARGB window paints black, so fill the margin (the
     * shape mask cuts it away anyway) and go without the shadow */
    gboolean dk = xxri_theme_dark();
    if (g_composited) cairo_set_source_rgba(cr, 0, 0, 0, 0);
    else if (dk)      cairo_set_source_rgb(cr, 0.141, 0.129, 0.141);
    else              cairo_set_source_rgb(cr, 0.93, 0.93, 0.94);
    cairo_paint(cr);
    cairo_restore(cr);

    if (g_composited) {
        /* 2px steps, software rendering */
        for (int i = SHADOW; i >= 1; i -= 2) {
            double t = (double)i / SHADOW;
            cairo_set_source_rgba(cr, 0, 0, 0, 0.13 * (1.0-t) * (1.0-t));
            rrect(cr, SHADOW - i, SHADOW - i + 2.0,
                  pw + 2*i, ph + 2*i, RADIUS + i);
            cairo_fill(cr);
        }
    }

    /* the glass panel */
    cairo_pattern_t *pat = cairo_pattern_create_linear(SHADOW, SHADOW, SHADOW, SHADOW+ph);
    if (dk) {           /* Settings' dark surfaces, #2A2B33 -> #242124 */
        cairo_pattern_add_color_stop_rgba(pat, 0, 0.165, 0.169, 0.200, 0.93);
        cairo_pattern_add_color_stop_rgba(pat, 1, 0.141, 0.129, 0.141, 0.90);
    } else {
        cairo_pattern_add_color_stop_rgba(pat, 0, 1.000, 1.000, 1.000, 0.93);
        cairo_pattern_add_color_stop_rgba(pat, 1, 0.945, 0.945, 0.955, 0.90);
    }
    rrect(cr, SHADOW, SHADOW, pw, ph, RADIUS);
    cairo_set_source(cr, pat);
    cairo_fill(cr);
    cairo_pattern_destroy(pat);

    /* faint wordmark behind the grid, like the mockup */
    if (g_mark) {
        int mw = cairo_image_surface_get_width(g_mark);
        int mh = cairo_image_surface_get_height(g_mark);
        if (mw > 0 && mh > 0) {
            double sc = (pw * 0.34) / mw;
            cairo_save(cr);
            rrect(cr, SHADOW, SHADOW, pw, ph, RADIUS);
            cairo_clip(cr);
            cairo_translate(cr, SHADOW + (pw - mw*sc)/2.0, SHADOW + (ph - mh*sc)/2.0 - ph*0.06);
            cairo_scale(cr, sc, sc);
            cairo_set_source_surface(cr, g_mark, 0, 0);
            cairo_paint_with_alpha(cr, 0.07);
            cairo_restore(cr);
        }
    }

    /* rim, same hairline as the dock and Control Center */
    cairo_set_line_width(cr, 1.0);
    cairo_set_source_rgba(cr, 1, 1, 1, dk ? 0.12 : 0.70);
    rrect(cr, SHADOW + 0.5, SHADOW + 0.5, pw - 1, ph - 1, RADIUS);
    cairo_stroke(cr);
    return FALSE;
}

/* no compositor: cut the square corners off with the Shape extension */
static void apply_shape(GtkWidget *w) {
    if (g_composited) return;
    GdkWindow *gw = gtk_widget_get_window(w);
    if (!gw) return;
    int W = gtk_widget_get_allocated_width(w);
    int H = gtk_widget_get_allocated_height(w);
    if (W <= 0 || H <= 0) return;
    cairo_surface_t *m = cairo_image_surface_create(CAIRO_FORMAT_A1, W, H);
    cairo_t *cr = cairo_create(m);
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR); cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, 1, 1, 1, 1);
    rrect(cr, SHADOW, SHADOW, W - 2*SHADOW, H - 2*SHADOW, RADIUS);
    cairo_fill(cr);
    cairo_destroy(cr);
    cairo_region_t *rgn = gdk_cairo_region_create_from_surface(m);
    gtk_widget_shape_combine_region(w, rgn);
    cairo_region_destroy(rgn);
    cairo_surface_destroy(m);
}

static void on_size_allocate(GtkWidget *w, GdkRectangle *a, gpointer d) {
    (void)a; (void)d;
    apply_shape(w);
}

/* ---------------------------------------------------------------- dismiss */

/* The open drawer holds the keyboard, so the closing Super tap lands here, not
 * in super_io(). Close on the release of a Super pressed alone. */
static gboolean g_super_tap = FALSE;

static gboolean on_key(GtkWidget *w, GdkEventKey *e, gpointer d) {
    (void)w; (void)d;
    if (e->keyval == GDK_KEY_Escape) { gtk_main_quit(); return TRUE; }
    g_super_tap = (e->keyval == GDK_KEY_Super_L || e->keyval == GDK_KEY_Super_R);
    return g_super_tap;
}

static gboolean on_key_release(GtkWidget *w, GdkEventKey *e, gpointer d) {
    (void)w; (void)d;
    if ((e->keyval == GDK_KEY_Super_L || e->keyval == GDK_KEY_Super_R) && g_super_tap) {
        gtk_main_quit();
        return TRUE;
    }
    return FALSE;
}

/* One press on the All Apps button both dismisses the drawer (outside click)
 * and makes wbar start a new one. Record when we closed; a copy started within
 * REOPEN_MS exits, so the button toggles. */
static void stamp_closed(void) {
    char b[32];
    snprintf(b, sizeof b, "%" G_GINT64_FORMAT, g_get_monotonic_time() / 1000);
    g_file_set_contents(CLOSEFILE, b, -1, NULL);
}

/* with the seat grabbed every press lands here; outside the panel closes */
static gboolean on_button(GtkWidget *w, GdkEventButton *e, gpointer d) {
    (void)d;
    int W = gtk_widget_get_allocated_width(w);
    int H = gtk_widget_get_allocated_height(w);
    if (e->x < SHADOW || e->y < SHADOW || e->x > W - SHADOW || e->y > H - SHADOW) {
        stamp_closed();
        gtk_main_quit();
        return TRUE;
    }
    return FALSE;
}

/* a toggle from elsewhere arrives as SIGTERM; leave the main loop so the pid
 * file gets removed */
static gboolean quit_idle(gpointer d) { (void)d; gtk_main_quit(); return FALSE; }
static void on_term(int s) { (void)s; g_idle_add(quit_idle, NULL); }

/* ---- dock button
 * `--button`: the All Apps key left of the dock. Its own window, since wbar
 * can't detach a slot; painted with the dock's glass, and positioned from
 * wbar's real window like xxri-wctl's indicators. */
#define BTN_GAP     10
#define DOT_WBAR    "/usr/local/share/wbar/dot.wbar"
#define TCE_ICONS   "/usr/local/tce.icons"   /* the live dock xxri-dock-pin writes */
#define ICON_FALLBACK 40    /* dot.wbar's -isize */

static GtkWidget *g_btn;
static int      g_btn_side  = ICON_FALLBACK;
static gboolean g_btn_hover = FALSE;
static gboolean g_btn_down  = FALSE;

/* size from -isize, not the wbar window: that one includes zoom headroom */
static int isize_of(const char *path) {
    char *body = NULL;
    int size = -1;
    if (g_file_get_contents(path, &body, NULL, NULL)) {
        const char *p = strstr(body, "-isize");
        if (p) {
            size = atoi(p + strlen("-isize"));
            if (size < 16 || size > 256) size = -1;
        }
        g_free(body);
    }
    return size;
}
/* tce.icons first, Settings can resize the dock mid-session; re-read only
 * when it changes */
static int dock_icon_size(void) {
    static int cached = -1;
    static time_t seen = 0;
    struct stat st;
    time_t m = stat(TCE_ICONS, &st) == 0 ? st.st_mtime : 0;
    if (cached > 0 && m == seen) return cached;
    seen = m;
    cached = isize_of(TCE_ICONS);
    if (cached <= 0) cached = isize_of(DOT_WBAR);
    if (cached <= 0) cached = ICON_FALLBACK;
    return cached;
}

static gboolean wm_instance(Display *dpy, Window w, char *buf, size_t n) {
    XClassHint ch;
    buf[0] = '\0';
    if (!XGetClassHint(dpy, w, &ch)) return FALSE;
    if (ch.res_name) g_strlcpy(buf, ch.res_name, n);
    if (ch.res_name)  XFree(ch.res_name);
    if (ch.res_class) XFree(ch.res_class);
    return buf[0] != '\0';
}

static gboolean dock_geometry(int *dx, int *dy, int *dw, int *dh) {
    Display *dpy = gdk_x11_get_default_xdisplay();
    Window root = DefaultRootWindow(dpy), r, parent, *kids = NULL;
    unsigned int nk = 0, i;
    gboolean found = FALSE;
    if (!XQueryTree(dpy, root, &r, &parent, &kids, &nk)) return FALSE;
    for (i = 0; i < nk && !found; i++) {
        char inst[128];
        if (!wm_instance(dpy, kids[i], inst, sizeof inst)) continue;
        if (g_ascii_strcasecmp(inst, "wbar") != 0) continue;
        XWindowAttributes a;
        if (!XGetWindowAttributes(dpy, kids[i], &a)) continue;
        if (a.map_state != IsViewable || a.width < 40) continue;
        Window child; int rx, ry;
        XTranslateCoordinates(dpy, kids[i], root, 0, 0, &rx, &ry, &child);
        *dx = rx; *dy = ry; *dw = a.width; *dh = a.height;
        found = TRUE;
    }
    if (kids) XFree(kids);
    return found;
}

static gboolean btn_draw(GtkWidget *w, cairo_t *cr, gpointer d) {
    (void)d;
    double S = gtk_widget_get_allocated_width(w);
    double H = gtk_widget_get_allocated_height(w);
    double rad = H * 0.39;            /* the dock's corner radius */

    cairo_save(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, 0, 0, 0, 0);
    cairo_paint(cr);
    cairo_restore(cr);

    double lift = g_btn_hover ? 0.06 : 0.0;
    if (g_btn_down) lift = -0.04;
    cairo_pattern_t *pat = cairo_pattern_create_linear(0, 0, 0, H);
    gboolean dk = xxri_theme_dark();
    if (dk) {           /* the dark dock's pill (wbar-bg-dark.png) */
        cairo_pattern_add_color_stop_rgba(pat, 0, 0.189, 0.190, 0.219, 0.80 + lift);
        cairo_pattern_add_color_stop_rgba(pat, 1, 0.142, 0.131, 0.143, 0.71 + lift);
    } else {
        cairo_pattern_add_color_stop_rgba(pat, 0, 0.995, 0.995, 0.998, 0.746 + lift);
        cairo_pattern_add_color_stop_rgba(pat, 1, 0.955, 0.955, 0.965, 0.690 + lift);
    }
    rrect(cr, 0, 0, S, H, rad);
    cairo_set_source(cr, pat);
    cairo_fill(cr);
    cairo_pattern_destroy(pat);

    cairo_set_line_width(cr, 1.0);
    cairo_set_source_rgba(cr, 1, 1, 1, dk ? 0.14 : 0.55);
    rrect(cr, 0.5, 0.5, S - 1, H - 1, rad);
    cairo_stroke(cr);

    /* nine dots, same geometry and gradient as xxri-launcher.svg */
    double cx = S / 2.0, cy = H / 2.0;
    double step = H * 0.1875, r = H * 0.067;
    cairo_pattern_t *dp = cairo_pattern_create_linear(cx - step, cy - step,
                                                      cx + step, cy + step);
    cairo_pattern_add_color_stop_rgb(dp, 0.00, 0.514, 0.443, 0.969);   /* #8371F7 */
    cairo_pattern_add_color_stop_rgb(dp, 0.55, 0.431, 0.388, 0.910);   /* #6E63E8 */
    cairo_pattern_add_color_stop_rgb(dp, 1.00, 0.180, 0.486, 0.965);   /* #2E7CF6 */
    cairo_set_source(cr, dp);
    for (int iy = -1; iy <= 1; iy++)
        for (int ix = -1; ix <= 1; ix++) {
            cairo_new_sub_path(cr);
            cairo_arc(cr, cx + ix*step, cy + iy*step, r, 0, 2*G_PI);
        }
    cairo_fill(cr);
    cairo_pattern_destroy(dp);
    return TRUE;
}

/* see apply_shape(); redone on resize, the size follows the dock */
static void btn_shape(void) {
    if (g_composited) return;
    GdkWindow *gw = gtk_widget_get_window(g_btn);
    if (!gw) return;
    int W = gtk_widget_get_allocated_width(g_btn);
    int H = gtk_widget_get_allocated_height(g_btn);
    if (W <= 0 || H <= 0) return;
    cairo_surface_t *m = cairo_image_surface_create(CAIRO_FORMAT_A1, W, H);
    cairo_t *cr = cairo_create(m);
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR); cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, 1, 1, 1, 1);
    rrect(cr, 0, 0, W, H, H * 0.39);
    cairo_fill(cr);
    cairo_destroy(cr);
    cairo_region_t *rgn = gdk_cairo_region_create_from_surface(m);
    gtk_widget_shape_combine_region(g_btn, rgn);
    cairo_region_destroy(rgn);
    cairo_surface_destroy(m);
}

static void btn_alloc(GtkWidget *w, GdkRectangle *a, gpointer d) {
    (void)w; (void)a; (void)d;
    btn_shape();
}

static gboolean btn_cross(GtkWidget *w, GdkEventCrossing *e, gpointer d) {
    (void)d;
    g_btn_hover = (e->type == GDK_ENTER_NOTIFY);
    if (!g_btn_hover) g_btn_down = FALSE;
    gtk_widget_queue_draw(w);
    return FALSE;
}

static gboolean btn_press(GtkWidget *w, GdkEventButton *e, gpointer d) {
    (void)e; (void)d;
    g_btn_down = TRUE;
    gtk_widget_queue_draw(w);
    return TRUE;
}

/* The drawer is its own process so a crash there can't take the resident
 * button down. A second copy closes the open one via the pid file (main()). */
static void toggle_drawer(void) {
    char *argv[] = { (char *)"/usr/local/bin/xxri-launcher", NULL };
    g_spawn_async(NULL, argv, NULL,
                  G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL,
                  NULL, NULL, NULL, NULL);
}

static gboolean btn_release(GtkWidget *w, GdkEventButton *e, gpointer d) {
    (void)e; (void)d;
    g_btn_down = FALSE;
    gtk_widget_queue_draw(w);
    toggle_drawer();
    return TRUE;
}

/* ---- Super key
 * A Super tap (pressed and released alone, within SUPER_TAP_MS) toggles the
 * drawer; Super+key stays a chord. Passive grab on the root window, on a
 * second X connection: GDK reads XI2 and has nowhere to put these core
 * events. */
#define SUPER_TAP_MS 700

static Display *g_kdpy;
static KeyCode  g_super_kc[8];     /* every keycode that is a Super key */
static int      g_super_n;
static gboolean g_super_armed;
static Time     g_super_at;

static int super_xerror(Display *d, XErrorEvent *e) {
    (void)d; (void)e;          /* BadAccess: someone else holds the key */
    return 0;
}

static gboolean is_super(unsigned kc) {
    for (int i = 0; i < g_super_n; i++) if (g_super_kc[i] == kc) return TRUE;
    return FALSE;
}

/* Grabs are per keycode and the core keymap is just the last-used keyboard's:
 * Xfbdev starts with Super=133 and switches to 115 at the first key press.
 * So grab every keycode any keyboard device maps to Super, and redo it on
 * MappingNotify. */
static void super_collect(XkbDescPtr d) {
    if (!d || !d->map) return;
    for (int kc = d->min_key_code; kc <= d->max_key_code; kc++) {
        if (!XkbKeyNumSyms(d, kc)) continue;
        KeySym ks = XkbKeySymEntry(d, kc, 0, 0);
        if ((ks == XK_Super_L || ks == XK_Super_R) && !is_super(kc) &&
            g_super_n < (int)G_N_ELEMENTS(g_super_kc))
            g_super_kc[g_super_n++] = (KeyCode)kc;
    }
}

static void super_grab(void) {
    static const unsigned mods[] = { 0, LockMask, Mod2Mask, LockMask | Mod2Mask };
    Window root = DefaultRootWindow(g_kdpy);
    /* only around the grabs: the handler is process-wide and GDK has its own */
    XErrorHandler old = XSetErrorHandler(super_xerror);
    for (int i = 0; i < g_super_n; i++)
        XUngrabKey(g_kdpy, g_super_kc[i], AnyModifier, root);
    g_super_n = 0;

    XkbDescPtr d = XkbGetMap(g_kdpy, XkbKeySymsMask, XkbUseCoreKbd);
    super_collect(d);
    if (d) XkbFreeKeyboard(d, 0, True);
    int n = 0;
    XIDeviceInfo *dev = XIQueryDevice(g_kdpy, XIAllDevices, &n);
    for (int i = 0; dev && i < n; i++) {
        if (dev[i].use != XISlaveKeyboard && dev[i].use != XIMasterKeyboard) continue;
        d = XkbGetMap(g_kdpy, XkbKeySymsMask, dev[i].deviceid);
        super_collect(d);
        if (d) XkbFreeKeyboard(d, 0, True);
    }
    if (dev) XIFreeDeviceInfo(dev);

    /* plain, or with Caps/Num Lock only */
    for (int i = 0; i < g_super_n; i++)
        for (unsigned m = 0; m < G_N_ELEMENTS(mods); m++)
            XGrabKey(g_kdpy, g_super_kc[i], mods[m], root, False,
                     GrabModeAsync, GrabModeAsync);
    XSync(g_kdpy, False);
    XSetErrorHandler(old);
}

static gboolean super_io(GIOChannel *ch, GIOCondition c, gpointer d) {
    (void)ch; (void)c; (void)d;
    while (XPending(g_kdpy)) {
        XEvent ev;
        XNextEvent(g_kdpy, &ev);
        if (ev.type == MappingNotify) {
            if (ev.xmapping.request != MappingPointer) {
                XRefreshKeyboardMapping(&ev.xmapping);
                super_grab();
            }
            continue;
        }
        if (ev.type != KeyPress && ev.type != KeyRelease) continue;
        gboolean super = is_super(ev.xkey.keycode);
        if (ev.type == KeyPress) {
            g_super_armed = super;             /* any other key: a chord */
            if (super) g_super_at = ev.xkey.time;
        } else if (super) {
            /* no dock on screen (the installer has the session): no drawer */
            if (g_super_armed && ev.xkey.time - g_super_at < SUPER_TAP_MS &&
                gtk_widget_get_visible(g_btn))
                toggle_drawer();
            g_super_armed = FALSE;
        }
    }
    return TRUE;
}

static void super_init(void) {
    g_kdpy = XOpenDisplay(NULL);
    if (!g_kdpy) return;
    super_grab();
    GIOChannel *ch = g_io_channel_unix_new(ConnectionNumber(g_kdpy));
    g_io_add_watch(ch, G_IO_IN, super_io, NULL);
    g_io_channel_unref(ch);
}

/* polled: wbar gets restarted (and resized) whenever the dock is rebuilt */
static gboolean btn_track(gpointer d) {
    (void)d;
    int dx, dy, dw, dh;
    if (!dock_geometry(&dx, &dy, &dw, &dh)) {
        gtk_widget_hide(g_btn);
        return TRUE;
    }
    int side = dock_icon_size();
    if (side != g_btn_side) {
        g_btn_side = side;
        gtk_widget_set_size_request(g_btn, g_btn_side, g_btn_side);
    }
    /* centred on the wbar window, which is taller than the icon row */
    int x = dx - BTN_GAP - g_btn_side;
    int y = dy + (dh - g_btn_side) / 2;
    if (x < 2) x = 2;
    int cx, cy;
    gtk_window_get_position(GTK_WINDOW(g_btn), &cx, &cy);
    if (cx != x || cy != y) gtk_window_move(GTK_WINDOW(g_btn), x, y);
    if (!gtk_widget_get_visible(g_btn)) gtk_widget_show_all(g_btn);
    return TRUE;
}

static void use_rgba_visual(GtkWidget *w) {
    GdkScreen *sc = gtk_widget_get_screen(w);
    GdkVisual *rgba = gdk_screen_get_rgba_visual(sc);
    g_composited = gdk_screen_is_composited(sc) && rgba != NULL;
    if (g_composited) gtk_widget_set_visual(w, rgba);
}

/* Light/Dark switched in Settings: repaint whatever is painted here */
static void theme_changed(gboolean dark, gpointer d) {
    (void)dark; (void)d;
    if (g_btn) gtk_widget_queue_draw(g_btn);
    if (g_win) gtk_widget_queue_draw(g_win);
}

static int run_button(void) {
    g_btn = gtk_window_new(GTK_WINDOW_POPUP);
    gtk_window_set_type_hint(GTK_WINDOW(g_btn), GDK_WINDOW_TYPE_HINT_DOCK);
    gtk_window_set_keep_above(GTK_WINDOW(g_btn), TRUE);
    gtk_window_set_accept_focus(GTK_WINDOW(g_btn), FALSE);
    gtk_window_set_focus_on_map(GTK_WINDOW(g_btn), FALSE);
    gtk_window_set_resizable(GTK_WINDOW(g_btn), FALSE);
    gtk_widget_set_app_paintable(g_btn, TRUE);
    use_rgba_visual(g_btn);
    gtk_widget_set_size_request(g_btn, g_btn_side, g_btn_side);
    gtk_widget_add_events(g_btn, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
                                 GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK);
    g_signal_connect(g_btn, "draw",                 G_CALLBACK(btn_draw),    NULL);
    g_signal_connect(g_btn, "button-press-event",   G_CALLBACK(btn_press),   NULL);
    g_signal_connect(g_btn, "button-release-event", G_CALLBACK(btn_release), NULL);
    g_signal_connect(g_btn, "enter-notify-event",   G_CALLBACK(btn_cross),   NULL);
    g_signal_connect(g_btn, "leave-notify-event",   G_CALLBACK(btn_cross),   NULL);
    g_signal_connect(g_btn, "destroy",              G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(g_btn, "size-allocate",        G_CALLBACK(btn_alloc),   NULL);

    btn_track(NULL);
    g_timeout_add(1000, btn_track, NULL);
    super_init();
    gtk_main();
    return 0;
}

/* ------------------------------------------------------------------ build */

static GtkWidget *make_tile(App *a) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    css_class(box, "xxri-tile");

    GtkWidget *img = NULL;
    if (a->icon) {
        GdkPixbuf *pb = gdk_pixbuf_new_from_file_at_scale(a->icon, ICON_PX, ICON_PX,
                                                          TRUE, NULL);
        if (pb) { img = gtk_image_new_from_pixbuf(pb); g_object_unref(pb); }
    }
    if (!img) img = gtk_image_new_from_icon_name("application-x-executable",
                                                 GTK_ICON_SIZE_DIALOG);
    gtk_widget_set_size_request(img, ICON_PX, ICON_PX);
    gtk_widget_set_halign(img, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(box), img, FALSE, FALSE, 0);

    GtkWidget *lab = gtk_label_new(a->name);
    css_class(lab, "xxri-tile-label");
    gtk_label_set_justify(GTK_LABEL(lab), GTK_JUSTIFY_CENTER);
    gtk_label_set_line_wrap(GTK_LABEL(lab), TRUE);
    gtk_label_set_lines(GTK_LABEL(lab), 2);
    gtk_label_set_ellipsize(GTK_LABEL(lab), PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars(GTK_LABEL(lab), 12);
    gtk_widget_set_halign(lab, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(box), lab, FALSE, FALSE, 0);

    gtk_widget_set_tooltip_text(box, a->name);
    return box;
}

/* ---- dock right-click
 * `--dockmenu ID X Y`: "Unpin from Dock" for a pinned wbar icon. wbar has no
 * menus, so xxri-wctl (which grabs button 3 on wbar) spawns this per click. */
static char *g_dm_id;

static void on_dockmenu_unpin(GtkMenuItem *item, gpointer d) {
    (void)item; (void)d;
    run_dock_pin("unpin", g_dm_id);
    gtk_main_quit();
}
static void on_dockmenu_deactivate(GtkMenuShell *m, gpointer d) {
    (void)m; (void)d;
    gtk_main_quit();
}

static int run_dockmenu(const char *id, int x, int y) {
    g_dm_id = (char *)id;
    GtkWidget *menu = gtk_menu_new();
    GtkWidget *item = gtk_menu_item_new_with_label("Unpin from Dock");
    g_signal_connect(item, "activate", G_CALLBACK(on_dockmenu_unpin), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    g_signal_connect(menu, "deactivate", G_CALLBACK(on_dockmenu_deactivate), NULL);
    gtk_widget_show_all(menu);
    GdkRectangle rect = { x, y, 1, 1 };
    GdkDisplay *disp = gdk_display_get_default();
    GdkSeat *seat = gdk_display_get_default_seat(disp);
    GdkDevice *dev = seat ? gdk_seat_get_pointer(seat) : NULL;
    GdkScreen *sc = gdk_screen_get_default();
    GdkWindow *root_win = gdk_screen_get_root_window(sc);
    /* X Y are already root coordinates */
    if (dev) gtk_menu_popup_at_rect(GTK_MENU(menu), root_win, &rect,
                                    GDK_GRAVITY_NORTH_WEST, GDK_GRAVITY_NORTH_WEST, NULL);
    else     gtk_menu_popup_at_pointer(GTK_MENU(menu), NULL);
    gtk_main();
    return 0;
}

int main(int argc, char **argv) {
    /* --frame-icon and --dock-bg run from xxri-dock-pin, possibly with no
     * display: keep them before gtk_init */
    if (argc >= 4 && !strcmp(argv[1], "--frame-icon"))
        return run_frame_icon(argv[2], argv[3], argc >= 5 ? atoi(argv[4]) : 64);

    if (argc >= 6 && !strcmp(argv[1], "--dock-bg"))
        return run_dock_bg(argv[2], argv[3], atoi(argv[4]), atoi(argv[5]),
                           argc >= 7 ? atoi(argv[6]) : (int)DOCK_CAL);

    if (argc >= 5 && !strcmp(argv[1], "--dockmenu")) {
        if (!gtk_init_check(&argc, &argv)) return 1;
        return run_dockmenu(argv[2], atoi(argv[3]), atoi(argv[4]));
    }

    gboolean want_button = FALSE;
    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--button")) want_button = TRUE;

    if (want_button) {
        if (!gtk_init_check(&argc, &argv)) { fprintf(stderr, "no display\n"); return 1; }
        /* resident: follows Settings' Mode while it runs */
        xxri_theme_init(NULL, theme_changed, NULL);
        return run_button();
    }

    /* toggle: a drawer is already open, close it */
    {
        char *pid = NULL;
        if (g_file_get_contents(PIDFILE, &pid, NULL, NULL)) {
            pid_t p = (pid_t)atoi(pid);
            g_free(pid);
            if (p > 1 && kill(p, 0) == 0) { kill(p, SIGTERM); return 0; }
        }
    }
    /* started by the same press that just closed one (see stamp_closed) */
    {
        char *stamp = NULL;
        if (g_file_get_contents(CLOSEFILE, &stamp, NULL, NULL)) {
            gint64 t = g_ascii_strtoll(stamp, NULL, 10);
            g_free(stamp);
            if (g_get_monotonic_time() / 1000 - t < REOPEN_MS) { unlink(CLOSEFILE); return 0; }
        }
    }
    if (!gtk_init_check(&argc, &argv)) { fprintf(stderr, "no display\n"); return 1; }
    signal(SIGTERM, on_term);
    { char b[32]; snprintf(b, sizeof b, "%d", (int)getpid());
      g_file_set_contents(PIDFILE, b, -1, NULL); }

    GtkCssProvider *css = gtk_css_provider_new();
    if (g_file_test(CSS_PATH, G_FILE_TEST_EXISTS))
        gtk_css_provider_load_from_path(css, CSS_PATH, NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css);
    xxri_theme_init(CSS_DARK_PATH, theme_changed, NULL);

    if (g_file_test(WATERMARK, G_FILE_TEST_EXISTS))
        g_mark = cairo_image_surface_create_from_png(WATERMARK);
    if (g_mark && cairo_surface_status(g_mark) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(g_mark); g_mark = NULL;
    }

    scan_apps();

    g_win = gtk_window_new(GTK_WINDOW_POPUP);
    gtk_window_set_type_hint(GTK_WINDOW(g_win), GDK_WINDOW_TYPE_HINT_DIALOG);
    gtk_window_set_keep_above(GTK_WINDOW(g_win), TRUE);
    gtk_window_set_resizable(GTK_WINDOW(g_win), FALSE);
    gtk_widget_set_app_paintable(g_win, TRUE);
    use_rgba_visual(g_win);
    css_class(g_win, "xxri-launcher");
    gtk_widget_add_events(g_win, GDK_BUTTON_PRESS_MASK | GDK_KEY_PRESS_MASK |
                                 GDK_KEY_RELEASE_MASK);
    g_signal_connect(g_win, "draw",              G_CALLBACK(on_draw),   NULL);
    g_signal_connect(g_win, "key-press-event",   G_CALLBACK(on_key),    NULL);
    g_signal_connect(g_win, "key-release-event", G_CALLBACK(on_key_release), NULL);
    g_signal_connect(g_win, "button-press-event", G_CALLBACK(on_button), NULL);
    g_signal_connect(g_win, "destroy",           G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(g_win, "size-allocate",     G_CALLBACK(on_size_allocate), NULL);

    /* height follows the row count up to a cap, then the grid scrolls;
     * centred, lifted a bit clear of the dock */
    GdkScreen *sc = gdk_screen_get_default();
    int sw = gdk_screen_get_width(sc), sh = gdk_screen_get_height(sc);
    int pw = MIN(580, (int)(sw * 0.57));

    int n    = (int)g_apps->len;
    int rows = (n + COLS - 1) / COLS; if (rows < 1) rows = 1;
    int tile_h = ICON_PX + 48;              /* icon + up to two label lines + cell padding */
    int grid_h = rows * tile_h + (rows - 1) * 10;
    int ph = 26 + grid_h + 30 + 40 + 20;    /* top + grid + gap + search + bottom */
    ph = CLAMP(ph, 240, (int)(sh * 0.68));

    int W  = pw + 2*SHADOW, H = ph + 2*SHADOW;
    gtk_window_set_default_size(GTK_WINDOW(g_win), W, H);
    gtk_widget_set_size_request(g_win, W, H);
    gtk_window_move(GTK_WINDOW(g_win), (sw - W)/2, (sh - H)/2 - (int)(sh * 0.045));

    GtkWidget *col = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_margin_start (col, SHADOW + 18);
    gtk_widget_set_margin_end   (col, SHADOW + 18);
    gtk_widget_set_margin_top   (col, SHADOW + 24);
    gtk_widget_set_margin_bottom(col, SHADOW + 20);
    gtk_container_add(GTK_CONTAINER(g_win), col);

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    css_class(scroll, "xxri-scroll");
    gtk_box_pack_start(GTK_BOX(col), scroll, TRUE, TRUE, 0);

    g_flow = gtk_flow_box_new();
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(g_flow), GTK_SELECTION_NONE);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(g_flow), COLS);
    gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(g_flow), COLS);
    gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(g_flow), TRUE);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(g_flow), 10);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(g_flow), 6);
    gtk_widget_set_valign(g_flow, GTK_ALIGN_START);
    css_class(g_flow, "xxri-grid");
    gtk_container_add(GTK_CONTAINER(scroll), g_flow);

    for (guint i = 0; i < g_apps->len; i++) {
        App *a = g_ptr_array_index(g_apps, i);
        GtkWidget *tile = make_tile(a);
        gtk_flow_box_insert(GTK_FLOW_BOX(g_flow), tile, -1);
        GtkWidget *child = gtk_widget_get_parent(tile);
        g_object_set_data(G_OBJECT(child), "app", a);
        css_class(child, "xxri-cell");
    }
    gtk_flow_box_set_filter_func(GTK_FLOW_BOX(g_flow), filter_func, NULL, NULL);
    g_signal_connect(g_flow, "child-activated", G_CALLBACK(on_child_activated), NULL);
    gtk_widget_add_events(g_flow, GDK_BUTTON_PRESS_MASK);
    g_signal_connect(g_flow, "button-press-event", G_CALLBACK(on_flow_button), NULL);

    g_search = gtk_search_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(g_search), "Search");
    gtk_entry_set_has_frame(GTK_ENTRY(g_search), FALSE);
    css_class(g_search, "xxri-search");
    gtk_widget_set_margin_top(g_search, 24);
    g_signal_connect(g_search, "changed",  G_CALLBACK(on_search_changed),  NULL);
    g_signal_connect(g_search, "activate", G_CALLBACK(on_search_activate), NULL);
    gtk_box_pack_end(GTK_BOX(col), g_search, FALSE, FALSE, 0);

    gtk_widget_show_all(g_win);

    /* override-redirect gets no focus from the WM: grab the seat (typing,
     * outside clicks) and give the old focus back on exit */
    Window prev_focus = PointerRoot; int prev_revert = RevertToPointerRoot;
    XGetInputFocus(gdk_x11_get_default_xdisplay(), &prev_focus, &prev_revert);
    {
        GdkWindow *gw = gtk_widget_get_window(g_win);
        GdkDisplay *dp = gdk_display_get_default();
        g_seat = gdk_display_get_default_seat(dp);
        if (g_seat && gw)
            gdk_seat_grab(g_seat, gw, GDK_SEAT_CAPABILITY_ALL, TRUE,
                          NULL, NULL, NULL, NULL);
        if (gw) gdk_window_focus(gw, GDK_CURRENT_TIME);
    }
    gtk_widget_grab_focus(g_search);

    gtk_main();
    unlink(PIDFILE);
    {
        /* the old focus window may be gone by now */
        GdkDisplay *dp = gdk_display_get_default();
        Display *xd = gdk_x11_get_default_xdisplay();
        gtk_widget_hide(g_win);
        gdk_x11_display_error_trap_push(dp);
        if (prev_focus == None) prev_focus = PointerRoot;
        XSetInputFocus(xd, prev_focus,
                       prev_focus == PointerRoot ? RevertToPointerRoot : RevertToParent,
                       CurrentTime);
        XSync(xd, False);
        gdk_x11_display_error_trap_pop_ignored(dp);
    }
    return 0;
}
