/*
 * xxri-media.c - XXRI Media, the media player.
 *
 * Layout from assets/mockup/mockup4.svg, numbers in metrics.h. Playback is
 * MPlayer behind player.h.
 *
 *   rail    glass strip on the left: window controls, subtitles, menu
 *   video   the rest; MPlayer draws into it itself
 *   bar     floating translucent control panel over the video
 *
 * The bar has to be its own override-redirect window: MPlayer paints over
 * anything drawn in the video window, and sibling X windows don't blend.
 * A toplevel does, through xxri-compositor.
 */
#include <gtk/gtk.h>
#define GETTEXT_PACKAGE "xxri-media"
#include "xxri-i18n.h"
#include <gdk/gdkx.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include "metrics.h"
#include "player.h"
#include "xxri-dialog.h"
#include "xxri-theme.h"

typedef enum { HOT_NONE=0, HOT_CLOSE, HOT_MAX, HOT_MIN, HOT_CC, HOT_MENU } RailHot;
typedef enum { BH_NONE=0, BH_NOTE, BH_VOL, BH_PREV, BH_PLAY, BH_NEXT,
               BH_FULL, BH_OPEN, BH_SPEED, BH_SEEK } BarHot;

typedef struct {
    GtkWidget *win, *fixed, *rail, *video, *bar;
    Metrics    m;
    XmPlayer  *pl;
    gboolean   composited;
    RailHot    rail_hot;
    BarHot     bar_hot;
    gboolean   dragging_seek, dragging_vol;
    /* by hand: flwm is ICCCM-only, no EWMH fullscreen */
    gboolean   fullscreen;
    gboolean   bar_suppressed;   /* a modal dialog owns the screen */
    gboolean   need_raise;       /* something may have restacked the bar */
    int        laid_w, laid_h;   /* the size relayout() last worked from */
    GdkRectangle home;           /* where the window is meant to sit */
    /* pictures are drawn here, not by the engine; img set = a picture is shown */
    GdkPixbuf *img;
    char      *img_path;
    int        img_w, img_h;
    /* playable files next to the open one, in file manager order; what
     * Previous/Next walk */
    GPtrArray *folder;
    guint      folder_i;
    GdkRectangle restore;
    guint      tick;
} App;

/* ------------------------------------------------------------------ paint */

static void rgb(cairo_t *cr, unsigned v, double a) {
    cairo_set_source_rgba(cr, ((v>>16)&0xff)/255.0, ((v>>8)&0xff)/255.0,
                          (v&0xff)/255.0, a);
}

static void rrect(cairo_t *cr, double x, double y, double w, double h, double r) {
    if (r > w/2) r = w/2;
    if (r > h/2) r = h/2;
    cairo_new_sub_path(cr);
    cairo_arc(cr, x+w-r, y+r,   r, -M_PI/2, 0);
    cairo_arc(cr, x+w-r, y+h-r, r, 0,        M_PI/2);
    cairo_arc(cr, x+r,   y+h-r, r, M_PI/2,   M_PI);
    cairo_arc(cr, x+r,   y+r,   r, M_PI,     1.5*M_PI);
    cairo_close_path(cr);
}

/* clip to the window outline, so all outer corners share one radius */
static void window_clip(cairo_t *cr, App *a, double dx, double dy) {
    if (a->fullscreen) return;
    cairo_save(cr);
    cairo_translate(cr, dx, dy);
    rrect(cr, 0, 0, a->m.win_w, a->m.win_h, a->m.radius);
    cairo_restore(cr);
    cairo_clip(cr);
}

/* --------------------------------------------------------------- the rail */

/* the rail's glyph colour: ink on the light glass, near-white on the dark */
static void rail_ink(cairo_t *cr, double a) {
    if (xxri_theme_dark()) cairo_set_source_rgba(cr, 0.949, 0.949, 0.957, a);
    else rgb(cr, C_INK, a);
}
/* a hovered control's halo */
static void rail_hot(cairo_t *cr, double a) {
    if (xxri_theme_dark()) cairo_set_source_rgba(cr, 1, 1, 1, a);
    else cairo_set_source_rgba(cr, 0.11, 0.10, 0.14, a);
}

static void draw_ctl(cairo_t *cr, int which, double cx, double cy, double sz,
                     gboolean hot)
{
    const double r = sz / 2.0;
    if (hot) {
        rail_hot(cr, 0.10);
        cairo_arc(cr, cx, cy, r + sz*0.28, 0, 2*M_PI);
        cairo_fill(cr);
    }
    switch (which) {
    case 0:                                   /* close: blue circle */
        rgb(cr, C_CLOSE, 1.0);
        cairo_arc(cr, cx, cy, r, 0, 2*M_PI);
        cairo_fill(cr);
        break;
    case 1:                                   /* maximise: purple square */
        rgb(cr, C_MAX, 1.0);
        rrect(cr, cx-r, cy-r, sz, sz, sz*0.30);
        cairo_fill(cr);
        break;
    default:                                  /* minimise: magenta triangle, down */
        rgb(cr, C_MIN, 1.0);
        cairo_move_to(cr, cx-r,   cy-r*0.85);
        cairo_line_to(cr, cx+r,   cy-r*0.85);
        cairo_line_to(cr, cx,     cy+r);
        cairo_close_path(cr);
        cairo_fill(cr);
        break;
    }
}

static void draw_cc(cairo_t *cr, double x, double y, double w, double h)
{
    double lw = h * 0.10; if (lw < 1) lw = 1;
    cairo_set_line_width(cr, lw);
    rail_ink(cr, 0.92);
    rrect(cr, x+lw/2, y+lw/2, w-lw, h-lw, h*0.30);
    cairo_stroke(cr);
    /* the two rows of dashes the artwork puts inside */
    double ix = x + w*0.17, iw = w*0.66;
    double y1 = y + h*0.46, y2 = y + h*0.68, dh = h*0.10;
    cairo_set_line_width(cr, dh);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_move_to(cr, ix, y1);            cairo_line_to(cr, ix+iw*0.42, y1);
    cairo_move_to(cr, ix+iw*0.56, y1);    cairo_line_to(cr, ix+iw, y1);
    cairo_move_to(cr, ix, y2);            cairo_line_to(cr, ix+iw*0.26, y2);
    cairo_move_to(cr, ix+iw*0.40, y2);    cairo_line_to(cr, ix+iw*0.74, y2);
    cairo_stroke(cr);
}

static void draw_menu(cairo_t *cr, double x, double y, double w, double h)
{
    double lw = h * 0.12; if (lw < 1.4) lw = 1.4;
    cairo_set_line_width(cr, lw);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    rail_ink(cr, 0.92);
    for (int i = 0; i < 3; i++) {
        double yy = y + lw/2 + i * (h - lw) / 2.0;
        cairo_move_to(cr, x, yy); cairo_line_to(cr, x + w, yy);
    }
    cairo_stroke(cr);
}

static gboolean rail_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
    App *a = d; Metrics *m = &a->m;
    int h = gtk_widget_get_allocated_height(w);
    cairo_save(cr);
    window_clip(cr, a, 0, 0);
    /* same 0.83 glass as the Browser sidebar, white or (Dark) Settings'
       window colour; opaque without compositor */
    gboolean dk = xxri_theme_dark();
    if (a->composited) cairo_set_source_rgba(cr, dk ? 0.141 : 1, dk ? 0.129 : 1, dk ? 0.141 : 1, A_GLASS_A);
    else if (dk)       cairo_set_source_rgb(cr, 0.114, 0.106, 0.114);
    else               cairo_set_source_rgb(cr, 0.95, 0.95, 0.96);
    cairo_paint(cr);
    cairo_restore(cr);

    double cx = m->rail_w / 2.0;
    for (int i = 0; i < 3; i++)
        draw_ctl(cr, i, cx, m->ctl_y[i] + m->ctl_size/2.0, m->ctl_size,
                 a->rail_hot == (RailHot)(HOT_CLOSE + i));

    /* subtitle and menu buttons, measured from the bottom like the artwork */
    int cc_y   = h - (m->win_h - m->cc_y);
    int menu_y = h - (m->win_h - m->menu_y);
    if (a->rail_hot == HOT_CC || a->rail_hot == HOT_MENU) {
        int hy = (a->rail_hot == HOT_CC) ? cc_y : menu_y;
        int hh = (a->rail_hot == HOT_CC) ? m->cc_h : m->menu_h;
        rail_hot(cr, 0.08);
        rrect(cr, cx - m->cc_w*0.72, hy - hh*0.35,
              m->cc_w*1.44, hh*1.7, m->cc_w*0.3);
        cairo_fill(cr);
    }
    draw_cc(cr, cx - m->cc_w/2.0, cc_y, m->cc_w, m->cc_h);
    draw_menu(cr, cx - m->menu_w/2.0, menu_y, m->menu_w, m->menu_h);
    return TRUE;
}

