/* xxri-tray - the notification island, bottom right, left of the Control
 * Center chip.
 *
 * Background apps put their icon here and nowhere else; normal apps stay in
 * the dock. Both tray standards are served:
 *   - XEmbed system tray (_NET_SYSTEM_TRAY_S0): Qt/GTK2 apps, GtkSocket does
 *     the embedding;
 *   - StatusNotifierItem over the session bus (org.kde.StatusNotifierWatcher),
 *     with the icon's menu read through com.canonical.dbusmenu.
 * The island is hidden while empty. One override-redirect, opaque window (the
 * embedded icons are 24-bit windows: an ARGB parent makes their
 * ParentRelative backgrounds a BadMatch), cut into a pill with XShape. */
#include <gtk/gtk.h>
#include <gtk/gtkx.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <cairo.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "xxri-theme.h"

#define ISLAND_H   38        /* same height as the Control Center chip */
#define EDGE_X     18
#define EDGE_Y     20
#define GAP        10        /* island to chip */
#define PAD        10
#define SPACING     6
#define ICON_PX    22
#define CELL       28
#define DEFAULT_CHIP_W 150
#define SNI_PATH   "/StatusNotifierItem"

typedef struct Item Item;
struct Item {
    gboolean   sni;
    GtkWidget *w;            /* the cell inside the island */
    /* XEmbed */
    Window     xid;
    /* StatusNotifierItem */
    char      *bus, *path, *key;
    char      *icon_name, *theme_path, *menu, *status, *title;
    gboolean   is_menu, hover;
    cairo_surface_t *surf;   /* IconPixmap, or the icon file */
    GdkEvent  *trigger;      /* the click that asked for a menu (GTK wants it to grab) */
    guint      watch;
};

static GtkWidget *g_win, *g_box;
static GList     *g_items;
static int        g_chip_x = -1, g_chip_w = DEFAULT_CHIP_W;
static Atom       a_opcode, a_rect;

static const char *k_watcher_xml =
"<node><interface name='org.kde.StatusNotifierWatcher'>"
"<method name='RegisterStatusNotifierItem'><arg type='s' direction='in'/></method>"
"<method name='RegisterStatusNotifierHost'><arg type='s' direction='in'/></method>"
"<property name='RegisteredStatusNotifierItems' type='as' access='read'/>"
"<property name='IsStatusNotifierHostRegistered' type='b' access='read'/>"
"<property name='ProtocolVersion' type='i' access='read'/>"
"<signal name='StatusNotifierItemRegistered'><arg type='s'/></signal>"
"<signal name='StatusNotifierItemUnregistered'><arg type='s'/></signal>"
"<signal name='StatusNotifierHostRegistered'/>"
"</interface></node>";

static GDBusConnection *g_bus;

/* ------------------------------------------------------------ layout */

static GdkRectangle primary_area(void) {
    GdkRectangle r = {0, 0, 1024, 768};
    GdkDisplay *d = gdk_display_get_default();
    GdkMonitor *m = d ? gdk_display_get_primary_monitor(d) : NULL;
    if (!m && d) m = gdk_display_get_monitor(d, 0);
    if (m) gdk_monitor_get_geometry(m, &r);
    return r;
}

static void read_chip_rect(void) {
    Display *xd = GDK_DISPLAY_XDISPLAY(gdk_display_get_default());
    Atom type; int fmt; unsigned long n, left; unsigned char *data = NULL;
    g_chip_x = -1; g_chip_w = DEFAULT_CHIP_W;
    if (XGetWindowProperty(xd, DefaultRootWindow(xd), a_rect, 0, 4, False, XA_CARDINAL,
                           &type, &fmt, &n, &left, &data) == Success && data) {
        if (type == XA_CARDINAL && fmt == 32 && n == 4) {
            long *v = (long *)data;
            g_chip_x = (int)v[0]; g_chip_w = (int)v[2];
        }
        XFree(data);
    }
}

static void pill_path(cairo_t *cr, int w, int h) {
    double r = h / 2.0;
    cairo_new_sub_path(cr);
    cairo_arc(cr, w - r, r, r, -M_PI / 2, M_PI / 2);
    cairo_arc(cr, r, r, r, M_PI / 2, 3 * M_PI / 2);
    cairo_close_path(cr);
}

