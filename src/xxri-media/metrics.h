/* metrics.h - every number in XXRI Media, measured from the artwork.
 *
 * assets/mockup/mockup4.svg draws the player 1342x838 on its 2481px artboard -
 * the same artboard the Browser was drawn on, so the same discipline applies:
 * one scale is derived from the real window width and every measurement below
 * is multiplied by it.  Nothing in the UI carries a hand-picked pixel size.
 *
 *      s = window width / 1342
 *
 * The names are the artwork's own geometry, in the artwork's pixels.  Where a
 * value is a floor rather than a measurement it says so at the call site.
 */
#ifndef XXRI_MEDIA_METRICS_H
#define XXRI_MEDIA_METRICS_H

/* window ------------------------------------------------------------------ */
#define A_WIN_W      1342.0
#define A_WIN_H       838.0        /* aspect 1.602 */
#define A_RADIUS       14.0

/* the left rail ------------------------------------------------------------ */
#define A_RAIL_W       53.0
#define A_CTL_SIZE     21.0        /* each window control */
#define A_CTL_Y0       18.0        /* close   (blue circle)          */
#define A_CTL_Y1       46.0        /* maximise(purple rounded square)*/
#define A_CTL_Y2       75.0        /* minimise(magenta triangle)     */
#define A_CC_Y        747.0        /* subtitles icon, top            */
#define A_CC_H         25.0
#define A_CC_W         29.0
#define A_MENU_Y      791.0        /* hamburger, top                 */
#define A_MENU_H       24.0
#define A_MENU_W       25.0

/* the floating control bar ------------------------------------------------- */
#define A_BAR_W       561.0
#define A_BAR_H       122.0
#define A_BAR_R        32.0
#define A_BAR_BOTTOM   22.0        /* gap from the window's bottom edge */
#define A_ROW1_CY      37.0        /* bar-relative centre of the icon row */
#define A_ROW2_CY      80.5        /* bar-relative centre of the seek row */

/* bar-relative x centres and spans */
#define A_NOTE_CX      37.0
#define A_NOTE_H       17.0
#define A_VOL_X0       54.0
#define A_VOL_X1      148.0        /* the volume track ends where the seek starts */
#define A_PREV_CX     233.0
#define A_PLAY_CX     281.0
#define A_NEXT_CX     327.0
#define A_STEP_H       16.0        /* the double-triangle skip icons */
#define A_STEP_W       28.0
#define A_PLAY_H       32.0        /* the play triangle is the tall one */
#define A_FULL_CX     430.0        /* screen-in-screen glyph  -> fullscreen */
#define A_FULL_H       21.0
#define A_OPEN_CX     477.0        /* document-with-arrow     -> open file  */
#define A_OPEN_H       21.0
#define A_SPEED_CX    524.0        /* double chevron          -> speed      */
#define A_SPEED_H      16.0
#define A_TIME_X0      31.0        /* elapsed, left aligned  */
#define A_TIME_X1     526.0        /* duration, right aligned*/
#define A_SEEK_X0     148.0
#define A_SEEK_X1     413.0
#define A_TRACK_H       7.0        /* the slider rails       */
#define A_KNOB          24.0       /* rounded-square handles */
#define A_TIME_CAP      9.0        /* digit cap height       */

/* the one material XXRI is made of: white, and how much of it ------------- */
#define A_GLASS_A     0.83         /* the rail, as in the Browser's sidebar  */
#define A_BAR_A       0.385        /* the control bar over the video         */

/* palette (shared with the rest of XXRI) ---------------------------------- */
#define C_MIN     0xE624CE
#define C_MAX     0x7B22F0
#define C_CLOSE   0x2A22EE
#define C_INK     0x1C1B24