/* ---------------------------------------------------------------- the bar */

static void tri_right(cairo_t *cr, double cx, double cy, double w, double h) {
    cairo_move_to(cr, cx - w/2, cy - h/2);
    cairo_line_to(cr, cx + w/2, cy);
    cairo_line_to(cr, cx - w/2, cy + h/2);
    cairo_close_path(cr);
}

static void draw_play(cairo_t *cr, App *a, double cx, double cy)
{
    Metrics *m = &a->m;
    double h = m->play_h, w = h * 0.86;
    cairo_set_source_rgba(cr, 1, 1, 1, 0.97);
    if (xm_has_media(a->pl) && !xm_paused(a->pl) && !xm_ended(a->pl)) {  /* pause: two bars */
        double bw = w * 0.30, gap = w * 0.28;
        rrect(cr, cx - gap/2 - bw, cy - h/2, bw, h, bw*0.22); cairo_fill(cr);
        rrect(cr, cx + gap/2,      cy - h/2, bw, h, bw*0.22); cairo_fill(cr);
    } else {
        tri_right(cr, cx + w*0.08, cy, w, h);
        cairo_fill(cr);
    }
}

/* the artwork's skip controls: two triangles side by side */
static void draw_skip(cairo_t *cr, App *a, double cx, double cy, int dir)
{
    Metrics *m = &a->m;
    double h = m->step_h, w = m->step_w * 0.46, gap = w * 0.12;
    cairo_set_source_rgba(cr, 1, 1, 1, 0.95);
    for (int i = 0; i < 2; i++) {
        double x = cx + dir * ((i ? 1 : -1) * (w/2 + gap/2));
        cairo_save(cr);
        cairo_translate(cr, x, cy);
        if (dir < 0) cairo_scale(cr, -1, 1);
        tri_right(cr, 0, 0, w, h);
        cairo_restore(cr);
        cairo_fill(cr);
    }
}

static void draw_note(cairo_t *cr, App *a, double cx, double cy, gboolean muted)
{
    Metrics *m = &a->m;
    double h = m->note_h, r = h * 0.21;
    cairo_set_source_rgba(cr, 1, 1, 1, muted ? 0.42 : 0.95);
    cairo_set_line_width(cr, h * 0.11);
    cairo_move_to(cr, cx + r*0.9, cy + h/2 - r);
    cairo_line_to(cr, cx + r*0.9, cy - h/2);
    cairo_line_to(cr, cx + h*0.30, cy - h/2 + h*0.14);
    cairo_stroke(cr);
    cairo_arc(cr, cx, cy + h/2 - r, r, 0, 2*M_PI);
    cairo_fill(cr);
    if (muted) {                       /* struck through when silent */
        cairo_set_source_rgba(cr, 1, 1, 1, 0.95);
        cairo_set_line_width(cr, h * 0.11);
        cairo_move_to(cr, cx - h*0.42, cy + h*0.46);
        cairo_line_to(cr, cx + h*0.52, cy - h*0.46);
        cairo_stroke(cr);
    }
}

/* screen-in-screen, the artwork's glyph for filling the display */
static void draw_full(cairo_t *cr, App *a, double cx, double cy)
{
    Metrics *m = &a->m;
    double h = m->full_h, w = h * 1.30, lw = h * 0.115;
    cairo_set_source_rgba(cr, 1, 1, 1, 0.95);
    cairo_set_line_width(cr, lw);
    rrect(cr, cx - w/2 + lw/2, cy - h/2 + lw/2, w*0.74 - lw, h*0.72 - lw, h*0.22);
    cairo_stroke(cr);
    rrect(cr, cx - w/2 + w*0.40, cy - h/2 + h*0.34, w*0.60, h*0.66, h*0.22);
    cairo_fill(cr);
}

/* a document with an arrow into it: open */
static void draw_open(cairo_t *cr, App *a, double cx, double cy)
{
    Metrics *m = &a->m;
    double h = m->open_h, w = h * 0.82, lw = h * 0.10;
    cairo_set_source_rgba(cr, 1, 1, 1, 0.95);
    cairo_set_line_width(cr, lw);
    /* the sheet */
    rrect(cr, cx - w/2 + lw/2, cy - h/2 + lw/2, w - lw, h - lw, h*0.10);
    cairo_stroke(cr);
    /* the two ruled lines at its foot */
    cairo_set_line_width(cr, lw*0.9);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_move_to(cr, cx - w*0.28, cy + h*0.22);
    cairo_line_to(cr, cx + w*0.28, cy + h*0.22);
    cairo_move_to(cr, cx - w*0.18, cy + h*0.36);
    cairo_line_to(cr, cx + w*0.18, cy + h*0.36);
    cairo_stroke(cr);
    /* the arrow coming down into it */
    double ay = cy - h*0.44, ah = h*0.46, aw = w*0.34;
    cairo_set_line_width(cr, lw);
    cairo_move_to(cr, cx, ay); cairo_line_to(cr, cx, ay + ah*0.55);
    cairo_stroke(cr);
    cairo_move_to(cr, cx - aw/2, ay + ah*0.40);
    cairo_line_to(cr, cx,        ay + ah);
    cairo_line_to(cr, cx + aw/2, ay + ah*0.40);
    cairo_close_path(cr);
    cairo_fill(cr);
}

static void draw_chevrons(cairo_t *cr, App *a, double cx, double cy)
{
    Metrics *m = &a->m;
    double h = m->speed_h, lw = h * 0.16, gap = h * 0.34;
    cairo_set_source_rgba(cr, 1, 1, 1, 0.95);
    cairo_set_line_width(cr, lw);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    for (int i = 0; i < 2; i++) {
        double x = cx - gap/2 + i*gap;
        cairo_move_to(cr, x - h*0.22, cy - h*0.40);
        cairo_line_to(cr, x + h*0.22, cy);
        cairo_line_to(cr, x - h*0.22, cy + h*0.40);
        cairo_stroke(cr);
    }
}

/* the artwork's clock: four groups, hours:minutes:seconds:hundredths */
static void fmt_time(char *buf, gsize n, double t)
{
    if (t < 0 || !(t == t)) t = 0;
    int total = (int)t;
    int hh = total / 3600, mm = (total / 60) % 60, ss = total % 60;
    int ff = (int)((t - total) * 100);
    g_snprintf(buf, n, "%02d:%02d:%02d:%02d", hh, mm, ss, ff);
}

/* grey AA, not subpixel: the text goes into a translucent group the compositor
 * blends, and subpixel fringes came out orange/teal */
static void bar_font(cairo_t *cr, App *a, double px, gboolean bold)
{
    cairo_font_options_t *o = cairo_font_options_create();
    cairo_font_options_set_antialias(o, CAIRO_ANTIALIAS_GRAY);
    cairo_font_options_set_hint_style(o, CAIRO_HINT_STYLE_SLIGHT);
    cairo_set_font_options(cr, o);
    cairo_font_options_destroy(o);
    cairo_select_font_face(cr, "DejaVu Sans", CAIRO_FONT_SLANT_NORMAL,
                           bold ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, px);
}