static void shape_pill(int w, int h) {
    GdkWindow *gw = gtk_widget_get_window(g_win);
    if (!gw) return;
    cairo_surface_t *m = cairo_image_surface_create(CAIRO_FORMAT_A8, w, h);
    cairo_t *cr = cairo_create(m);
    pill_path(cr, w, h);
    cairo_fill(cr);
    cairo_destroy(cr);
    cairo_region_t *reg = gdk_cairo_region_create_from_surface(m);
    gdk_window_shape_combine_region(gw, reg, 0, 0);
    cairo_region_destroy(reg);
    cairo_surface_destroy(m);
}

static void relayout(void) {
    int n = 0;
    for (GList *l = g_items; l; l = l->next) {
        Item *i = l->data;
        if (i->w && gtk_widget_get_visible(i->w)) n++;
    }
    if (n == 0) { gtk_widget_hide(g_win); return; }
    GdkRectangle pa = primary_area();
    int sw = pa.x + pa.width, sh = pa.y + pa.height;
    int w = 2 * PAD + n * CELL + (n - 1) * SPACING;
    int right = (g_chip_x >= 0) ? g_chip_x : sw - EDGE_X - g_chip_w;
    int x = right - GAP - w, y = sh - EDGE_Y - ISLAND_H;
    gtk_window_resize(GTK_WINDOW(g_win), w, ISLAND_H);
    gtk_window_move(GTK_WINDOW(g_win), x, y);
    gtk_widget_show(g_win);
    shape_pill(w, ISLAND_H);
    gdk_window_raise(gtk_widget_get_window(g_win));
}

static gboolean on_island_draw(GtkWidget *wd, cairo_t *cr, gpointer d) {
    int w = gtk_widget_get_allocated_width(wd), h = gtk_widget_get_allocated_height(wd);
    gboolean dk = xxri_theme_dark();
    pill_path(cr, w, h);
    if (dk) cairo_set_source_rgb(cr, 0.141, 0.129, 0.141);
    else    cairo_set_source_rgb(cr, 0.945, 0.945, 0.955);
    cairo_fill_preserve(cr);
    cairo_set_line_width(cr, 1.0);
    cairo_set_source_rgba(cr, dk ? 1 : 0, dk ? 1 : 0, dk ? 1 : 0, dk ? 0.14 : 0.10);
    cairo_stroke(cr);
    return FALSE;
}

static void remove_item(Item *it) {
    if (!g_list_find(g_items, it)) return;
    g_items = g_list_remove(g_items, it);
    if (it->w) gtk_widget_destroy(it->w);
    if (it->watch) g_bus_unwatch_name(it->watch);
    if (it->sni && g_bus) {
        g_dbus_connection_emit_signal(g_bus, NULL, "/StatusNotifierWatcher",
            "org.kde.StatusNotifierWatcher", "StatusNotifierItemUnregistered",
            g_variant_new("(s)", it->key), NULL);
    }
    if (it->surf) cairo_surface_destroy(it->surf);
    if (it->trigger) gdk_event_free(it->trigger);
    g_free(it->bus); g_free(it->path); g_free(it->key); g_free(it->icon_name);
    g_free(it->theme_path); g_free(it->menu); g_free(it->status); g_free(it->title);
    g_free(it);
    relayout();
}

/* ------------------------------------------------------------ XEmbed */

static gboolean relayout_idle(gpointer d) { relayout(); return G_SOURCE_REMOVE; }

static gboolean on_plug_removed(GtkSocket *s, gpointer d) {
    /* the client went away: drop its cell (returning FALSE destroys the socket) */
    Item *it = d;
    if (g_list_find(g_items, it)) {
        g_items = g_list_remove(g_items, it);
        g_free(it);
        g_idle_add(relayout_idle, NULL);
    }
    return FALSE;
}