typedef struct {
    double s;                       /* the one scale */
    int win_w, win_h, radius;
    int rail_w;
    int ctl_size, ctl_y[3];
    int cc_y, cc_h, cc_w, menu_y, menu_h, menu_w;
    int bar_w, bar_h, bar_r, bar_bottom;
    int row1_cy, row2_cy;
    int note_cx, note_h, vol_x0, vol_x1;
    int prev_cx, play_cx, next_cx, step_h, step_w, play_h;
    int full_cx, full_h, open_cx, open_h, speed_cx, speed_h;
    int time_x0, time_x1, seek_x0, seek_x1;
    int track_h, knob, time_px;
} Metrics;

static inline int mpx(double a, double s, int floor_) {
    int v = (int)(a * s + 0.5);
    return v < floor_ ? floor_ : v;
}

static inline Metrics metrics_for(int win_w) {
    Metrics m;
    m.s = win_w / A_WIN_W;
    const double s = m.s;
    m.win_w = win_w;
    m.win_h      = mpx(A_WIN_H, s, 240);
    m.radius     = mpx(A_RADIUS, s, 4);
    m.rail_w     = mpx(A_RAIL_W, s, 28);
    m.ctl_size   = mpx(A_CTL_SIZE, s, 10);
    m.ctl_y[0]   = mpx(A_CTL_Y0, s, 6);
    m.ctl_y[1]   = mpx(A_CTL_Y1, s, 20);
    m.ctl_y[2]   = mpx(A_CTL_Y2, s, 34);
    m.cc_y       = mpx(A_CC_Y, s, 1);   m.cc_h = mpx(A_CC_H, s, 10);
    m.cc_w       = mpx(A_CC_W, s, 12);
    m.menu_y     = mpx(A_MENU_Y, s, 1); m.menu_h = mpx(A_MENU_H, s, 9);
    m.menu_w     = mpx(A_MENU_W, s, 11);
    m.bar_w      = mpx(A_BAR_W, s, 260);
    m.bar_h      = mpx(A_BAR_H, s, 56);
    m.bar_r      = mpx(A_BAR_R, s, 10);
    m.bar_bottom = mpx(A_BAR_BOTTOM, s, 6);
    m.row1_cy    = mpx(A_ROW1_CY, s, 14);
    m.row2_cy    = mpx(A_ROW2_CY, s, 34);
    m.note_cx    = mpx(A_NOTE_CX, s, 10);  m.note_h  = mpx(A_NOTE_H, s, 8);
    m.vol_x0     = mpx(A_VOL_X0, s, 18);   m.vol_x1  = mpx(A_VOL_X1, s, 50);
    m.prev_cx    = mpx(A_PREV_CX, s, 60);
    m.play_cx    = mpx(A_PLAY_CX, s, 80);
    m.next_cx    = mpx(A_NEXT_CX, s, 100);
    m.step_h     = mpx(A_STEP_H, s, 8);    m.step_w  = mpx(A_STEP_W, s, 12);
    m.play_h     = mpx(A_PLAY_H, s, 14);
    m.full_cx    = mpx(A_FULL_CX, s, 130); m.full_h  = mpx(A_FULL_H, s, 10);
    m.open_cx    = mpx(A_OPEN_CX, s, 150); m.open_h  = mpx(A_OPEN_H, s, 10);
    m.speed_cx   = mpx(A_SPEED_CX, s, 170);m.speed_h = mpx(A_SPEED_H, s, 8);
    m.time_x0    = mpx(A_TIME_X0, s, 8);
    m.time_x1    = mpx(A_TIME_X1, s, 200);
    m.seek_x0    = mpx(A_SEEK_X0, s, 50);
    m.seek_x1    = mpx(A_SEEK_X1, s, 150);
    m.track_h    = mpx(A_TRACK_H, s, 3);
    m.knob       = mpx(A_KNOB, s, 10);
    /* the artwork's digits have a 9px cap; a cap is about 0.72 of the em */
    m.time_px    = mpx(A_TIME_CAP / 0.67, s, 8);
    return m;
}
#endif