static void bar_text(cairo_t *cr, App *a, double x, double cy,
                     const char *s, gboolean right_align)
{
    bar_font(cr, a, a->m.time_px, FALSE);
    cairo_text_extents_t e; cairo_text_extents(cr, s, &e);
    double tx = right_align ? x - e.width - e.x_bearing : x - e.x_bearing;
    cairo_set_source_rgba(cr, 1, 1, 1, 0.95);
    cairo_move_to(cr, tx, cy + a->m.time_px * 0.36);
    cairo_show_text(cr, s);
}

static void draw_slider(cairo_t *cr, App *a, double x0, double x1, double cy,
                        double frac, gboolean show_rest)
{
    Metrics *m = &a->m;
    double th = m->track_h, k = m->knob;
    if (frac < 0) frac = 0; if (frac > 1) frac = 1;
    double kx = x0 + (x1 - x0) * frac;
    if (show_rest) {                       /* what is left to play */
        cairo_set_source_rgba(cr, 1, 1, 1, 0.42);
        rrect(cr, x0, cy - th/2, x1 - x0, th, th/2);
        cairo_fill(cr);
    }
    cairo_set_source_rgba(cr, 1, 1, 1, 0.97);
    rrect(cr, x0, cy - th/2, kx - x0, th, th/2);
    cairo_fill(cr);
    rrect(cr, kx - k/2, cy - k/2, k, k, k*0.32);
    cairo_fill(cr);
}

static gboolean bar_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
    App *a = d; Metrics *m = &a->m;
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, 0, 0, 0, 0);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    rrect(cr, 0, 0, m->bar_w, m->bar_h, m->bar_r);
    if (a->composited) cairo_set_source_rgba(cr, 1, 1, 1, A_BAR_A);
    else               cairo_set_source_rgba(cr, 0.42, 0.47, 0.52, 0.96);
    cairo_fill(cr);

    double r1 = m->row1_cy, r2 = m->row2_cy;
    gboolean has = xm_has_media(a->pl) || a->img != NULL;
    double alpha_off = has ? 1.0 : 0.55;
    cairo_push_group(cr);

    draw_note(cr, a, m->note_cx, r1, xm_muted(a->pl));
    draw_slider(cr, a, m->vol_x0, m->vol_x1, r1,
                xm_muted(a->pl) ? 0 : xm_volume(a->pl), FALSE);
    draw_skip(cr, a, m->prev_cx, r1, -1);
    draw_play(cr, a, m->play_cx, r1);
    draw_skip(cr, a, m->next_cx, r1, +1);
    draw_full(cr, a, m->full_cx, r1);
    draw_open(cr, a, m->open_cx, r1);
    draw_chevrons(cr, a, m->speed_cx, r1);

    char t0[32], t1[32];
    if (a->img) {
        /* pictures: position in the folder and the size where the times go,
         * and the seek track scrubs through the folder */
        guint n = a->folder ? a->folder->len : 1;
        g_snprintf(t0, sizeof t0, "%u / %u", a->folder_i + 1, n);
        g_snprintf(t1, sizeof t1, "%d x %d", a->img_w, a->img_h);
        bar_text(cr, a, m->time_x0, r2, t0, FALSE);
        bar_text(cr, a, m->time_x1, r2, t1, TRUE);
        draw_slider(cr, a, m->seek_x0, m->seek_x1, r2,
                    n > 1 ? a->folder_i / (double)(n - 1) : 0, TRUE);
    } else {
        fmt_time(t0, sizeof t0, xm_position(a->pl));
        fmt_time(t1, sizeof t1, xm_duration(a->pl));
        bar_text(cr, a, m->time_x0, r2, t0, FALSE);
        bar_text(cr, a, m->time_x1, r2, t1, TRUE);
        double dur = xm_duration(a->pl);
        draw_slider(cr, a, m->seek_x0, m->seek_x1, r2,
                    dur > 0 ? xm_position(a->pl) / dur : 0, TRUE);
    }

    cairo_pop_group_to_source(cr);
    cairo_paint_with_alpha(cr, alpha_off);

    /* speed label when not 1x */
    if (fabs(xm_speed(a->pl) - 1.0) > 0.01) {
        char s[16]; g_snprintf(s, sizeof s, "%gx", xm_speed(a->pl));
        bar_font(cr, a, m->time_px * 0.85, TRUE);
        cairo_text_extents_t e; cairo_text_extents(cr, s, &e);
        cairo_set_source_rgba(cr, 1, 1, 1, 0.95);
        cairo_move_to(cr, m->speed_cx - e.width/2 - e.x_bearing,
                      r1 + m->speed_h*0.55 + m->time_px*0.8);
        cairo_show_text(cr, s);
    }
    return TRUE;
}

/* --------------------------------------------------------------- the video */

static gboolean video_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
    App *a = d; Metrics *m = &a->m;
    /* the engine owns these pixels while it runs; painting here flickers */
    if (a->pl && xm_engine_running(a->pl))
        return TRUE;
    int W = gtk_widget_get_allocated_width(w), H = gtk_widget_get_allocated_height(w);
    cairo_save(cr);
    window_clip(cr, a, -m->rail_w, 0);
    cairo_set_source_rgb(cr, 0.055, 0.055, 0.060);
    cairo_paint(cr);
    cairo_restore(cr);
    if (a->img) {
        /* fit and centre, like the engine's -zoom does for video */
        double iw = a->img_w, ih = a->img_h;
        double sc = MIN(W / iw, H / ih);
        double dw = iw * sc, dh = ih * sc;
        cairo_save(cr);
        window_clip(cr, a, -m->rail_w, 0);
        cairo_translate(cr, (W - dw) / 2.0, (H - dh) / 2.0);
        cairo_scale(cr, sc, sc);
        gdk_cairo_set_source_pixbuf(cr, a->img, 0, 0);
        cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_GOOD);
        cairo_paint(cr);
        cairo_restore(cr);
        return TRUE;
    }
    if (!xm_has_media(a->pl)) {
        const char *msg = _("Open a file to play");
        bar_font(cr, a, a->m.time_px * 1.25, FALSE);
        cairo_text_extents_t e; cairo_text_extents(cr, msg, &e);
        cairo_set_source_rgba(cr, 1, 1, 1, 0.34);
        cairo_move_to(cr, W/2.0 - e.width/2 - e.x_bearing,
                      H/2.0 - a->m.bar_h);
        cairo_show_text(cr, msg);
    }
    return TRUE;
}

/* ------------------------------------------------------------- geometry */

static void place_bar(App *a)
{
    if (!a->bar || !gtk_widget_get_realized(a->win)) return;
    Metrics *m = &a->m;
    int ww, wh, wx, wy;
    gtk_window_get_size(GTK_WINDOW(a->win), &ww, &wh);
    gdk_window_get_origin(gtk_widget_get_window(a->win), &wx, &wy);
    /* centred, the artwork's distance off the bottom */
    int bx = wx + (ww - m->bar_w) / 2;
    int by = wy + wh - m->bar_bottom - m->bar_h;
    gtk_window_move(GTK_WINDOW(a->bar), bx, by);
    gtk_widget_set_size_request(a->bar, m->bar_w, m->bar_h);
    gdk_window_raise(gtk_widget_get_window(a->bar));
}

/*
 * MPlayer moves and resizes the -wid window to suit each file (a 320x240 clip
 * covered the rail), and GTK doesn't notice. So the geometry is re-asserted
 * from the 250ms tick. A clipping parent window doesn't work: xxri-compositor
 * places native child windows at their own coordinates.
 */
static void place_engine_window(App *a, int rail, int vw, int vh)
{
    GdkWindow *vwin = a->video ? gtk_widget_get_window(a->video) : NULL;
    if (!vwin) return;
    vw = MAX(1, vw); vh = MAX(1, vh);
    int cx, cy, cw = gdk_window_get_width(vwin), ch = gdk_window_get_height(vwin);
    gdk_window_get_position(vwin, &cx, &cy);
    if (cx == rail && cy == 0 && cw == vw && ch == vh) return;
    gdk_window_move_resize(vwin, rail, 0, vw, vh);
}