static void dock_xembed(Window xid) {
    for (GList *l = g_items; l; l = l->next)
        if (!((Item *)l->data)->sni && ((Item *)l->data)->xid == xid) return;
    Item *it = g_new0(Item, 1);
    it->xid = xid;
    GtkWidget *sock = gtk_socket_new();
    it->w = sock;
    gtk_widget_set_size_request(sock, CELL, CELL);
    gtk_widget_set_halign(sock, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(sock, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(g_box), sock, FALSE, FALSE, 0);
    g_signal_connect(sock, "plug-removed", G_CALLBACK(on_plug_removed), it);
    g_items = g_list_append(g_items, it);
    gtk_widget_show(sock);
    gtk_socket_add_id(GTK_SOCKET(sock), xid);
    g_idle_add(relayout_idle, NULL);
}

static GdkFilterReturn tray_filter(GdkXEvent *xev, GdkEvent *ev, gpointer d) {
    XEvent *e = xev;
    if (e->type == ClientMessage && e->xclient.message_type == a_opcode) {
        if (e->xclient.data.l[1] == 0)           /* SYSTEM_TRAY_REQUEST_DOCK */
            dock_xembed((Window)e->xclient.data.l[2]);
        return GDK_FILTER_REMOVE;
    }
    return GDK_FILTER_CONTINUE;
}

static GdkFilterReturn root_filter(GdkXEvent *xev, GdkEvent *ev, gpointer d) {
    XEvent *e = xev;
    if (e->type == PropertyNotify && e->xproperty.atom == a_rect) {
        read_chip_rect();
        g_idle_add(relayout_idle, NULL);
    }
    return GDK_FILTER_CONTINUE;
}

static void xembed_start(void) {
    GdkDisplay *gd = gdk_display_get_default();
    Display *xd = GDK_DISPLAY_XDISPLAY(gd);
    int scr = DefaultScreen(xd);
    char name[48]; snprintf(name, sizeof name, "_NET_SYSTEM_TRAY_S%d", scr);
    Atom sel = XInternAtom(xd, name, False);
    a_opcode = XInternAtom(xd, "_NET_SYSTEM_TRAY_OPCODE", False);
    if (XGetSelectionOwner(xd, sel) != None) return;   /* another tray runs */
    /* own a tiny window; the tray protocol sends its messages to it */
    Window root = DefaultRootWindow(xd);
    Window own = XCreateSimpleWindow(xd, root, -10, -10, 1, 1, 0, 0, 0);
    Atom orient = XInternAtom(xd, "_NET_SYSTEM_TRAY_ORIENTATION", False);
    long horiz = 0;
    XChangeProperty(xd, own, orient, XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&horiz, 1);
    XSetSelectionOwner(xd, sel, own, CurrentTime);
    if (XGetSelectionOwner(xd, sel) != own) return;
    GdkWindow *gw = gdk_x11_window_foreign_new_for_display(gd, own);
    if (gw) gdk_window_add_filter(gw, tray_filter, NULL);
    XEvent m; memset(&m, 0, sizeof m);
    m.xclient.type = ClientMessage; m.xclient.window = root;
    m.xclient.message_type = XInternAtom(xd, "MANAGER", False);
    m.xclient.format = 32;
    m.xclient.data.l[0] = CurrentTime; m.xclient.data.l[1] = sel; m.xclient.data.l[2] = own;
    XSendEvent(xd, root, False, StructureNotifyMask, &m);
    XFlush(xd);
}

/* ------------------------------------------------------- SNI: icons */

static cairo_surface_t *surface_from_pixmap(GVariant *arr) {
    /* a(iiay): width, height, ARGB32 bytes in network order; take the size
     * closest to (and not under) what we draw */
    GVariantIter it; GVariant *e; int bw = 0, bh = 0; GVariant *bbytes = NULL;
    g_variant_iter_init(&it, arr);
    while ((e = g_variant_iter_next_value(&it))) {
        int w, h; GVariant *b;
        g_variant_get(e, "(ii@ay)", &w, &h, &b);
        gsize len = g_variant_get_size(b);
        if (w > 0 && h > 0 && len >= (gsize)w * h * 4 &&
            (!bbytes || (bw < ICON_PX && w > bw) || (w >= ICON_PX && w < bw))) {
            if (bbytes) g_variant_unref(bbytes);
            bbytes = b; bw = w; bh = h;
        } else g_variant_unref(b);
        g_variant_unref(e);
    }
    if (!bbytes) return NULL;
    const guchar *src = g_variant_get_data(bbytes);
    cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, bw, bh);
    guchar *dst = cairo_image_surface_get_data(s);
    int stride = cairo_image_surface_get_stride(s);
    cairo_surface_flush(s);
    for (int y = 0; y < bh; y++) {
        guint32 *row = (guint32 *)(dst + y * stride);
        for (int x = 0; x < bw; x++) {
            const guchar *p = src + (y * bw + x) * 4;
            guint a = p[0], r = p[1] * a / 255, g = p[2] * a / 255, b = p[3] * a / 255;
            row[x] = (a << 24) | (r << 16) | (g << 8) | b;
        }
    }
    cairo_surface_mark_dirty(s);
    g_variant_unref(bbytes);
    return s;
}

