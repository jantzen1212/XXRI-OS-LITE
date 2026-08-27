/* xxri-chrome.h - the XXRI window controls, drawn by the application itself.
 *
 * The mockups do not put a titlebar above these windows.  The △ □ X controls
 * sit INSIDE the application's own top-left surface - the sidebar in Settings,
 * the icon rail in Store - and that surface runs to the top edge of the window.
 * A window-manager titlebar cannot do that: it is a separate opaque strip
 * spanning the whole width, which is exactly the "empty white header" the
 * design does not have.
 *
 * So these windows are undecorated (flwm honours _MOTIF_WM_HINTS, which is what
 * gtk_window_set_decorated(FALSE) sets) and draw the controls themselves.  The
 * glyphs are deliberately identical to the ones flwm draws for every other
 * window, so the language is the same everywhere: a magenta triangle to
 * minimise, a purple rounded square to maximise, a blue circle with an X
 * knocked out of it to close.
 */
#ifndef XXRI_CHROME_H
#define XXRI_CHROME_H
#include <gtk/gtk.h>
#include <math.h>

#define XC_MIN   0xE624CE
#define XC_MAX   0x7B22F0
#define XC_CLOSE 0x2A22EE
#define XC_BTN_W 15          /* hit width per control, as in flwm */
#define XC_GLYPH 5           /* half-size of the drawn glyph        */

typedef struct {
    GtkWidget* win;
    int        hot;          /* control under the pointer, -1 = none */
    int        pressed;
    gboolean   maximized;
    int        rx, ry, rw, rh;   /* geometry to restore to */
} XxriChrome;

static void xc_rgb(cairo_t* cr, unsigned v, double a) {
    cairo_set_source_rgba(cr, ((v>>16)&0xff)/255.0, ((v>>8)&0xff)/255.0,
                          (v&0xff)/255.0, a);
}

static gboolean xc_draw(GtkWidget* w, cairo_t* cr, gpointer d) {
    XxriChrome* c = d;
    int h = gtk_widget_get_allocated_height(w);
    for (int i = 0; i < 3; i++) {
        double cx = i * XC_BTN_W + XC_BTN_W / 2.0;
        double cy = h / 2.0;
        unsigned base = i == 0 ? XC_MIN : i == 1 ? XC_MAX : XC_CLOSE;
        gboolean hot = (c->hot == i), press = (c->pressed == i);
        /* the same felt-not-seen halo flwm uses */
        if (hot || press) {
            cairo_set_source_rgba(cr, 0.11, 0.10, 0.14, press ? 0.13 : 0.07);
            cairo_arc(cr, cx, cy, 8.5, 0, 2*M_PI);
            cairo_fill(cr);
        }
        xc_rgb(cr, base, press ? 0.80 : 1.0);
        const double r = XC_GLYPH;
        if (i == 0) {                       /* minimise: left-pointing triangle */
            cairo_move_to(cr, cx - r - 1, cy);
            cairo_line_to(cr, cx + r, cy - r - 1);
            cairo_line_to(cr, cx + r, cy + r + 1);
            cairo_close_path(cr); cairo_fill(cr);
        } else if (i == 1) {                /* maximise: rounded square */
            double x0 = cx - r, y0 = cy - r, s = 2*r, rr = 2.0;
            cairo_new_sub_path(cr);
            cairo_arc(cr, x0+s-rr, y0+rr,   rr, -M_PI/2, 0);
            cairo_arc(cr, x0+s-rr, y0+s-rr, rr, 0, M_PI/2);
            cairo_arc(cr, x0+rr,   y0+s-rr, rr, M_PI/2, M_PI);
            cairo_arc(cr, x0+rr,   y0+rr,   rr, M_PI, 1.5*M_PI);
            cairo_close_path(cr); cairo_fill(cr);
        } else {                            /* close: circle with an X cut out */
            cairo_arc(cr, cx, cy, r + 0.5, 0, 2*M_PI);
            cairo_fill(cr);
            cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
            cairo_set_line_width(cr, (hot || press) ? 2.0 : 1.4);
            cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
            double a = r - 2;
            cairo_move_to(cr, cx-a, cy-a); cairo_line_to(cr, cx+a, cy+a);
            cairo_move_to(cr, cx-a, cy+a); cairo_line_to(cr, cx+a, cy-a);
            cairo_stroke(cr);
            cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
        }
    }
    return TRUE;
}