static void relayout(App *a)
{
    Metrics *m = &a->m;
    /* the allocation, not gtk_window_get_size: that still reports the old size
     * until the WM answers a resize (fullscreen left a black margin) */
    int ww = gtk_widget_get_allocated_width(a->win);
    int wh = gtk_widget_get_allocated_height(a->win);
    if (ww < 2 || wh < 2) gtk_window_get_size(GTK_WINDOW(a->win), &ww, &wh);
    int rail = a->fullscreen ? 0 : m->rail_w;
    if (!a->fullscreen)
        gtk_widget_set_size_request(a->rail, rail, wh);
    gtk_widget_set_visible(a->rail, !a->fullscreen);
    gtk_fixed_move(GTK_FIXED(a->fixed), a->video, rail, 0);
    gtk_widget_set_size_request(a->video, ww - rail, wh);
    /* not in GTK's layout; MPlayer refits to the ConfigureNotify */
    place_engine_window(a, rail, ww - rail, wh);
    place_bar(a);
}

static void set_fullscreen(App *a, gboolean on)
{
    if (a->fullscreen == on) return;
    GdkRectangle sc = { 0, 0, 1024, 768 };
    GdkDisplay *dpy = gtk_widget_get_display(a->win);
    GdkMonitor *mon = gdk_display_get_monitor_at_window(dpy,
                          gtk_widget_get_window(a->win));
    if (!mon && gdk_display_get_n_monitors(dpy) > 0)
        mon = gdk_display_get_monitor(dpy, 0);
    if (mon) gdk_monitor_get_geometry(mon, &sc);
    /* drop the size requests first, they are a floor: leaving fullscreen
     * did nothing. The next size-allocate sets proper ones again. */
    gtk_widget_set_size_request(a->video, 1, 1);
    gtk_widget_set_size_request(a->rail, 1, 1);

    if (on) {
        int wx, wy, ww, wh;
        gtk_window_get_position(GTK_WINDOW(a->win), &wx, &wy);
        gtk_window_get_size(GTK_WINDOW(a->win), &ww, &wh);
        a->restore = (GdkRectangle){ wx, wy, ww, wh };
        a->fullscreen = TRUE;
        gtk_window_resize(GTK_WINDOW(a->win), sc.width, sc.height);
        /* move after the resize, flwm lost a move sent first */
        gtk_window_move(GTK_WINDOW(a->win), sc.x, sc.y);
    } else {
        a->fullscreen = FALSE;
        gtk_window_resize(GTK_WINDOW(a->win), a->restore.width, a->restore.height);
        gtk_window_move(GTK_WINDOW(a->win), a->restore.x, a->restore.y);
    }
    /* no relayout here: it would put the old-size floor right back. The
     * size-allocate with the new size does it. */
    a->laid_w = a->laid_h = 0;         /* force the next size-allocate to act */
    gtk_widget_set_visible(a->rail, !a->fullscreen);
    gtk_widget_queue_draw(a->win);
}

/* --------------------------------------------------------------- actions */

/* redraw the bar, never the video while the engine runs (black flicker) */
static void refresh(gpointer user)
{
    App *a = user;
    if (a->bar) gtk_widget_queue_draw(a->bar);
    if (a->video && !xm_engine_running(a->pl))
        gtk_widget_queue_draw(a->video);
}

static void message(App *a, const char *heading, const char *detail)
{
    GtkWidget *d = gtk_message_dialog_new(GTK_WINDOW(a->win),
        GTK_DIALOG_MODAL, GTK_MESSAGE_OTHER, GTK_BUTTONS_OK, "%s", heading);
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(d), "%s", detail);
    gtk_window_set_decorated(GTK_WINDOW(d), FALSE);
    gtk_style_context_add_class(gtk_widget_get_style_context(d), "xxri-chooser");
    gtk_window_set_position(GTK_WINDOW(d), GTK_WIN_POS_CENTER_ON_PARENT);
    a->bar_suppressed = TRUE;
    if (a->bar) gtk_widget_hide(a->bar);
    gtk_dialog_run(GTK_DIALOG(d));
    gtk_widget_destroy(d);
    a->bar_suppressed = FALSE;
    a->need_raise = TRUE;
    if (a->bar) { gtk_widget_show(a->bar); place_bar(a); }
    refresh(a);
}

/* ---- media types
 * One list for the chooser filter, the folder walk and picture-vs-engine.
 * Audio/video by extension (what MPlayer plays); pictures are whatever
 * gdk-pixbuf can load here, asked at startup rather than hard-coded. */
static const char *const AV_EXT[] = {
    "mp4","mkv","webm","avi","mov","m4v","mpg","mpeg","ogv","wmv","flv","3gp",
    "mp3","wav","ogg","oga","flac","m4a","opus","aac","wma", NULL };

static GHashTable *image_ext;          /* extension -> itself, lower case */

static void init_image_ext(void)
{
    image_ext = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    GSList *fmts = gdk_pixbuf_get_formats();
    for (GSList *l = fmts; l; l = l->next) {
        char **ext = gdk_pixbuf_format_get_extensions(l->data);
        for (int i = 0; ext && ext[i]; i++)
            g_hash_table_add(image_ext, g_ascii_strdown(ext[i], -1));
        g_strfreev(ext);
    }
    g_slist_free(fmts);
}

/* lower case, no dot, "" if none */
static char *ext_of(const char *path)
{
    const char *base = strrchr(path, '/');
    const char *dot  = strrchr(base ? base : path, '.');
    return dot && dot[1] ? g_ascii_strdown(dot + 1, -1) : g_strdup("");
}

static gboolean is_image_file(const char *path)
{
    char *e = ext_of(path);
    gboolean r = *e && image_ext && g_hash_table_contains(image_ext, e);
    g_free(e);
    return r;
}

static gboolean is_av_file(const char *path)
{
    char *e = ext_of(path);
    gboolean r = FALSE;
    for (int i = 0; !r && AV_EXT[i]; i++) r = (g_strcmp0(e, AV_EXT[i]) == 0);
    g_free(e);
    return r;
}

static gboolean is_media_file(const char *path)
{
    return is_av_file(path) || is_image_file(path);
}

static char *abs_path(const char *p)
{
    char buf[PATH_MAX];
    if (realpath(p, buf)) return g_strdup(buf);
    if (g_path_is_absolute(p)) return g_strdup(p);
    char *cwd = g_get_current_dir(), *r = g_build_filename(cwd, p, NULL);
    g_free(cwd);
    return r;
}

/* file manager order */
static gint cmp_media(gconstpointer pa, gconstpointer pb)
{
    char *na = g_path_get_basename(*(char *const *)pa);
    char *nb = g_path_get_basename(*(char *const *)pb);
    char *ka = g_utf8_collate_key_for_filename(na, -1);
    char *kb = g_utf8_collate_key_for_filename(nb, -1);
    int r = strcmp(ka, kb);
    g_free(na); g_free(nb); g_free(ka); g_free(kb);
    return r;
}

static void scan_folder(App *a, const char *path)
{
    if (a->folder) g_ptr_array_free(a->folder, TRUE);
    a->folder = g_ptr_array_new_with_free_func(g_free);
    a->folder_i = 0;

    char *dir = g_path_get_dirname(path);
    GDir *d = g_dir_open(dir, 0, NULL);
    if (d) {
        const char *n;
        while ((n = g_dir_read_name(d))) {
            char *f = g_build_filename(dir, n, NULL);
            if (is_media_file(f) && g_file_test(f, G_FILE_TEST_IS_REGULAR))
                g_ptr_array_add(a->folder, f);
            else
                g_free(f);
        }
        g_dir_close(d);
    }
    g_free(dir);
    g_ptr_array_sort(a->folder, cmp_media);

    for (guint i = 0; i < a->folder->len; i++)
        if (g_strcmp0(g_ptr_array_index(a->folder, i), path) == 0) {
            a->folder_i = i;
            break;
        }
}