static cairo_surface_t *png_named(const char *name, const char *theme_path) {
    /* the Store/Settings rule: PNGs through cairo only, no icon theme here */
    if (!name || !*name) return NULL;
    char *cands[24]; int n = 0;
    if (name[0] == '/') cands[n++] = g_strdup(name);
    else {
        static const int sizes[] = {22, 24, 32, 48, 16, 64, 128};
        if (theme_path && *theme_path) {
            cands[n++] = g_strdup_printf("%s/%s.png", theme_path, name);
            for (int i = 0; i < 5; i++)
                cands[n++] = g_strdup_printf("%s/hicolor/%dx%d/apps/%s.png", theme_path, sizes[i], sizes[i], name);
        }
        cands[n++] = g_strdup_printf("/usr/local/share/pixmaps/%s.png", name);
        for (int i = 0; i < 7; i++)
            cands[n++] = g_strdup_printf("/usr/local/share/icons/hicolor/%dx%d/apps/%s.png", sizes[i], sizes[i], name);
        for (int i = 0; i < 7; i++)
            cands[n++] = g_strdup_printf("/usr/share/icons/hicolor/%dx%d/apps/%s.png", sizes[i], sizes[i], name);
    }
    cairo_surface_t *s = NULL;
    for (int i = 0; i < n; i++) {
        if (!s && g_file_test(cands[i], G_FILE_TEST_EXISTS)) {
            cairo_surface_t *t = cairo_image_surface_create_from_png(cands[i]);
            if (cairo_surface_status(t) == CAIRO_STATUS_SUCCESS) s = t; else cairo_surface_destroy(t);
        }
        g_free(cands[i]);
    }
    return s;
}

static gboolean on_cell_draw(GtkWidget *wd, cairo_t *cr, gpointer d) {
    Item *it = d;
    int w = gtk_widget_get_allocated_width(wd), h = gtk_widget_get_allocated_height(wd);
    if (it->hover) {
        cairo_arc(cr, w / 2.0, h / 2.0, w / 2.0, 0, 2 * M_PI);
        cairo_set_source_rgba(cr, 0.5, 0.5, 0.5, 0.22);
        cairo_fill(cr);
    }
    cairo_surface_t *s = it->surf;
    if (!s) s = png_named("xxri-app-generic", NULL);
    if (s) {
        double iw = cairo_image_surface_get_width(s), ih = cairo_image_surface_get_height(s);
        double k = ICON_PX / (iw > ih ? iw : ih);
        cairo_save(cr);
        cairo_translate(cr, (w - iw * k) / 2, (h - ih * k) / 2);
        cairo_scale(cr, k, k);
        cairo_set_source_surface(cr, s, 0, 0);
        cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_BEST);
        cairo_paint(cr);
        cairo_restore(cr);
        if (s != it->surf) cairo_surface_destroy(s);
    } else {
        cairo_arc(cr, w / 2.0, h / 2.0, 6, 0, 2 * M_PI);
        cairo_set_source_rgb(cr, 0.4, 0.4, 0.45);
        cairo_fill(cr);
    }
    return TRUE;
}

/* ------------------------------------------------------- SNI: menus */

typedef struct { Item *it; int id; } MenuCtx;