static int xc_index(GdkEventButton* e) {
    int i = (int)(e->x / XC_BTN_W);
    return (i >= 0 && i < 3) ? i : -1;
}

/* Maximise without EWMH: flwm is ICCCM-only, so the window does it itself and
   stops above the dock, exactly where a window-manager maximise would. */
static void xc_toggle_max(XxriChrome* c) {
    GtkWindow* w = GTK_WINDOW(c->win);
    GdkScreen* sc = gtk_widget_get_screen(c->win);
    int sw = gdk_screen_get_width(sc), sh = gdk_screen_get_height(sc);
    const char* res = g_getenv("XXRI_DOCK_RESERVE");
    int reserve = res ? atoi(res) : 78;
    if (c->maximized) {
        gtk_window_move(w, c->rx, c->ry);
        gtk_window_resize(w, c->rw, c->rh);
        c->maximized = FALSE;
    } else {
        gtk_window_get_position(w, &c->rx, &c->ry);
        gtk_window_get_size(w, &c->rw, &c->rh);
        gtk_window_move(w, 0, 0);
        gtk_window_resize(w, sw, sh - reserve);
        c->maximized = TRUE;
    }
}

static gboolean xc_press(GtkWidget* w, GdkEventButton* e, gpointer d) {
    XxriChrome* c = d;
    int i = xc_index(e);
    if (i < 0 || e->button != 1) return FALSE;
    c->pressed = i;
    gtk_widget_queue_draw(w);
    return TRUE;
}
static gboolean xc_release(GtkWidget* w, GdkEventButton* e, gpointer d) {
    XxriChrome* c = d;
    int i = xc_index(e);
    int was = c->pressed;
    c->pressed = -1;
    gtk_widget_queue_draw(w);
    if (i < 0 || i != was) return TRUE;
    if (i == 0)      gtk_window_iconify(GTK_WINDOW(c->win));
    else if (i == 1) xc_toggle_max(c);
    else             gtk_window_close(GTK_WINDOW(c->win));
    return TRUE;
}
static gboolean xc_motion(GtkWidget* w, GdkEventMotion* e, gpointer d) {
    XxriChrome* c = d;
    int i = (int)(e->x / XC_BTN_W);
    if (i < 0 || i > 2) i = -1;
    if (i != c->hot) { c->hot = i; gtk_widget_queue_draw(w); }
    return FALSE;
}
static gboolean xc_leave(GtkWidget* w, GdkEventCrossing* e, gpointer d) {
    (void)e; XxriChrome* c = d;
    if (c->hot != -1 || c->pressed != -1) { c->hot = c->pressed = -1; gtk_widget_queue_draw(w); }
    return FALSE;
}

/* The area beside the controls drags the window, and double-click maximises -
   the two things a titlebar was doing for us. */
static gboolean xc_drag_press(GtkWidget* w, GdkEventButton* e, gpointer d) {
    (void)w; XxriChrome* c = d;
    if (e->button != 1) return FALSE;
    if (e->type == GDK_2BUTTON_PRESS) { xc_toggle_max(c); return TRUE; }
    gtk_window_begin_move_drag(GTK_WINDOW(c->win), e->button,
                               (gint)e->x_root, (gint)e->y_root, e->time);
    return TRUE;
}

static XxriChrome* xxri_chrome_new(GtkWidget* win) {
    XxriChrome* c = g_new0(XxriChrome, 1);
    c->win = win; c->hot = -1; c->pressed = -1;
    gtk_window_set_decorated(GTK_WINDOW(win), FALSE);
    return c;
}

/* The control cluster widget itself. */
static GtkWidget* xxri_chrome_controls(XxriChrome* c) {
    GtkWidget* da = gtk_drawing_area_new();
    gtk_widget_set_size_request(da, XC_BTN_W * 3, 22);
    gtk_widget_add_events(da, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
                              GDK_POINTER_MOTION_MASK | GDK_LEAVE_NOTIFY_MASK);
    g_signal_connect(da, "draw",                G_CALLBACK(xc_draw),    c);
    g_signal_connect(da, "button-press-event",  G_CALLBACK(xc_press),   c);
    g_signal_connect(da, "button-release-event",G_CALLBACK(xc_release), c);
    g_signal_connect(da, "motion-notify-event", G_CALLBACK(xc_motion),  c);
    g_signal_connect(da, "leave-notify-event",  G_CALLBACK(xc_leave),   c);
    gtk_widget_set_halign(da, GTK_ALIGN_START);
    gtk_widget_set_valign(da, GTK_ALIGN_CENTER);
    return da;
}