static void drop_image(App *a)
{
    if (a->img) { g_object_unref(a->img); a->img = NULL; }
    g_clear_pointer(&a->img_path, g_free);
}

/* the file on screen, either kind (Save a Copy) */
static const char *current_path(App *a)
{
    return a->img_path ? a->img_path : xm_current_file(a->pl);
}

static gboolean show_image(App *a, const char *path)
{
    GError *err = NULL;
    GdkPixbuf *pb = gdk_pixbuf_new_from_file(path, &err);
    if (!pb) {
        char *base = g_path_get_basename(path);
        message(a, _("Could not open the picture"),
                err && err->message ? err->message : base);
        g_free(base);
        g_clear_error(&err);
        return FALSE;
    }
    /* stop the engine first, it paints over anything in the video area */
    xm_quit(a->pl);
    drop_image(a);
    a->img = pb;
    a->img_path = g_strdup(path);
    a->img_w = gdk_pixbuf_get_width(pb);
    a->img_h = gdk_pixbuf_get_height(pb);
    return TRUE;
}

/* show one file; the folder list stays as it is */
static void present(App *a, const char *path)
{
    if (is_image_file(path)) {
        show_image(a, path);
    } else {
        drop_image(a);
        xm_open(a->pl, path);
    }
    a->need_raise = TRUE;
    if (a->video) gtk_widget_queue_draw(a->video);
    if (a->bar)   gtk_widget_queue_draw(a->bar);
}

/* command line, chooser and file manager all open through here */
static void open_path(App *a, const char *path)
{
    char *abs = abs_path(path);
    scan_folder(a, abs);
    present(a, abs);
    g_free(abs);
}

static void step_media(App *a, int dir)
{
    if (!a->folder || a->folder->len < 2) return;
    a->folder_i = (a->folder_i + a->folder->len + dir) % a->folder->len;
    present(a, g_ptr_array_index(a->folder, a->folder_i));
}

static void goto_media(App *a, guint i)
{
    if (!a->folder || i >= a->folder->len || i == a->folder_i) return;
    a->folder_i = i;
    present(a, g_ptr_array_index(a->folder, a->folder_i));
}

/* both file dialogs, styled like XXRI Files by xxri-media.css */
static GtkWidget *xxri_chooser(App *a, GtkFileChooserAction act,
                               const char *title, const char *accept)
{
    GtkWidget *d = gtk_file_chooser_dialog_new(title, GTK_WINDOW(a->win), act,
        _("Cancel"), GTK_RESPONSE_CANCEL, accept, GTK_RESPONSE_ACCEPT, NULL);

    GtkWidget *ok = gtk_dialog_get_widget_for_response(GTK_DIALOG(d),
                                                       GTK_RESPONSE_ACCEPT);
    if (ok) gtk_style_context_add_class(gtk_widget_get_style_context(ok),
                                        "suggested-action");

    gtk_window_set_decorated(GTK_WINDOW(d), FALSE);
    gtk_window_set_modal(GTK_WINDOW(d), TRUE);
    gtk_style_context_add_class(gtk_widget_get_style_context(d), "xxri-chooser");

    GtkWidget *ca  = gtk_dialog_get_content_area(GTK_DIALOG(d));
    GtkWidget *hdr = gtk_label_new(title);
    gtk_widget_set_name(hdr, "xxri-dlg-title");
    gtk_widget_set_halign(hdr, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(ca), hdr, FALSE, FALSE, 0);
    gtk_box_reorder_child(GTK_BOX(ca), hdr, 0);
    gtk_widget_show(hdr);

    GtkFileFilter *f = gtk_file_filter_new();
    gtk_file_filter_set_name(f, _("Media files"));
    for (int i = 0; AV_EXT[i]; i++) {
        char *g = g_strconcat("*.", AV_EXT[i], NULL);
        gtk_file_filter_add_pattern(f, g);
        g_free(g);
    }
    if (image_ext) {
        GList *keys = g_hash_table_get_keys(image_ext);
        for (GList *l = keys; l; l = l->next) {
            char *g = g_strconcat("*.", (const char *)l->data, NULL);
            gtk_file_filter_add_pattern(f, g);
            g_free(g);
        }
        g_list_free(keys);
    }
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(d), f);
    GtkFileFilter *all = gtk_file_filter_new();
    gtk_file_filter_set_name(all, _("All files"));
    gtk_file_filter_add_pattern(all, "*");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(d), all);
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(d), g_get_home_dir());

    /* cap the size with a geometry hint, the WM applies it at map time.
     * A size request is only a floor (natural size 1096x822 hid the buttons
     * on 768 lines), and a resize after show left it mapped but undrawn. */
    int ww, wh;
    gtk_window_get_size(GTK_WINDOW(a->win), &ww, &wh);
    int dw = ww < 620 ? ww : 620, dh = wh < 400 ? wh : 400;
    GdkGeometry geo;
    geo.min_width = 380;  geo.min_height = 240;
    geo.max_width = dw;   geo.max_height = dh;
    gtk_window_set_geometry_hints(GTK_WINDOW(d), NULL, &geo,
                                  GDK_HINT_MIN_SIZE | GDK_HINT_MAX_SIZE);
    gtk_window_set_default_size(GTK_WINDOW(d), dw, dh);
    gtk_window_set_position(GTK_WINDOW(d), GTK_WIN_POS_CENTER_ON_PARENT);
    return d;
}

/* returns the chosen path or NULL. Never show_all() a chooser: it keeps
 * widgets hidden on purpose and stops mapping */
static char *xxri_chooser_run(App *a, GtkWidget *d)
{
    char *fn = NULL;
    a->bar_suppressed = TRUE;          /* the bar would sit over the chooser */
    if (a->bar) gtk_widget_hide(a->bar);
    gtk_widget_show(d);
    if (gtk_dialog_run(GTK_DIALOG(d)) == GTK_RESPONSE_ACCEPT)
        fn = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(d));
    gtk_widget_destroy(d);
    a->bar_suppressed = FALSE;
    a->need_raise = TRUE;
    if (a->bar) { gtk_widget_show(a->bar); place_bar(a); }
    refresh(a);
    return fn;
}

/*
 * Raise and focus by hand: flwm is ICCCM-only and ignores _NET_ACTIVE_WINDOW,
 * so opened from Files the player mapped behind it. Undecorated means no frame
 * for flwm to raise or focus on a click either, so this is also wired to any
 * press in the window.
 */
static void raise_self(App *a)
{
    GdkWindow *w = gtk_widget_get_window(a->win);
    if (!w) return;
    gdk_window_raise(w);
    gtk_window_present_with_time(GTK_WINDOW(a->win), GDK_CURRENT_TIME);
    /* without focus the shortcuts died and the bar hid on the first click */
    if (gdk_window_is_viewable(w))
        XSetInputFocus(GDK_WINDOW_XDISPLAY(w), GDK_WINDOW_XID(w),
                       RevertToPointerRoot, CurrentTime);
    a->need_raise = TRUE;               /* the bar must follow the window up */
}

static gboolean raise_once(gpointer d)
{
    App *a = d;
    raise_self(a);
    /* re-send the position flwm applies after mapping, see unsmear() */
    gtk_window_move(GTK_WINDOW(a->win), a->home.x, a->home.y);
    return G_SOURCE_REMOVE;
}

/*
 * flwm maps first and positions after, so the engine's first frames land at
 * the screen corner and the compositor never hears about the strip the window
 * then leaves: a band of video stays on the desktop. Move a pixel and back
 * (an unchanged position sends no ConfigureNotify, so no damage), on separate
 * main loop turns or the two moves cancel out. Once is enough.
 */
static gboolean unsmear_back(gpointer d)
{
    App *a = d;
    gtk_window_move(GTK_WINDOW(a->win), a->home.x, a->home.y);
    /* raise again: Files can take the focus back after the 120ms raise */
    raise_self(a);
    return G_SOURCE_REMOVE;
}