static void on_menu_activate(GtkMenuItem *mi, gpointer d) {
    MenuCtx *c = d;
    g_dbus_connection_call(g_bus, c->it->bus, c->it->menu, "com.canonical.dbusmenu", "Event",
        g_variant_new("(isvu)", c->id, "clicked", g_variant_new_int32(0), (guint32)(g_get_monotonic_time() / 1000)),
        NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
}

static GtkWidget *build_menu(Item *it, GVariant *node) {
    int id; GVariant *props, *kids;
    g_variant_get(node, "(i@a{sv}@av)", &id, &props, &kids);
    GtkWidget *menu = gtk_menu_new();
    GVariantIter ki; GVariant *kv;
    g_variant_iter_init(&ki, kids);
    while ((kv = g_variant_iter_next_value(&ki))) {
        /* each child is a variant holding a node; tolerate nested wrapping and
         * skip anything that is not a node (an app's menu must not crash us) */
        GVariant *child = g_variant_ref(kv);
        while (g_variant_is_of_type(child, G_VARIANT_TYPE_VARIANT)) {
            GVariant *in = g_variant_get_variant(child);
            g_variant_unref(child); child = in;
        }
        if (!g_variant_is_of_type(child, G_VARIANT_TYPE("(ia{sv}av)"))) {
            g_variant_unref(child); g_variant_unref(kv); continue;
        }
        int cid; GVariant *cp, *ck;
        g_variant_get(child, "(i@a{sv}@av)", &cid, &cp, &ck);
        const char *type = NULL, *label = NULL, *tt = NULL, *cd = NULL;
        gboolean enabled = TRUE, visible = TRUE; int tstate = 0;
        g_variant_lookup(cp, "type", "&s", &type);
        g_variant_lookup(cp, "label", "&s", &label);
        g_variant_lookup(cp, "enabled", "b", &enabled);
        g_variant_lookup(cp, "visible", "b", &visible);
        g_variant_lookup(cp, "toggle-type", "&s", &tt);
        g_variant_lookup(cp, "toggle-state", "i", &tstate);
        g_variant_lookup(cp, "children-display", "&s", &cd);
        if (visible) {
            GtkWidget *mi;
            if (type && !strcmp(type, "separator")) mi = gtk_separator_menu_item_new();
            else if (tt && *tt) {
                mi = gtk_check_menu_item_new_with_mnemonic(label ? label : "");
                gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mi), tstate == 1);
                if (!strcmp(tt, "radio")) gtk_check_menu_item_set_draw_as_radio(GTK_CHECK_MENU_ITEM(mi), TRUE);
            } else mi = gtk_menu_item_new_with_mnemonic(label ? label : "");
            gtk_widget_set_sensitive(mi, enabled);
            if (cd && !strcmp(cd, "submenu") && g_variant_n_children(ck) > 0)
                gtk_menu_item_set_submenu(GTK_MENU_ITEM(mi), build_menu(it, child));
            else if (!(type && !strcmp(type, "separator"))) {
                MenuCtx *c = g_new0(MenuCtx, 1); c->it = it; c->id = cid;
                g_signal_connect_data(mi, "activate", G_CALLBACK(on_menu_activate), c, (GClosureNotify)g_free, 0);
            }
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
        }
        g_variant_unref(cp); g_variant_unref(ck); g_variant_unref(child); g_variant_unref(kv);
    }
    g_variant_unref(props); g_variant_unref(kids);
    gtk_widget_show_all(menu);
    return menu;
}