/* Invisible resize edges.
 *
 * These windows are undecorated, so there is no frame border to grab.  A
 * GtkEventBox with visible_window = FALSE gets an INPUT-ONLY X window: it
 * receives the button press but paints absolutely nothing, so the edges add no
 * border, no padding and no visible change whatsoever.  They are placed with a
 * GtkOverlay, which does not affect the layout of the widget underneath.
 */
static gboolean xc_edge_press(GtkWidget* w, GdkEventButton* e, gpointer d) {
    XxriChrome* c = d;
    if (e->button != 1) return FALSE;
    GdkWindowEdge edge = (GdkWindowEdge)GPOINTER_TO_INT(
        g_object_get_data(G_OBJECT(w), "xxri-edge"));
    gtk_window_begin_resize_drag(GTK_WINDOW(c->win), edge, e->button,
                                 (gint)e->x_root, (gint)e->y_root, e->time);
    return TRUE;
}

static GtkWidget* xc_edge(XxriChrome* c, GdkWindowEdge edge, int w, int h,
                          GtkAlign ha, GtkAlign va) {
    GtkWidget* eb = gtk_event_box_new();
    gtk_event_box_set_visible_window(GTK_EVENT_BOX(eb), FALSE);  /* input only */
    gtk_widget_set_size_request(eb, w, h);
    gtk_widget_set_halign(eb, ha);
    gtk_widget_set_valign(eb, va);
    gtk_widget_add_events(eb, GDK_BUTTON_PRESS_MASK);
    g_object_set_data(G_OBJECT(eb), "xxri-edge", GINT_TO_POINTER(edge));
    g_signal_connect(eb, "button-press-event", G_CALLBACK(xc_edge_press), c);
    return eb;
}

/* Wrap the window's root widget so the window can be resized from its edges.
   Returns the widget to put in the window instead of `root`. */
static GtkWidget* xxri_chrome_resizable(XxriChrome* c, GtkWidget* root) {
    const int T = 5, K = 14;          /* edge thickness, corner size */
    GtkWidget* ov = gtk_overlay_new();
    gtk_container_add(GTK_CONTAINER(ov), root);
    struct { GdkWindowEdge e; int w, h; GtkAlign ha, va; } E[] = {
        { GDK_WINDOW_EDGE_EAST,       T, -1, GTK_ALIGN_END,   GTK_ALIGN_FILL },
        { GDK_WINDOW_EDGE_WEST,       T, -1, GTK_ALIGN_START, GTK_ALIGN_FILL },
        { GDK_WINDOW_EDGE_SOUTH,     -1,  T, GTK_ALIGN_FILL,  GTK_ALIGN_END  },
        { GDK_WINDOW_EDGE_SOUTH_EAST, K,  K, GTK_ALIGN_END,   GTK_ALIGN_END  },
        { GDK_WINDOW_EDGE_SOUTH_WEST, K,  K, GTK_ALIGN_START, GTK_ALIGN_END  },
    };
    for (unsigned i = 0; i < G_N_ELEMENTS(E); i++)
        gtk_overlay_add_overlay(GTK_OVERLAY(ov),
            xc_edge(c, E[i].e, E[i].w, E[i].h, E[i].ha, E[i].va));
    return ov;
}

/* Make any widget behave as the window's drag handle. */
static void xxri_chrome_drag_area(XxriChrome* c, GtkWidget* w) {
    GtkWidget* ev = w;
    if (!gtk_widget_get_has_window(w)) {
        /* a plain box has no window of its own, so it cannot receive buttons */
        return;
    }
    gtk_widget_add_events(ev, GDK_BUTTON_PRESS_MASK);
    g_signal_connect(ev, "button-press-event", G_CALLBACK(xc_drag_press), c);
}
#endif