static gboolean unsmear(gpointer d)
{
    App *a = d;
    gtk_window_move(GTK_WINDOW(a->win), a->home.x, a->home.y + 1);
    g_timeout_add(220, unsmear_back, a);
    return G_SOURCE_REMOVE;
}

static void open_dialog(App *a)
{
    GtkWidget *d = xxri_chooser(a, GTK_FILE_CHOOSER_ACTION_OPEN,
                                _("Open Media"), _("Open"));
    char *fn = xxri_chooser_run(a, d);
    if (fn) { open_path(a, fn); g_free(fn); }
}

/* Save a Copy: a plain file copy, no re-encoding */
static void save_dialog(App *a)
{
    const char *src = current_path(a);
    if (!src || !*src) {
        message(a, _("Nothing to save"), _("Open a media file first."));
        return;
    }
    GtkWidget *d = xxri_chooser(a, GTK_FILE_CHOOSER_ACTION_SAVE,
                                _("Save As"), _("Save"));
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(d), TRUE);
    char *base = g_path_get_basename(src);
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(d), base);
    g_free(base);

    char *dst = xxri_chooser_run(a, d);
    if (!dst) return;
    if (g_strcmp0(dst, src) == 0) { g_free(dst); return; }

    GFile  *gs = g_file_new_for_path(src), *gd = g_file_new_for_path(dst);
    GError *err = NULL;
    gboolean ok = g_file_copy(gs, gd, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL,
                              &err);
    if (!ok) {
        message(a, _("Could not save"),
                err && err->message ? err->message : _("The copy failed."));
        g_clear_error(&err);
    }
    g_object_unref(gs); g_object_unref(gd); g_free(dst);
}

static void speed_chosen(GtkMenuItem *it, gpointer d)
{
    App *a = g_object_get_data(G_OBJECT(it), "app");
    double v = *(double*)d;
    xm_set_speed(a->pl, v);
}

static void speed_menu(App *a)
{
    static double sp[] = { 0.5, 0.75, 1.0, 1.25, 1.5, 2.0 };
    GtkWidget *menu = gtk_menu_new();
    for (guint i = 0; i < G_N_ELEMENTS(sp); i++) {
        char lbl[48];
        if (fabs(sp[i] - 1.0) < 0.001) g_strlcpy(lbl, _("Normal"), sizeof lbl);
        else g_snprintf(lbl, sizeof lbl, "%gx", sp[i]);
        GtkWidget *it = gtk_check_menu_item_new_with_label(lbl);
        gtk_check_menu_item_set_draw_as_radio(GTK_CHECK_MENU_ITEM(it), TRUE);
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(it),
                                       fabs(xm_speed(a->pl) - sp[i]) < 0.001);
        g_object_set_data(G_OBJECT(it), "app", a);
        g_signal_connect(it, "activate", G_CALLBACK(speed_chosen), &sp[i]);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), it);
    }
    gtk_widget_show_all(menu);
    gtk_menu_popup_at_pointer(GTK_MENU(menu), NULL);
}

static void on_menu_open(GtkMenuItem *it, gpointer d) { open_dialog((App*)d); }
static void on_menu_save(GtkMenuItem *it, gpointer d) { save_dialog((App*)d); }
static void on_menu_sub (GtkMenuItem *it, gpointer d) { xm_cycle_subtitles(((App*)d)->pl); }
static void on_menu_full(GtkMenuItem *it, gpointer d) { App*a=d; set_fullscreen(a, !a->fullscreen); }
static void on_menu_quit(GtkMenuItem *it, gpointer d) { gtk_main_quit(); }

static void main_menu(App *a)
{
    GtkWidget *menu = gtk_menu_new();
    struct { const char *l; GCallback cb; } items[] = {
        { _("Open File…"), G_CALLBACK(on_menu_open) },
        { _("Save a Copy…"), G_CALLBACK(on_menu_save) },
        { _("Subtitles: next track"), G_CALLBACK(on_menu_sub) },
        { _("Fullscreen"), G_CALLBACK(on_menu_full) },
        { NULL, NULL },
        { _("Quit XXRI Media"), G_CALLBACK(on_menu_quit) },
    };
    for (guint i = 0; i < G_N_ELEMENTS(items); i++) {
        GtkWidget *it = items[i].l ? gtk_menu_item_new_with_label(items[i].l)
                                   : gtk_separator_menu_item_new();
        if (items[i].l) g_signal_connect(it, "activate", items[i].cb, a);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), it);
    }
    gtk_widget_show_all(menu);
    gtk_menu_popup_at_pointer(GTK_MENU(menu), NULL);
}

/* ------------------------------------------------------------ rail input */

static RailHot rail_at(App *a, double x, double y, int h)
{
    Metrics *m = &a->m;
    double cx = m->rail_w / 2.0, r = m->ctl_size;
    for (int i = 0; i < 3; i++) {
        double cy = m->ctl_y[i] + m->ctl_size/2.0;
        if (fabs(x - cx) <= r && fabs(y - cy) <= r*0.8)
            return (RailHot)(HOT_CLOSE + i);
    }
    int cc_y   = h - (m->win_h - m->cc_y);
    int menu_y = h - (m->win_h - m->menu_y);
    if (fabs(x - cx) <= m->cc_w*0.8   && y >= cc_y - 4   && y <= cc_y + m->cc_h + 4)
        return HOT_CC;
    if (fabs(x - cx) <= m->menu_w*0.9 && y >= menu_y - 5 && y <= menu_y + m->menu_h + 5)
        return HOT_MENU;
    return HOT_NONE;
}

static gboolean rail_motion(GtkWidget *w, GdkEventMotion *e, gpointer d)
{
    App *a = d;
    RailHot h = rail_at(a, e->x, e->y, gtk_widget_get_allocated_height(w));
    if (h != a->rail_hot) { a->rail_hot = h; gtk_widget_queue_draw(w); }
    return FALSE;
}
static gboolean rail_leave(GtkWidget *w, GdkEvent *e, gpointer d)
{
    App *a = d;
    if (a->rail_hot) { a->rail_hot = HOT_NONE; gtk_widget_queue_draw(w); }
    return FALSE;
}

static gboolean rail_press(GtkWidget *w, GdkEventButton *e, gpointer d)
{
    App *a = d;
    if (e->button != 1) return FALSE;
    switch (rail_at(a, e->x, e->y, gtk_widget_get_allocated_height(w))) {
    case HOT_CLOSE: gtk_main_quit(); return TRUE;
    case HOT_MAX:   set_fullscreen(a, !a->fullscreen); return TRUE;
    case HOT_MIN:   gtk_window_iconify(GTK_WINDOW(a->win)); return TRUE;
    case HOT_CC:    xm_cycle_subtitles(a->pl); return TRUE;
    case HOT_MENU:  main_menu(a); return TRUE;
    default: break;
    }
    /* the rest of the rail drags the window */
    gtk_window_begin_move_drag(GTK_WINDOW(a->win), e->button,
                               e->x_root, e->y_root, e->time);
    return TRUE;
}

/* ------------------------------------------------------------- bar input */

static BarHot bar_at(App *a, double x, double y)
{
    Metrics *m = &a->m;
    double r1 = m->row1_cy, r2 = m->row2_cy;
    double pad = m->knob;
    if (fabs(y - r2) <= pad) {
        if (x >= m->seek_x0 - pad*0.6 && x <= m->seek_x1 + pad*0.6) return BH_SEEK;
        return BH_NONE;
    }
    if (fabs(y - r1) <= pad) {
        if (fabs(x - m->note_cx) <= m->note_h) return BH_NOTE;
        if (x >= m->vol_x0 - pad*0.6 && x <= m->vol_x1 + pad*0.6) return BH_VOL;
        if (fabs(x - m->prev_cx)  <= m->step_w*0.7) return BH_PREV;
        if (fabs(x - m->play_cx)  <= m->play_h*0.7) return BH_PLAY;
        if (fabs(x - m->next_cx)  <= m->step_w*0.7) return BH_NEXT;
        if (fabs(x - m->full_cx)  <= m->full_h*0.8) return BH_FULL;
        if (fabs(x - m->open_cx)  <= m->open_h*0.8) return BH_OPEN;
        if (fabs(x - m->speed_cx) <= m->speed_h*0.9) return BH_SPEED;
    }
    return BH_NONE;
}

