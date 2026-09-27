/* xxri-launcher - the XXRI OS Lite "All Apps" launcher.
 *
 * A One UI-style app drawer: one centred, translucent, rounded panel holding a
 * grid of every installed application with a search field along the bottom.
 * It is an OVERLAY - the desktop and the dock stay visible and untouched
 * behind it, which is why this is a panel-sized window rather than a
 * fullscreen scrim.
 *
 * The app list is not hard-coded.  It is read from the freedesktop .desktop
 * files in /usr/local/share/applications at every open, which is the same
 * place the Store writes an entry when it integrates an application - so an
 * app installed from the Store shows up here with no extra wiring.
 *
 * Window type: GTK_WINDOW_POPUP (override-redirect).  The mockups have no
 * titlebar and flwm would insist on drawing one for a normal toplevel; being
 * override-redirect also keeps the drawer out of the window list.  The cost is
 * that the window manager will not hand us the keyboard, so the seat is
 * grabbed explicitly - that grab is also what lets a click anywhere outside
 * the panel dismiss it.
 */
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <cairo.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>

#define CSS_PATH   "/usr/local/share/xxri-launcher/xxri-launcher.css"
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
    char *id;        /* .desktop id (filename minus ".desktop") - the handle
                       * pinned.list and xxri-dock-pin identify this app by */
    char *name;      /* Name=, shown under the icon                    */
    char *lname;     /* lower-cased name, for searching                */
    char *lextra;    /* lower-cased comment/generic name, also searched */
    char *exec;      /* Exec= with the %-field codes removed           */
    char *icon;      /* absolute path to a usable icon file, or NULL   */
    char *key;       /* xxri-dock-launch focus key / WM_CLASS instance */
    gboolean pinned; /* currently in the dock's pinned area             */
} App;

static GPtrArray *g_apps;
static GtkWidget *g_win, *g_flow, *g_search;
static GdkSeat   *g_seat;   /* the seat the drawer holds - see on_flow_button() */
static gboolean   g_composited = FALSE;
static cairo_surface_t *g_mark = NULL;   /* faint XXRI wordmark behind the grid */

/* pinned.list: one .desktop id per line, comments and blank lines ignored -
 * the exact format xxri-dock-pin itself reads and writes.  Read into a set
 * (id -> anything) so scan_apps() can mark each App.pinned in O(1), and a
 * live pin/unpin flips just that one App's flag instead of re-scanning. */
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

/* ================================================ the XXRI icon container ==
 *
 * Every dock icon gets ONE silhouette, owned by XXRI, so an application's own
 * PNG cannot bring its own shape to the dock.  Measured on the shipped icons,
 * their self-drawn corners range from 32% to 40% of the tile - visibly
 * different radii sitting side by side, which is exactly what this removes.
 *
 * The shape is a superellipse (a squircle) rather than a rounded rectangle:
 * that is the Samsung-style silhouette, and its curvature is continuous, so
 * it reads as one shape rather than four arcs joined to four straight edges.
 *
 * Which of the two treatments an icon gets is decided FROM THE ARTWORK, never
 * from a per-application rule:
 *
 *   - Art that is already its own full-bleed tile (opaque at the edge
 *     midpoints - every XXRI icon is like this) is scaled to fill the
 *     container and clipped to it.  It keeps its own colours and artwork
 *     untouched; only its corners are normalised to the system radius.
 *   - Anything else (a bare logo on transparency, which is what a
 *     third-party .desktop usually points at) is centred inside a neutral
 *     XXRI plate.  That plate is what gives such a logo the same silhouette
 *     as everything else instead of floating shapelessly in the dock.
 */
#define SQUIRCLE_N   4.0    /* superellipse exponent; 4 is the One UI-ish look */
#define LOGO_INSET   0.62   /* a bare logo occupies this much of the container */