/* the app's own menu, for items without a usable dbusmenu */
static void app_context_menu(Item *it) {
    g_dbus_connection_call(g_bus, it->bus, it->path, "org.kde.StatusNotifierItem", "ContextMenu",
        g_variant_new("(ii)", 0, 0), NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
}

static void on_layout(GObject *src, GAsyncResult *res, gpointer d) {
    Item *it = d;
    GError *err = NULL;
    GVariant *r = g_dbus_connection_call_finish(G_DBUS_CONNECTION(src), res, &err);
    if (!r || !g_list_find(g_items, it)) {
        if (err) g_error_free(err);
        if (r) g_variant_unref(r);
        if (g_list_find(g_items, it)) app_context_menu(it);
        return;
    }
    GVariant *layout; guint rev;
    g_variant_get(r, "(u@(ia{sv}av))", &rev, &layout);
    GtkWidget *menu = build_menu(it, layout);
    g_object_ref_sink(menu);
    g_signal_connect(menu, "deactivate", G_CALLBACK(g_object_unref), NULL);
    gtk_menu_popup_at_widget(GTK_MENU(menu), it->w, GDK_GRAVITY_NORTH_EAST, GDK_GRAVITY_SOUTH_EAST, it->trigger);
    g_variant_unref(layout); g_variant_unref(r);
}

static void show_item_menu(Item *it) {
    if (it->menu && it->menu[0] == '/' && strcmp(it->menu, "/NO_DBUSMENU")) {
        g_dbus_connection_call(g_bus, it->bus, it->menu, "com.canonical.dbusmenu", "AboutToShow",
            g_variant_new("(i)", 0), NULL, G_DBUS_CALL_FLAGS_NONE, 500, NULL, NULL, NULL);
        g_dbus_connection_call(g_bus, it->bus, it->menu, "com.canonical.dbusmenu", "GetLayout",
            g_variant_new("(iias)", 0, -1, NULL), G_VARIANT_TYPE("(u(ia{sv}av))"),
            G_DBUS_CALL_FLAGS_NONE, 2000, NULL, on_layout, it);
    } else {
        app_context_menu(it);
    }
}

static gboolean on_cell_button(GtkWidget *wd, GdkEventButton *e, gpointer d) {
    Item *it = d;
    int x = (int)e->x_root, y = (int)e->y_root;
    const char *m = NULL;
    if (it->trigger) gdk_event_free(it->trigger);
    it->trigger = gdk_event_copy((GdkEvent *)e);
    if (e->button == 1) {
        if (it->is_menu) { show_item_menu(it); return TRUE; }
        m = "Activate";
    } else if (e->button == 2) m = "SecondaryActivate";
    else if (e->button == 3) { show_item_menu(it); return TRUE; }
    if (m) g_dbus_connection_call(g_bus, it->bus, it->path, "org.kde.StatusNotifierItem", m,
        g_variant_new("(ii)", x, y), NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
    return TRUE;
}

/* ------------------------------------------------------- SNI: items */

static void apply_props(Item *it, GVariant *props) {
    const char *s; GVariant *v;
    g_free(it->icon_name); it->icon_name = NULL;
    if (g_variant_lookup(props, "IconName", "&s", &s)) it->icon_name = g_strdup(s);
    if (g_variant_lookup(props, "IconThemePath", "&s", &s)) { g_free(it->theme_path); it->theme_path = g_strdup(s); }
    if (g_variant_lookup(props, "Menu", "&o", &s)) { g_free(it->menu); it->menu = g_strdup(s); }
    if (g_variant_lookup(props, "Status", "&s", &s)) { g_free(it->status); it->status = g_strdup(s); }
    if (g_variant_lookup(props, "Title", "&s", &s)) { g_free(it->title); it->title = g_strdup(s); }
    g_variant_lookup(props, "ItemIsMenu", "b", &it->is_menu);
    cairo_surface_t *ns = NULL;
    if (it->icon_name && *it->icon_name) ns = png_named(it->icon_name, it->theme_path);
    if (!ns && (v = g_variant_lookup_value(props, "IconPixmap", G_VARIANT_TYPE("a(iiay)")))) {
        ns = surface_from_pixmap(v); g_variant_unref(v);
    }
    if (it->surf) cairo_surface_destroy(it->surf);
    it->surf = ns;
    if (it->w) {
        gtk_widget_set_tooltip_text(it->w, it->title && *it->title ? it->title : NULL);
        gtk_widget_set_visible(it->w, !(it->status && !strcmp(it->status, "Passive")));
        gtk_widget_queue_draw(it->w);
    }
}

static void on_getall(GObject *src, GAsyncResult *res, gpointer d) {
    Item *it = d;
    GError *err = NULL;
    GVariant *r = g_dbus_connection_call_finish(G_DBUS_CONNECTION(src), res, &err);
    if (err) g_error_free(err);
    if (!r) return;
    if (g_list_find(g_items, it)) {
        GVariant *props; g_variant_get(r, "(@a{sv})", &props);
        apply_props(it, props);
        g_variant_unref(props);
        relayout();
    }
    g_variant_unref(r);
}

static void refresh_item(Item *it) {
    g_dbus_connection_call(g_bus, it->bus, it->path, "org.freedesktop.DBus.Properties", "GetAll",
        g_variant_new("(s)", "org.kde.StatusNotifierItem"), G_VARIANT_TYPE("(a{sv})"),
        G_DBUS_CALL_FLAGS_NONE, 2000, NULL, on_getall, it);
}

static void on_item_signal(GDBusConnection *c, const char *sender, const char *path, const char *iface,
                           const char *sig, GVariant *p, gpointer d) {
    if (g_list_find(g_items, d)) refresh_item(d);
}

static void on_name_vanished(GDBusConnection *c, const char *name, gpointer d) { remove_item(d); }

static gboolean on_cell_cross(GtkWidget *wd, GdkEventCrossing *e, gpointer d) {
    Item *it = d;
    it->hover = (e->type == GDK_ENTER_NOTIFY);
    gtk_widget_queue_draw(wd);
    return FALSE;
}

static void register_item(const char *sender, const char *arg) {
    char *bus, *path;
    if (arg[0] == '/') { bus = g_strdup(sender); path = g_strdup(arg); }
    else {
        const char *slash = strchr(arg, '/');
        bus = slash ? g_strndup(arg, slash - arg) : g_strdup(arg);
        path = g_strdup(slash ? slash : SNI_PATH);
    }
    char *key = g_strdup_printf("%s%s", bus, path);
    for (GList *l = g_items; l; l = l->next) {
        Item *o = l->data;
        if (o->sni && !strcmp(o->key, key)) { g_free(bus); g_free(path); g_free(key); return; }
    }
    Item *it = g_new0(Item, 1);
    it->sni = TRUE; it->bus = bus; it->path = path; it->key = key;
    GtkWidget *eb = gtk_event_box_new();
    gtk_event_box_set_visible_window(GTK_EVENT_BOX(eb), FALSE);
    gtk_event_box_set_above_child(GTK_EVENT_BOX(eb), TRUE);   /* GtkDrawingArea has its own window */
    gtk_widget_add_events(eb, GDK_BUTTON_PRESS_MASK | GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK);
    gtk_widget_set_size_request(eb, CELL, CELL);
    gtk_widget_set_halign(eb, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(eb, GTK_ALIGN_CENTER);
    GtkWidget *da = gtk_drawing_area_new();
    gtk_container_add(GTK_CONTAINER(eb), da);
    g_signal_connect(da, "draw", G_CALLBACK(on_cell_draw), it);
    g_signal_connect(eb, "button-press-event", G_CALLBACK(on_cell_button), it);
    g_signal_connect(eb, "enter-notify-event", G_CALLBACK(on_cell_cross), it);
    g_signal_connect(eb, "leave-notify-event", G_CALLBACK(on_cell_cross), it);
    it->w = eb;
    gtk_box_pack_start(GTK_BOX(g_box), eb, FALSE, FALSE, 0);
    g_items = g_list_append(g_items, it);
    it->watch = g_bus_watch_name_on_connection(g_bus, bus, G_BUS_NAME_WATCHER_FLAGS_NONE,
                                               NULL, on_name_vanished, it, NULL);
    for (const char *s = "NewIcon\0NewStatus\0NewTitle\0NewAttentionIcon\0NewOverlayIcon\0"; *s; s += strlen(s) + 1)
        g_dbus_connection_signal_subscribe(g_bus, bus, "org.kde.StatusNotifierItem", s, path, NULL,
                                           G_DBUS_SIGNAL_FLAGS_NONE, on_item_signal, it, NULL);
    gtk_widget_show_all(eb);
    refresh_item(it);
    g_dbus_connection_emit_signal(g_bus, NULL, "/StatusNotifierWatcher", "org.kde.StatusNotifierWatcher",
        "StatusNotifierItemRegistered", g_variant_new("(s)", key), NULL);
    relayout();
}

/* ------------------------------------------------------- SNI: watcher */

static void watcher_call(GDBusConnection *c, const char *sender, const char *op, const char *iface,
                         const char *method, GVariant *params, GDBusMethodInvocation *inv, gpointer d) {
    if (!strcmp(method, "RegisterStatusNotifierItem")) {
        const char *arg; g_variant_get(params, "(&s)", &arg);
        register_item(sender, arg);
    }
    g_dbus_method_invocation_return_value(inv, NULL);
}

static GVariant *watcher_prop(GDBusConnection *c, const char *sender, const char *op, const char *iface,
                              const char *name, GError **err, gpointer d) {
    if (!strcmp(name, "RegisteredStatusNotifierItems")) {
        GVariantBuilder b; g_variant_builder_init(&b, G_VARIANT_TYPE("as"));
        for (GList *l = g_items; l; l = l->next)
            if (((Item *)l->data)->sni) g_variant_builder_add(&b, "s", ((Item *)l->data)->key);
        return g_variant_builder_end(&b);
    }
    if (!strcmp(name, "IsStatusNotifierHostRegistered")) return g_variant_new_boolean(TRUE);
    return g_variant_new_int32(0);
}

static const GDBusInterfaceVTable k_vtable = { watcher_call, watcher_prop, NULL };

static void on_bus_acquired(GDBusConnection *c, const char *name, gpointer d) {
    GDBusNodeInfo *ni = g_dbus_node_info_new_for_xml(k_watcher_xml, NULL);
    g_bus = c;
    g_dbus_connection_register_object(c, "/StatusNotifierWatcher", ni->interfaces[0], &k_vtable, NULL, NULL, NULL);
    g_dbus_node_info_unref(ni);
}

static void on_name_acquired(GDBusConnection *c, const char *name, gpointer d) {
    g_dbus_connection_emit_signal(c, NULL, "/StatusNotifierWatcher", "org.kde.StatusNotifierWatcher",
                                  "StatusNotifierHostRegistered", NULL, NULL);
}

/* ------------------------------------------------------------ main */

static void on_monitors_changed(GdkScreen *s, gpointer d) { relayout(); }
static void on_theme_changed(gboolean dark, gpointer d) { if (g_win) gtk_widget_queue_draw(g_win); }

int main(int argc, char **argv) {
    gdk_set_allowed_backends("x11");     /* XEmbed and XShape are X11 things */
    gtk_init(&argc, &argv);
    xxri_theme_init(NULL, on_theme_changed, NULL);
    GdkDisplay *gd = gdk_display_get_default();
    Display *xd = GDK_DISPLAY_XDISPLAY(gd);
    a_rect = XInternAtom(xd, "_XXRI_CC_RECT", False);
    read_chip_rect();

    g_win = gtk_window_new(GTK_WINDOW_POPUP);          /* override-redirect */
    gtk_window_set_type_hint(GTK_WINDOW(g_win), GDK_WINDOW_TYPE_HINT_DOCK);
    gtk_widget_set_app_paintable(g_win, TRUE);
    g_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, SPACING);
    gtk_widget_set_margin_start(g_box, PAD);
    gtk_widget_set_margin_end(g_box, PAD);
    gtk_container_add(GTK_CONTAINER(g_win), g_box);
    gtk_widget_show(g_box);
    g_signal_connect(g_win, "draw", G_CALLBACK(on_island_draw), NULL);
    gtk_widget_realize(g_win);

    GdkWindow *root = gdk_screen_get_root_window(gdk_screen_get_default());
    gdk_window_set_events(root, gdk_window_get_events(root) | GDK_PROPERTY_CHANGE_MASK);
    gdk_window_add_filter(root, root_filter, NULL);
    g_signal_connect(gdk_screen_get_default(), "monitors-changed", G_CALLBACK(on_monitors_changed), NULL);

    xembed_start();
    g_bus_own_name(G_BUS_TYPE_SESSION, "org.kde.StatusNotifierWatcher", G_BUS_NAME_OWNER_FLAGS_NONE,
                   on_bus_acquired, on_name_acquired, NULL, NULL, NULL);
    gtk_main();
    return 0;
}