static void apply_seek_at(App *a, double x)
{
    Metrics *m = &a->m;
    double f = (x - m->seek_x0) / (double)(m->seek_x1 - m->seek_x0);
    if (f < 0) f = 0; else if (f > 1) f = 1;
    if (a->img) {
        guint n = a->folder ? a->folder->len : 0;
        if (n > 1) goto_media(a, (guint)(f * (n - 1) + 0.5));
        return;
    }
    xm_seek_fraction(a->pl, f);
}
static void apply_vol_at(App *a, double x)
{
    Metrics *m = &a->m;
    double f = (x - m->vol_x0) / (double)(m->vol_x1 - m->vol_x0);
    xm_set_volume(a->pl, f);
}

static gboolean bar_press(GtkWidget *w, GdkEventButton *e, gpointer d)
{
    App *a = d;
    if (e->button != 1) return FALSE;
    switch (bar_at(a, e->x, e->y)) {
    case BH_NOTE:  xm_toggle_mute(a->pl); break;
    case BH_VOL:   a->dragging_vol = TRUE;  apply_vol_at(a, e->x); break;
    /* prev/next walk the folder; seeking is the track and arrow keys */
    case BH_PREV:  step_media(a, -1); break;
    case BH_PLAY:  if (!a->img) xm_toggle_pause(a->pl); break;
    case BH_NEXT:  step_media(a, +1); break;
    case BH_FULL:  set_fullscreen(a, !a->fullscreen); break;
    case BH_OPEN:  open_dialog(a); break;
    case BH_SPEED: speed_menu(a); break;
    case BH_SEEK:  a->dragging_seek = TRUE; apply_seek_at(a, e->x); break;
    default: break;
    }
    return TRUE;
}

static gboolean bar_release(GtkWidget *w, GdkEventButton *e, gpointer d)
{
    App *a = d;
    a->dragging_seek = a->dragging_vol = FALSE;
    return TRUE;
}

static gboolean bar_motion(GtkWidget *w, GdkEventMotion *e, gpointer d)
{
    App *a = d;
    if (a->dragging_seek) { apply_seek_at(a, e->x); return TRUE; }
    if (a->dragging_vol)  { apply_vol_at(a, e->x);  return TRUE; }
    BarHot h = bar_at(a, e->x, e->y);
    if (h != a->bar_hot) { a->bar_hot = h; gtk_widget_queue_draw(w); }
    return TRUE;
}

/* ----------------------------------------------------------- video input */

static gboolean video_press(GtkWidget *w, GdkEventButton *e, gpointer d)
{
    App *a = d;
    if (e->type == GDK_2BUTTON_PRESS && e->button == 1) {
        set_fullscreen(a, !a->fullscreen);
        return TRUE;
    }
    if (e->button == 1) { if (!a->img) xm_toggle_pause(a->pl); return TRUE; }
    if (e->button == 3) { main_menu(a); return TRUE; }
    return FALSE;
}

static gboolean on_key(GtkWidget *w, GdkEventKey *e, gpointer d)
{
    App *a = d;
    switch (e->keyval) {
    case GDK_KEY_space: case GDK_KEY_p:
        if (!a->img) xm_toggle_pause(a->pl);
        return TRUE;
    /* arrows seek in a film, step through the folder on a picture */
    case GDK_KEY_Right:
        if (a->img) step_media(a, +1);
        else xm_seek_relative(a->pl, e->state & GDK_SHIFT_MASK ? 60 : 10);
        return TRUE;
    case GDK_KEY_Left:
        if (a->img) step_media(a, -1);
        else xm_seek_relative(a->pl, e->state & GDK_SHIFT_MASK ? -60 : -10);
        return TRUE;
    case GDK_KEY_Page_Down: step_media(a, +1); return TRUE;
    case GDK_KEY_Page_Up:   step_media(a, -1); return TRUE;
    case GDK_KEY_Up:    xm_set_volume(a->pl, xm_volume(a->pl) + 0.05); return TRUE;
    case GDK_KEY_Down:  xm_set_volume(a->pl, xm_volume(a->pl) - 0.05); return TRUE;
    case GDK_KEY_m:     xm_toggle_mute(a->pl); return TRUE;
    case GDK_KEY_f:     set_fullscreen(a, !a->fullscreen); return TRUE;
    case GDK_KEY_Escape:
        if (a->fullscreen) { set_fullscreen(a, FALSE); return TRUE; }
        return FALSE;
    case GDK_KEY_o:
        if (e->state & GDK_CONTROL_MASK) { open_dialog(a); return TRUE; }
        return FALSE;
    case GDK_KEY_q:
        if (e->state & GDK_CONTROL_MASK) { gtk_main_quit(); return TRUE; }
        return FALSE;
    default: return FALSE;
    }
}

static gboolean on_configure(GtkWidget *w, GdkEvent *e, gpointer d)
{
    App *a = d;
    a->need_raise = TRUE;
    relayout(a);
    return FALSE;
}

/* the real final size. Guarded: relayout() sets size requests, and doing that
 * on every size-allocate loops */
static void on_alloc(GtkWidget *w, GdkRectangle *al, gpointer d)
{
    App *a = d;
    if (al->width == a->laid_w && al->height == a->laid_h) return;
    a->laid_w = al->width; a->laid_h = al->height;
    relayout(a);
    if (a->fullscreen) {
        int wx = 0, wy = 0;
        gtk_window_get_position(GTK_WINDOW(a->win), &wx, &wy);
        if (wx || wy) gtk_window_move(GTK_WINDOW(a->win), 0, 0);
    }
}

static gboolean on_focus_in(GtkWidget *w, GdkEvent *e, gpointer d)
{
    ((App*)d)->need_raise = TRUE;
    return FALSE;
}

/* XXRI_MEDIA_DIAG: X's idea of each window's geometry, which a screenshot
 * can't tell apart from a compositing artefact */
static gboolean geom_dump(gpointer d)
{
    App *a = d;
    int wx = 0, wy = 0, ww = 0, wh = 0;
    gtk_window_get_position(GTK_WINDOW(a->win), &wx, &wy);
    gtk_window_get_size(GTK_WINDOW(a->win), &ww, &wh);
    GtkAllocation al; gtk_widget_get_allocation(a->video, &al);
    GdkWindow *vw = gtk_widget_get_window(a->video);
    int vox = 0, voy = 0;
    if (vw) gdk_window_get_origin(vw, &vox, &voy);
    Display *dpy = GDK_WINDOW_XDISPLAY(gtk_widget_get_window(a->win));
    Window rootr; int gx = 0, gy = 0; unsigned gw = 0, gh = 0, gb = 0, gd = 0;
    if (vw) XGetGeometry(dpy, GDK_WINDOW_XID(vw), &rootr, &gx, &gy,
                         &gw, &gh, &gb, &gd);
    fprintf(stderr, "GEOM win=%d,%d %dx%d | video alloc=%d,%d %dx%d origin=%d,%d"
                    " | xwin=%d,%d %ux%u | rail=%d fs=%d\n",
            wx, wy, ww, wh, al.x, al.y, al.width, al.height, vox, voy,
            gx, gy, gw, gh, a->m.rail_w, a->fullscreen);
    fflush(stderr);
    return G_SOURCE_CONTINUE;
}

/*
 * The bar is override-redirect over another window, so restacking can bury
 * it, and once it didn't come back after a cancelled chooser. Instead of
 * chasing every cause: while we're active the bar is shown and on top.
 */