static void squircle_path(cairo_t *cr, double x, double y, double s) {
    const int STEPS = 96;               /* smooth at any size the dock uses */
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

/* "Is this artwork its own tile?" - sampled at the four edge midpoints, just
 * inside the edge.  A tile is opaque there; a logo on transparency is not. */
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

/* The colour the artwork has NEAREST one of its own corners: walk in along the
 * diagonal until the art turns opaque.  Clipping alone cannot give every icon
 * the same silhouette, because a clip only ever removes pixels - art whose own
 * corners are rounder than the container (xxri-file's are, and every shipped
 * icon also stops 2-4px short of the edge) would simply keep its own rounder
 * corners.  So the container is FILLED first, with the art's own corner
 * colours interpolated across it, and the art painted on top; the sliver
 * between the two shapes then continues the icon's gradient instead of
 * banding against it. */
static void sample_corner(GdkPixbuf *pb, int cx, int cy, double *r, double *g, double *b) {
    int w = gdk_pixbuf_get_width(pb), h = gdk_pixbuf_get_height(pb);
    int rs = gdk_pixbuf_get_rowstride(pb), nc = gdk_pixbuf_get_n_channels(pb);
    const guchar *p = gdk_pixbuf_get_pixels(pb);
    int dx = cx < w / 2 ? 1 : -1, dy = cy < h / 2 ? 1 : -1;
    int steps = MIN(w, h) * 2 / 5;
    /* Start a tenth of the way in rather than at the very pixel: art of this
     * kind carries an antialiased, part-blended rim, and sampling THAT gives
     * the backfill a pale fringe where it meets the real artwork. */
    int i0 = MAX(3, MIN(w, h) / 10);
    *r = *g = *b = 1.0;                       /* white if the art never opens up */
    for (int i = i0; i < steps; i++) {
        int px = CLAMP(cx + dx * i, 0, w - 1), py = CLAMP(cy + dy * i, 0, h - 1);
        const guchar *q = p + py * rs + px * nc;
        if (nc < 4 || q[3] > 200) {
            *r = q[0] / 255.0; *g = q[1] / 255.0; *b = q[2] / 255.0;
            return;
        }
    }
}

/* pb may be NULL - a missing or unreadable icon (an app whose .desktop points
 * at a file that never got written) still gets the container, so the dock
 * shows a consistent empty tile instead of a hole or a broken-image glyph. */
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

        double sc = MAX(s / iw, s / ih);          /* fill, never letterbox */
        cairo_translate(cr, x + (s - iw*sc)/2.0, y + (s - ih*sc)/2.0);
        cairo_scale(cr, sc, sc);
        gdk_cairo_set_source_pixbuf(cr, pb, 0, 0);
        cairo_paint(cr);
    } else {
        cairo_pattern_t *pat = cairo_pattern_create_linear(x, y, x, y + s);
        cairo_pattern_add_color_stop_rgb(pat, 0, 1.000, 1.000, 1.000);
        cairo_pattern_add_color_stop_rgb(pat, 1, 0.910, 0.918, 0.973);
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

/* `xxri-launcher --frame-icon SRC DST SIZE` - wbar can only load a PNG from a
 * path, so the pinned icons have to be pre-rendered through the container and
 * cached; xxri-dock-pin calls this while it builds the dock.  It deliberately
 * does NOT gtk_init: this runs at boot, before (or without) a display. */
static int run_frame_icon(const char *src, const char *dst, int size) {
    GdkPixbuf *pb = gdk_pixbuf_new_from_file_at_scale(src, size, size, TRUE, NULL);
    cairo_surface_t *sf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, size, size);
    cairo_t *cr = cairo_create(sf);
    draw_icon_in_container(cr, pb, 0, 0, size);
    cairo_destroy(cr);
    cairo_status_t st = cairo_surface_write_to_png(sf, dst);
    cairo_surface_destroy(sf);
    if (pb) g_object_unref(pb);
    return st == CAIRO_STATUS_SUCCESS ? 0 : 1;
}

/* ============================================ the dock's own separator ====
 *
 * The thin white rule between the pinned icons and the running ones is drawn
 * INTO the dock's background image, not shipped as an icon of its own.
 *
 * It has to be, because wbar normalises every entry it draws to one -isize
 * square: a separator entry is laid out at a full icon's width however narrow
 * its image is (measured on the running dock - the row pitch is exactly 47px,
 * -isize 40 + -idist 7, for every entry), so a 2px rule was costing 54px of
 * dock and pushing the running section far away from the pinned icons.  wbar
 * DOES stretch this one background image across the whole bar, so a rule drawn
 * at the right fraction of it lands in the gap after the last pinned icon
 * while every icon keeps the same uniform 47px spacing.
 *
 * The geometry below is the dock's own, measured off it rather than assumed.
 * n icons occupy 47n-7 (n icons of 40 with n-1 gaps of 7); the background is
 * stretched over that plus half an icon of zoom headroom at each end, and the
 * gap that follows icon k sits at 47k-3.5 inside the icon row.
 *
 * The headroom is the part worth knowing: the image is NOT stretched over the
 * icons, it is stretched over the wider rectangle wbar reserves so icons have
 * somewhere to grow into on hover.  Calibrated against the pill's own rim,
 * which the background draws at a known place (canvas x=4 and x=635): on an
 * 8-icon dock those land at screen x=309 and x=712, which puts the mapping at
 * 0.639 screen pixels per image pixel and the rectangle at 409.8 wide for an
 * icon row of 369 - i.e. 20.4 a side.
 */
#define DOCK_PITCH   47.0   /* -isize 40 + -idist 7, from dot.wbar             */
#define DOCK_ICON    40.0   /* -isize                                          */
#define DOCK_BARPAD  20.4   /* zoom headroom the bar reserves, each side       */
#define DOCK_BARH    48.5   /* bar height the background is stretched over     */
#define SEP_W         2.0   /* the rule, on screen: same 2px it always was     */
#define SEP_H        24.0   /* ... and the same height                          */
/* The geometric centre of the gap (see sepx below) rendered ~1-2 screen px
 * left of where it reads as centred - the running icon's own rounded
 * squircle edge starts appreciably later than its bounding box, so the gap
 * as the eye judges it is not quite the gap as the math measures it.  A
 * small empirical correction, nothing more; it does not change the geometry
 * the position is derived from. */
#define SEP_X_NUDGE   2.5   /* screen px, positive = toward the running icon   */
/* Solid, not the artwork's old 0.65.  The old separator was a translucent
 * white line in an ICON, and it only ever read as a line at all because
 * Imlib2 scaled that icon without premultiplying - the transparent pixels
 * around it dragged their black into the edges and it came out as a grey
 * smudge.  Drawn into the background there is no such fringe, and 0.65 white
 * over a pill that is already 0.93 white at 0.66 alpha is invisible.  Full
 * white is what actually renders as the white rule it was always meant to be. */
#define SEP_ALPHA     1.0

static int run_dock_bg(const char *src, const char *dst, int k, int n) {
    if (k < 0 || n <= 0 || k > n) return 1;
    cairo_surface_t *bg = cairo_image_surface_create_from_png(src);
    if (!bg || cairo_surface_status(bg) != CAIRO_STATUS_SUCCESS) return 1;
    double cw = cairo_image_surface_get_width(bg);
    double ch = cairo_image_surface_get_height(bg);

    /* what the image is stretched onto, in screen pixels */
    double barw = n * DOCK_PITCH - (DOCK_PITCH - DOCK_ICON) + 2 * DOCK_BARPAD;
    /* the middle of the gap that follows the k-th icon */
    double sepx = DOCK_BARPAD + k * DOCK_PITCH - (DOCK_PITCH - DOCK_ICON) / 2.0
                + SEP_X_NUDGE;

    /* ... expressed back in the image's own pixels, so that after wbar
     * stretches it the rule comes out the size it is supposed to be */
    double x = sepx * cw / barw;
    double w = SEP_W * cw / barw;
    double h = SEP_H * ch / DOCK_BARH;

    cairo_t *cr = cairo_create(bg);
    cairo_set_source_rgba(cr, 1, 1, 1, SEP_ALPHA);
    cairo_rectangle(cr, x - w / 2.0, (ch - h) / 2.0, w, h);
    cairo_fill(cr);
    cairo_destroy(cr);

    cairo_status_t st = cairo_surface_write_to_png(bg, dst);
    cairo_surface_destroy(bg);
    return st == CAIRO_STATUS_SUCCESS ? 0 : 1;
}

/* Exec= carries field codes (%U %f %i ...) that are only meaningful when the
 * launcher passes documents in.  We never do, and leaving them in would hand
 * the application a literal "%U" argument. */
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

/* The focus key xxri-dock-launch matches against WM_CLASS, so clicking an app
 * that is already open raises it instead of starting a second copy - exactly
 * what the dock does.  Two cases cannot be focused by class and are marked "+"
 * (always start a new one): a terminal, and a Store app launched indirectly
 * through `xxri-app launch <id>`, whose class is the app's own, not xxri-app's. */