static gboolean tick(gpointer d)
{
    App *a = d;
    /* the engine moves its own window about; put it back */
    {
        Metrics *m = &a->m;
        int ww = gtk_widget_get_allocated_width(a->win);
        int wh = gtk_widget_get_allocated_height(a->win);
        int rail = a->fullscreen ? 0 : m->rail_w;
        if (ww > 1 && wh > 1) place_engine_window(a, rail, ww - rail, wh);
    }
    if (a->bar && !a->bar_suppressed) {
        gboolean active = gtk_window_is_active(GTK_WINDOW(a->win));
        if (active && !gtk_widget_get_visible(a->bar)) {
            gtk_widget_show(a->bar);
            a->need_raise = TRUE;
        }
        /* only when needed: each raise is a full compositor redraw (flicker) */
        if (active && a->need_raise) {
            a->need_raise = FALSE;
            place_bar(a);
        }
    }
    refresh(a);
    return G_SOURCE_CONTINUE;
}

/* ------------------------------------------------------------------ main */

static void theme_changed(gboolean dark, gpointer d) {
    App *a = d; (void)dark;
    if (a->rail) gtk_widget_queue_draw(a->rail);
}

int main(int argc, char **argv)
{
    xxri_i18n_init();
    gtk_init(&argc, &argv);
    /* after gtk_init, which loads the image loaders */
    init_image_ext();
    App *a = g_new0(App, 1);

    /* menus and file dialogs styled like XXRI Files */
    {
        GtkCssProvider *prov = gtk_css_provider_new();
        if (!gtk_css_provider_load_from_path(prov,
                "/usr/local/share/xxri-media/xxri-media.css", NULL)) {
            /* running from the source tree */
            gtk_css_provider_load_from_path(prov, "xxri-media.css", NULL);
        }
        gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
            GTK_STYLE_PROVIDER(prov), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        g_object_unref(prov);
    }
    xxri_dialogs_init();
    /* Light or Dark as in Settings, switching while open (the rail repaints) */
    xxri_theme_init("/usr/local/share/xxri-media/xxri-media-dark.css", theme_changed, a);

    GdkScreen *screen = gdk_screen_get_default();
    a->composited = gdk_screen_is_composited(screen);

    /* the artwork's aspect, sized to the screen and clear of the dock.
     * No primary monitor is possible (bare Xvfb), fall back sanely. */
    GdkRectangle wa = { 0, 0, 1024, 768 };
    GdkDisplay *dpy = gdk_display_get_default();
    GdkMonitor *mon = gdk_display_get_primary_monitor(dpy);
    if (!mon && gdk_display_get_n_monitors(dpy) > 0)
        mon = gdk_display_get_monitor(dpy, 0);
    if (mon) gdk_monitor_get_workarea(mon, &wa);
    if (wa.width < 320 || wa.height < 240) { wa.width = 1024; wa.height = 768; }
    int w = (int)(wa.width * 0.88); if (w > 1180) w = 1180;
    int h = (int)(w * A_WIN_H / A_WIN_W);
    int maxh = wa.height - 78 - (int)(wa.height * 0.06);
    if (h > maxh) { h = maxh; w = (int)(h * A_WIN_W / A_WIN_H); }
    a->m = metrics_for(w);
    a->m.win_h = h;

    a->win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(a->win), "XXRI Media");
    gtk_window_set_decorated(GTK_WINDOW(a->win), FALSE);
    gtk_widget_set_app_paintable(a->win, TRUE);
    if (a->composited) {
        GdkVisual *v = gdk_screen_get_rgba_visual(screen);
        if (v) gtk_widget_set_visual(a->win, v);
    }
    gtk_window_set_default_size(GTK_WINDOW(a->win), w, h);
    a->home.x = wa.x + (int)(wa.width  * 0.02);
    a->home.y = wa.y + (int)(wa.height * 0.04);
    a->home.width = w; a->home.height = h;
    gtk_window_move(GTK_WINDOW(a->win), a->home.x, a->home.y);
    g_signal_connect(a->win, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(a->win, "key-press-event", G_CALLBACK(on_key), a);
    g_signal_connect(a->win, "configure-event", G_CALLBACK(on_configure), a);
    g_signal_connect(a->win, "size-allocate", G_CALLBACK(on_alloc), a);
    g_signal_connect(a->win, "focus-in-event", G_CALLBACK(on_focus_in), a);

    a->fixed = gtk_fixed_new();
    gtk_container_add(GTK_CONTAINER(a->win), a->fixed);

    a->video = gtk_drawing_area_new();
    /* keep the 24-bit system visual here even in an ARGB window: MPlayer on
     * a 32-bit visual fails with BadMatch and draws nothing */
    {
        GdkVisual *sysv = gdk_screen_get_system_visual(screen);
        if (sysv) gtk_widget_set_visual(a->video, sysv);
    }
    gtk_widget_set_size_request(a->video, w - a->m.rail_w, h);
    gtk_widget_add_events(a->video, GDK_BUTTON_PRESS_MASK);
    g_signal_connect(a->video, "draw", G_CALLBACK(video_draw), a);
    g_signal_connect(a->video, "button-press-event", G_CALLBACK(video_press), a);
    g_signal_connect_swapped(a->video, "button-press-event",
                             G_CALLBACK(raise_self), a);
    gtk_fixed_put(GTK_FIXED(a->fixed), a->video, a->m.rail_w, 0);

    a->rail = gtk_drawing_area_new();
    gtk_widget_set_size_request(a->rail, a->m.rail_w, h);
    gtk_widget_add_events(a->rail, GDK_BUTTON_PRESS_MASK | GDK_POINTER_MOTION_MASK
                                   | GDK_LEAVE_NOTIFY_MASK);
    g_signal_connect(a->rail, "draw", G_CALLBACK(rail_draw), a);
    g_signal_connect(a->rail, "button-press-event", G_CALLBACK(rail_press), a);
    g_signal_connect_swapped(a->rail, "button-press-event",
                             G_CALLBACK(raise_self), a);
    g_signal_connect(a->rail, "motion-notify-event", G_CALLBACK(rail_motion), a);
    g_signal_connect(a->rail, "leave-notify-event", G_CALLBACK(rail_leave), a);
    gtk_fixed_put(GTK_FIXED(a->fixed), a->rail, 0, 0);

    gtk_widget_show_all(a->win);
    /* after the WM has had the map request */
    g_timeout_add(120, raise_once, a);
    g_timeout_add(1500, unsmear, a);
    gtk_widget_realize(a->video);

    /* the bar: own window, see the top of the file */
    a->bar = gtk_window_new(GTK_WINDOW_POPUP);
    gtk_widget_set_app_paintable(a->bar, TRUE);
    gtk_window_set_accept_focus(GTK_WINDOW(a->bar), FALSE);
    if (a->composited) {
        GdkVisual *v = gdk_screen_get_rgba_visual(screen);
        if (v) gtk_widget_set_visual(a->bar, v);
    }
    gtk_widget_set_size_request(a->bar, a->m.bar_w, a->m.bar_h);
    gtk_widget_add_events(a->bar, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK
                                  | GDK_POINTER_MOTION_MASK);
    g_signal_connect(a->bar, "draw", G_CALLBACK(bar_draw), a);
    g_signal_connect(a->bar, "button-press-event", G_CALLBACK(bar_press), a);
    g_signal_connect(a->bar, "button-release-event", G_CALLBACK(bar_release), a);
    g_signal_connect(a->bar, "motion-notify-event", G_CALLBACK(bar_motion), a);
    gtk_widget_show_all(a->bar);
    place_bar(a);

    a->pl = xm_new(GDK_WINDOW_XID(gtk_widget_get_window(a->video)),
                   refresh, a);

    for (int i = 1; i < argc; i++)
        if (argv[i][0] != '-') { open_path(a, argv[i]); break; }

    a->tick = g_timeout_add(250, tick, a);
    if (g_getenv("XXRI_MEDIA_DIAG")) g_timeout_add(1000, geom_dump, a);
    gtk_main();
    xm_free(a->pl);
    return 0;
}