/* X-XXRI-DockKey, when a .desktop carries one, is used VERBATIM (including any
 * +/- prefix) instead of the derivation below - the escape hatch for the
 * handful of apps whose WM_CLASS does not match their own binary name.  The
 * editor is Tiny Core's FLTK one and calls itself WM_CLASS "FLTK", not
 * "editor"; without this override every other piece that keys off this
 * value (the dock's own indicator/focus, and the running-apps strip's
 * "already pinned, do not show twice" check) would silently never match a
 * real editor window.  xxri-dock-pin's dock_key_for() honours the same field
 * for the same reason when it builds the live dock. */
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

/* Icon resolution, most specific first.  X-FullPathIcon is an XXRI convention
 * already used by every shipped .desktop file and is trusted before Icon=,
 * because the themed-icon lookup is the slow path on this hardware. */
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
    const char *n;
    while ((n = g_dir_read_name(d))) {
        if (!g_str_has_suffix(n, ".desktop")) continue;
        char *path = g_build_filename(APPDIR, n, NULL);
        char *body = NULL;
        if (!g_file_get_contents(path, &body, NULL, NULL)) { g_free(path); continue; }

        /* Only the [Desktop Entry] group matters; cutting the action groups off
         * keeps a per-action Name=/Exec= from being read as the app's own. */
        char *start = strstr(body, "[Desktop Entry]");
        char *scope = g_strdup(start ? start : body);
        char *next  = strstr(scope + 1, "\n[");
        if (next) *next = '\0';

        char *type = kv(scope, "Type");
        char *name = kv(scope, "Name");
        char *exec = kv(scope, "Exec");
        gboolean skip = truthy(kv(scope, "NoDisplay")) || truthy(kv(scope, "Hidden"));
        /* OnlyShowIn is deliberately NOT honoured.  Tiny Core stamps
         * "OnlyShowIn=Old;" on aterm.desktop to keep it out of its own legacy
         * menu, and obeying that would drop the Terminal - a real, dock-pinned
         * application - out of the drawer. */
        /* The drawer has a .desktop of its own so it appears in the window
         * manager's menu, but listing itself inside its own grid would be
         * absurd - skip it. */
        if (!skip && exec && strstr(exec, "xxri-launcher")) skip = TRUE;
        if (!skip && name && *name && exec && *exec &&
            (!type || !strcmp(type, "Application"))) {
            App *a = g_new0(App, 1);
            a->id    = g_strndup(n, strlen(n) - strlen(".desktop"));
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
        /* Unparseable Exec (unbalanced quotes): hand the whole line to a shell
         * rather than silently doing nothing when the icon is clicked. */
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

/* xxri-dock-pin does the actual work (and needs sudo for the root:staff
 * tce.icons file), so pin/unpin here is just running it and reading whether
 * it succeeded - the same "shell out, check the exit code" idiom the rest of
 * this codebase uses for anything that touches system state. */
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

/* The drawer holds the whole seat (that grab is what delivers typing to the
 * search field and gives us the outside clicks that dismiss it).  A GtkMenu
 * needs a grab of its OWN to work, and it cannot take one while ours is
 * outstanding: the menu then renders perfectly but is completely dead -
 * clicking an item fires no "activate" at all, and every click and keystroke
 * keeps going to the widgets underneath instead.  So the seat is handed back
 * for exactly as long as the menu is up, and taken again when it closes.
 * "deactivate" covers both ways out, a selection and a dismissal. */
static void seat_regrab(GtkMenuShell *m, gpointer d) {
    (void)m; (void)d;
    GdkWindow *gw = gtk_widget_get_window(g_win);
    if (g_seat && gw)
        gdk_seat_grab(g_seat, gw, GDK_SEAT_CAPABILITY_ALL, TRUE,
                      NULL, NULL, NULL, NULL);
}

/* Right-click a tile: "Pin to Dock" if it is not on the dock (greyed out once
 * the pinned area is full, per the 8-icon cap), "Unpin from Dock" if it
 * already is.  The drawer itself stays open underneath. */
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

/* Enter in the search field runs the first match, so a search can be completed
 * without moving to the mouse. */
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
    /* GTK runs the filter once as soon as it is installed, which happens while
     * the grid is being built - before the search field exists. */
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
    /* With the compositor running the window has a real alpha channel, so the
     * area outside the rounded panel is cleared to nothing and the desktop
     * behind shows through.  Without it an ARGB window paints black, so the
     * transparent margin is filled with the wallpaper's own lavender instead -
     * the panel still reads correctly, it just loses the soft shadow. */
    if (g_composited) cairo_set_source_rgba(cr, 0, 0, 0, 0);
    else              cairo_set_source_rgb(cr, 0.906, 0.918, 0.984);
    cairo_paint(cr);
    cairo_restore(cr);

    if (g_composited) {
        /* Six wide steps rather than one per pixel: this is a 700x560 surface
         * on software rendering, and the difference is invisible. */
        for (int i = SHADOW; i >= 1; i -= 2) {
            double t = (double)i / SHADOW;
            cairo_set_source_rgba(cr, 0.06, 0.05, 0.16, 0.13 * (1.0-t) * (1.0-t));
            rrect(cr, SHADOW - i, SHADOW - i + 2.0,
                  pw + 2*i, ph + 2*i, RADIUS + i);
            cairo_fill(cr);
        }
    }

    /* the panel: a bright, faintly lavender glass */
    cairo_pattern_t *pat = cairo_pattern_create_linear(SHADOW, SHADOW, SHADOW, SHADOW+ph);
    cairo_pattern_add_color_stop_rgba(pat, 0, 1.000, 1.000, 1.000, 0.93);
    cairo_pattern_add_color_stop_rgba(pat, 1, 0.937, 0.945, 0.992, 0.90);
    rrect(cr, SHADOW, SHADOW, pw, ph, RADIUS);
    cairo_set_source(cr, pat);
    cairo_fill(cr);
    cairo_pattern_destroy(pat);

    /* the wordmark, very faint, centred - the same touch the mockup has behind
     * its grid.  Drawn under the widgets because this runs before the children. */
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

    /* the glass rim, the same hairline the dock and Control Center carry */
    cairo_set_line_width(cr, 1.0);
    cairo_set_source_rgba(cr, 1, 1, 1, 0.70);
    rrect(cr, SHADOW + 0.5, SHADOW + 0.5, pw - 1, ph - 1, RADIUS);
    cairo_stroke(cr);
    return FALSE;                 /* let the children draw on top */
}

/* If the compositor is not running an ARGB window paints black, so the drawer
 * falls back to an opaque panel - and an opaque panel would show its square
 * corners.  Clipping the window with the X Shape extension makes the corners
 * genuinely not part of the window, so the desktop shows through them.  The
 * mask is rendered as a 1-bit surface and handed straight to GDK, which is
 * both shorter and more accurate than stepping the corner arcs by hand.  With a
 * compositor this is skipped: real alpha gives antialiased corners instead. */
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

static gboolean on_key(GtkWidget *w, GdkEventKey *e, gpointer d) {
    (void)w; (void)d;
    if (e->keyval == GDK_KEY_Escape) { gtk_main_quit(); return TRUE; }
    return FALSE;
}

/* Pressing the dock's All Apps button while the drawer is open used to close it
 * and immediately open it again, so the button looked dead: the press dismisses
 * the drawer as an outside click, and wbar - which acts on button press - then
 * starts a fresh copy from the very same press.  Neither half can be suppressed
 * from here, so instead the moment of closing is recorded, and a copy starting
 * within REOPEN_MS treats itself as the second half of that one gesture and
 * exits.  Pressing the button therefore toggles, which is what it looks like it
 * should do, and this works no matter which client the press is delivered to. */
static void stamp_closed(void) {
    char b[32];
    snprintf(b, sizeof b, "%" G_GINT64_FORMAT, g_get_monotonic_time() / 1000);
    g_file_set_contents(CLOSEFILE, b, -1, NULL);
}

/* A click outside the panel closes the drawer.  Because the seat is grabbed,
 * presses anywhere on screen arrive here; the ones inside the panel are passed
 * on so buttons and the entry keep working. */
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

/* A toggle from elsewhere (the window manager menu, a second dock press that
 * reached us rather than wbar) arrives as SIGTERM.  Quitting the main loop
 * rather than dying on the spot is what lets the pid file be removed, so the
 * next press does not find a stale pid. */
static gboolean quit_idle(gpointer d) { (void)d; gtk_main_quit(); return FALSE; }
static void on_term(int s) { (void)s; g_idle_add(quit_idle, NULL); }

/* ======================================================= the dock button ==
 *
 * `xxri-launcher --button` is the small floating All Apps key that sits just
 * to the LEFT of the dock, separated from it by a gap - a distinct surface, not
 * a wbar slot.  It has to be its own window because wbar can only draw icons
 * inside its own background; there is no way to detach one of its slots.
 *
 * It is drawn rather than iconified: the same glass the dock is made of
 * (sampled from xxri-theme/wbar-bg.png) with the nine dots painted straight
 * onto it, so the two surfaces read as the same material.  Putting the light
 * tile PNG on a glass pill instead would nest one tile inside another.
 *
 * The dock's real geometry is read from wbar's own window, exactly the way
 * xxri-wctl positions the running indicators, so the button stays glued to the
 * dock's left edge at any screen size or icon count instead of assuming a
 * hard-coded layout.
 */
#define BTN_GAP     10      /* the visible separation from the dock */
#define DOT_WBAR    "/usr/local/share/wbar/dot.wbar"
#define ICON_FALLBACK 40    /* dot.wbar's own -isize, if the file is unreadable */

static GtkWidget *g_btn;
static int      g_btn_side  = ICON_FALLBACK;
static gboolean g_btn_hover = FALSE;
static gboolean g_btn_down  = FALSE;

/* The button's own size comes from the dock's -isize, not the wbar window's
 * full height.  wbar reserves extra vertical room above its icons for the
 * hover-zoom animation (-zoomf in dot.wbar), so the WINDOW is taller than the
 * icons actually drawn in it; sizing the button to that full height made it
 * visibly bulkier than a real dock icon and top-aligned rather than centred
 * on the icon row.  Reading the same -isize the dock itself uses keeps the
 * two in lockstep if that number is ever tuned, rather than guessing a ratio. */
static int dock_icon_size(void) {
    static int cached = -1;
    if (cached > 0) return cached;
    char *body = NULL;
    if (g_file_get_contents(DOT_WBAR, &body, NULL, NULL)) {
        const char *p = strstr(body, "-isize");
        if (p) {
            int v = atoi(p + strlen("-isize"));
            if (v >= 16 && v <= 256) cached = v;
        }
        g_free(body);
    }
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
    double rad = H * 0.39;            /* measured off the dock's own corners */

    cairo_save(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, 0, 0, 0, 0);
    cairo_paint(cr);
    cairo_restore(cr);

    /* the dock's glass, a touch brighter under the pointer */
    double lift = g_btn_hover ? 0.06 : 0.0;
    if (g_btn_down) lift = -0.04;
    cairo_pattern_t *pat = cairo_pattern_create_linear(0, 0, 0, H);
    cairo_pattern_add_color_stop_rgba(pat, 0, 0.992, 0.993, 0.999, 0.746 + lift);
    cairo_pattern_add_color_stop_rgba(pat, 1, 0.955, 0.961, 0.995, 0.690 + lift);
    rrect(cr, 0, 0, S, H, rad);
    cairo_set_source(cr, pat);
    cairo_fill(cr);
    cairo_pattern_destroy(pat);

    cairo_set_line_width(cr, 1.0);
    cairo_set_source_rgba(cr, 1, 1, 1, 0.55);
    rrect(cr, 0.5, 0.5, S - 1, H - 1, rad);
    cairo_stroke(cr);

    /* the nine dots, same geometry and gradient as xxri-launcher.svg */
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

/* Same reason as the drawer's apply_shape(): with no compositor the ARGB window
 * paints black, so without clipping the button would sit on four black corners.
 * Re-applied on every resize because the side follows the dock's height. */
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

/* The drawer is started as a separate process rather than opened in-line: this
 * one stays resident for the whole session, and keeping the transient window
 * out of it means a crash in the drawer can never take the button down - the
 * same reason xxri-wctl's indicators are a process of their own. */
static gboolean btn_press(GtkWidget *w, GdkEventButton *e, gpointer d) {
    (void)e; (void)d;
    g_btn_down = TRUE;
    gtk_widget_queue_draw(w);
    return TRUE;
}

static gboolean btn_release(GtkWidget *w, GdkEventButton *e, gpointer d) {
    (void)e; (void)d;
    g_btn_down = FALSE;
    gtk_widget_queue_draw(w);
    char *argv[] = { (char *)"/usr/local/bin/xxri-launcher", NULL };
    g_spawn_async(NULL, argv, NULL,
                  G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL,
                  NULL, NULL, NULL, NULL);
    return TRUE;
}

/* wbar is restarted whenever the dock is rebuilt (wbar_setup.sh runs at every
 * login, and the Store re-runs it when an app is integrated), and the dock's
 * width changes with it, so the position is re-checked rather than set once. */
static gboolean btn_track(gpointer d) {
    (void)d;
    int dx, dy, dw, dh;
    if (!dock_geometry(&dx, &dy, &dw, &dh)) {
        gtk_widget_hide(g_btn);      /* no dock yet: nothing to sit beside */
        return TRUE;
    }
    int side = dock_icon_size();
    if (side != g_btn_side) {
        g_btn_side = side;
        gtk_widget_set_size_request(g_btn, g_btn_side, g_btn_side);
    }
    /* Centred in the dock's real window band, not top-aligned to it - dh is
     * the wbar window's full height (icons plus zoom headroom), so the icon
     * row itself sits somewhere inside that, not flush with its top edge. */
    int x = dx - BTN_GAP - g_btn_side;
    int y = dy + (dh - g_btn_side) / 2;
    if (x < 2) x = 2;                /* never leave the screen on a tiny dock */
    int cx, cy;
    gtk_window_get_position(GTK_WINDOW(g_btn), &cx, &cy);
    if (cx != x || cy != y) gtk_window_move(GTK_WINDOW(g_btn), x, y);
    if (!gtk_widget_get_visible(g_btn)) gtk_widget_show_all(g_btn);
    return TRUE;
}

static int run_button(void) {
    g_btn = gtk_window_new(GTK_WINDOW_POPUP);
    gtk_window_set_type_hint(GTK_WINDOW(g_btn), GDK_WINDOW_TYPE_HINT_DOCK);
    gtk_window_set_keep_above(GTK_WINDOW(g_btn), TRUE);
    gtk_window_set_accept_focus(GTK_WINDOW(g_btn), FALSE);
    gtk_window_set_focus_on_map(GTK_WINDOW(g_btn), FALSE);
    gtk_window_set_resizable(GTK_WINDOW(g_btn), FALSE);
    gtk_widget_set_app_paintable(g_btn, TRUE);
    {
        GdkScreen *sc = gtk_widget_get_screen(g_btn);
        GdkVisual *rgba = gdk_screen_get_rgba_visual(sc);
        g_composited = gdk_screen_is_composited(sc) && rgba != NULL;
        if (g_composited) gtk_widget_set_visual(g_btn, rgba);
    }
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

/* ==================================================== the dock right-click
 *
 * `xxri-launcher --dockmenu ID X Y` is the "Unpin from Dock" popup for a
 * PINNED icon drawn by wbar itself.  wbar is a third-party binary with no
 * menu of its own, so xxri-wctl (which already grabs button 3 on wbar's
 * window - see cmd_indicators() there) spawns one of these, short-lived,
 * right at the click position, exactly the way xxri-power-menu is spawned
 * fresh per click rather than kept resident. */
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
    /* gtk_menu_popup_at_rect anchors relative to a GdkWindow; the root window
     * IS the coordinate space X already gave us (event.x_root/y_root), so no
     * translation is needed. */
    if (dev) gtk_menu_popup_at_rect(GTK_MENU(menu), root_win, &rect,
                                    GDK_GRAVITY_NORTH_WEST, GDK_GRAVITY_NORTH_WEST, NULL);
    else     gtk_menu_popup_at_pointer(GTK_MENU(menu), NULL);
    gtk_main();
    return 0;
}

int main(int argc, char **argv) {
    /* Before gtk_init: this one runs from xxri-dock-pin at boot, with no
     * display of any kind, and only needs GdkPixbuf + cairo. */
    if (argc >= 4 && !strcmp(argv[1], "--frame-icon"))
        return run_frame_icon(argv[2], argv[3], argc >= 5 ? atoi(argv[4]) : 64);

    /* Same story: xxri-dock-pin calls this while it builds the dock, with no
     * display of its own. */
    if (argc >= 6 && !strcmp(argv[1], "--dock-bg"))
        return run_dock_bg(argv[2], argv[3], atoi(argv[4]), atoi(argv[5]));

    if (argc >= 5 && !strcmp(argv[1], "--dockmenu")) {
        if (!gtk_init_check(&argc, &argv)) return 1;
        return run_dockmenu(argv[2], atoi(argv[3]), atoi(argv[4]));
    }

    gboolean want_button = FALSE;
    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--button")) want_button = TRUE;

    /* --button is the resident All Apps key beside the dock; with no arguments
     * this is the drawer itself.  One binary, because both share the panel
     * geometry, the palette and the dot artwork.  The running applications are
     * NOT drawn here: they are entries in the dock's own wbar config, written
     * by xxri-dock-pin, so they sit inside the one dock pill rather than in a
     * second window pretending to be part of it. */
    if (want_button) {
        if (!gtk_init_check(&argc, &argv)) { fprintf(stderr, "no display\n"); return 1; }
        return run_button();
    }

    /* Toggle: a second press of the button closes the open drawer rather
     * than starting a second one on top of it. */
    {
        char *pid = NULL;
        if (g_file_get_contents(PIDFILE, &pid, NULL, NULL)) {
            pid_t p = (pid_t)atoi(pid);
            g_free(pid);
            if (p > 1 && kill(p, 0) == 0) { kill(p, SIGTERM); return 0; }
        }
    }
    /* The other half of the toggle: this copy was started by the same button
     * press that just dismissed the previous one. */
    {
        char *cs = NULL;
        if (g_file_get_contents(CLOSEFILE, &cs, NULL, NULL)) {
            gint64 t = g_ascii_strtoll(cs, NULL, 10);
            g_free(cs);
            if (g_get_monotonic_time() / 1000 - t < REOPEN_MS) { unlink(CLOSEFILE); return 0; }
        }
    }
    if (!gtk_init_check(&argc, &argv)) { fprintf(stderr, "no display\n"); return 1; }
    signal(SIGTERM, on_term);
    { char b[32]; snprintf(b, sizeof b, "%d", (int)getpid());
      g_file_set_contents(PIDFILE, b, -1, NULL); }

    GtkCssProvider *p = gtk_css_provider_new();
    if (g_file_test(CSS_PATH, G_FILE_TEST_EXISTS))
        gtk_css_provider_load_from_path(p, CSS_PATH, NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(p), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(p);

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
    {
        GdkScreen *sc = gtk_widget_get_screen(g_win);
        GdkVisual *rgba = gdk_screen_get_rgba_visual(sc);
        g_composited = gdk_screen_is_composited(sc) && rgba != NULL;
        if (g_composited) gtk_widget_set_visual(g_win, rgba);
    }
    css_class(g_win, "xxri-launcher");
    gtk_widget_add_events(g_win, GDK_BUTTON_PRESS_MASK | GDK_KEY_PRESS_MASK);
    g_signal_connect(g_win, "draw",              G_CALLBACK(on_draw),   NULL);
    g_signal_connect(g_win, "key-press-event",   G_CALLBACK(on_key),    NULL);
    g_signal_connect(g_win, "button-press-event", G_CALLBACK(on_button), NULL);
    g_signal_connect(g_win, "destroy",           G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(g_win, "size-allocate",     G_CALLBACK(on_size_allocate), NULL);

    /* Size and position.  The width is a fixed share of the screen so the grid
     * always has COLS full columns; the height is grown to the number of rows
     * the installed apps actually need, so the panel neither leaves a large
     * dead area under a short grid nor clips a long one - past the cap the grid
     * scrolls instead.  The whole panel is then centred and lifted clear of the
     * dock: this is an overlay on the desktop, not a replacement for it. */
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

    GtkWidget *sw_ = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw_),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    css_class(sw_, "xxri-scroll");
    gtk_box_pack_start(GTK_BOX(col), sw_, TRUE, TRUE, 0);

    g_flow = gtk_flow_box_new();
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(g_flow), GTK_SELECTION_NONE);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(g_flow), COLS);
    gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(g_flow), COLS);
    gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(g_flow), TRUE);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(g_flow), 10);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(g_flow), 6);
    gtk_widget_set_valign(g_flow, GTK_ALIGN_START);
    css_class(g_flow, "xxri-grid");
    gtk_container_add(GTK_CONTAINER(sw_), g_flow);

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

    /* the search field, along the bottom exactly as in the reference */
    g_search = gtk_search_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(g_search), "Search");
    gtk_entry_set_has_frame(GTK_ENTRY(g_search), FALSE);
    css_class(g_search, "xxri-search");
    gtk_widget_set_margin_top(g_search, 24);
    g_signal_connect(g_search, "changed",  G_CALLBACK(on_search_changed),  NULL);
    g_signal_connect(g_search, "activate", G_CALLBACK(on_search_activate), NULL);
    gtk_box_pack_end(GTK_BOX(col), g_search, FALSE, FALSE, 0);

    gtk_widget_show_all(g_win);

    /* Override-redirect windows get no focus from the window manager, so take
     * the seat directly.  This is what delivers typing to the search field and
     * gives us the outside clicks that dismiss the drawer. */
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
    return 0;
}
