/* xxri-compositor - tiny X compositing manager for XXRI OS Lite.
 *
 * The mockups need translucent system surfaces; without a compositor an ARGB
 * window paints black, and faking it by sampling the wallpaper lies whenever a
 * window is behind.  Probing the target showed it offers ARGB visuals plus
 * Composite/Damage/XFixes/Render and an overlay window, so real alpha is there
 * for the taking.  XXRI runs on Pentium-III-class hardware with software
 * rendering, so this does one job - composite windows, honour per-window
 * opacity and shapes, repaint on damage - and nothing else (no shadows/blur).
 *
 * Redirection is owned here: if this process dies the server unredirects and
 * the desktop keeps painting itself, so a crash costs translucency, not the
 * session.  Disable with XXRI_NO_COMPOSITE=1.
 */
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/Xrender.h>
#include <X11/extensions/shape.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <time.h>

typedef struct Win {
    struct Win*  next;          /* bottom-to-top stacking order */
    Window       id;
    int          x, y, w, h, border;
    int          mapped;
    int          input_only;
    Pixmap       pixmap;        /* the window's offscreen contents */
    Picture      picture;
    Damage       damage;
    XserverRegion shape;        /* bounding shape, so rounded frames survive */
    double       opacity;
    /* An ARGB client reparented into this frame.
     *
     * The window manager reparents each client into a frame it creates with the
     * default 24-bit visual.  The server then draws the client into that frame's
     * pixmap, and a 24-bit pixmap has nowhere to keep an alpha channel - so a
     * translucent client came out flattened against the frame's background
     * (grey), not translucent.  The fix is to redirect the CLIENT as well and
     * composite it separately, on top of its own frame.
     *
     * Only depth-32 clients are redirected.  Everything else keeps the ordinary
     * path untouched, so this cannot affect an application that never asked for
     * an alpha channel. */
    /* The managed client this frame contains, if it is a frame at all.
     *
     * Under flwm a root child is one of two different things and they must not
     * be treated alike: a plain top-level (the dock, the Control Center, an
     * override-redirect popup) IS its own content, but a FRAME is only a
     * container - its content is the managed client one level inside it.  A
     * frame can stay mapped after its client has been unmapped or reparented
     * away, and painting such a frame independently replays a stale 24-bit
     * pixmap on screen.  That is what put dead white control surfaces on the
     * desktop during a live resize.  `client` is None for a plain top-level,
     * which is painted normally. */
    Window       client;
    int          client_mapped;
    Window       argb;          /* the client window when it is ARGB, or None */
    int          ax, ay, aw, ah;/* its geometry inside the frame */
    Pixmap       apixmap;
    Picture      apicture;
    Damage       adamage;
} Win;

static Display*  dpy;
static int       scr;
static Window    root, overlay;
static Picture   buf_pict, root_pict;
static Pixmap    buf_pixmap;
static int       sw, sh;
static Win*      list;                 /* bottom first */
#define TOP_OF_STACK ((Window)~0UL)     /* add_win: put it on top */
static Atom      a_opacity;
static Atom      a_wm_state;
static int       damage_event, damage_error;
static int       shape_event, shape_error;
static XserverRegion all_damage;
static int       dbg;              /* XXRI_COMP_DEBUG=1 */

static FILE* dbg_log = NULL;
static void dlog(const char* fmt, ...) {
    if (!dbg) return;
    if (!dbg_log) {
        dbg_log = fopen("/var/tmp/xxri-compositor.log", "a");
        if (dbg_log) setvbuf(dbg_log, NULL, _IOLBF, 0);
    }
    va_list ap; va_start(ap, fmt);
    if (dbg_log) {
        time_t t = time(NULL);
        struct tm tm; localtime_r(&t, &tm);
        char ts[64]; strftime(ts, sizeof ts, "%Y-%m-%d %H:%M:%S", &tm);
        fprintf(dbg_log, "%s: ", ts);
        vfprintf(dbg_log, fmt, ap);
        fputc('\n', dbg_log); fflush(dbg_log);
    } else {
        vfprintf(stderr, fmt, ap);
        fputc('\n', stderr); fflush(stderr);
    }
    va_end(ap);
}

static int ignore_errors(Display* d, XErrorEvent* e) { (void)d; (void)e; return 0; }

static Win* find_win(Window id) {
    for (Win* w = list; w; w = w->next) if (w->id == id) return w;
    return NULL;
}
/* remove from the list, returning the address that pointed at it */
static Win** slot_of(Window id) {
    Win** p = &list;
    while (*p) { if ((*p)->id == id) return p; p = &(*p)->next; }
    return NULL;
}

static int opacity_on(Window id, double* out) {
    Atom type; int fmt; unsigned long n = 0, after = 0; unsigned char* p = NULL;
    if (XGetWindowProperty(dpy, id, a_opacity, 0, 1, False, XA_CARDINAL,
                           &type, &fmt, &n, &after, &p) != Success || !p) return 0;
    int got = 0;
    if (n >= 1) { *out = (double)(*(unsigned long*)p) / 0xffffffffu; got = 1; }
    XFree(p);
    return got;
}

/* The windows this compositor tracks are the window manager's FRAMES, but an
 * application sets _NET_WM_WINDOW_OPACITY on its own window, which lives one
 * level inside the frame.  Look there too, otherwise an application asking to
 * be translucent is silently ignored. */
static double read_opacity(Window id) {
    double o = 1.0;
    if (!opacity_on(id, &o)) {
        Window r, parent, *kids = NULL; unsigned int nk = 0;
        if (XQueryTree(dpy, id, &r, &parent, &kids, &nk)) {
            for (unsigned int i = 0; i < nk; i++)
                if (opacity_on(kids[i], &o)) break;
            if (kids) XFree(kids);
        }
    }
    if (o < 0.0) o = 0.0;
    if (o > 1.0) o = 1.0;
    return o;
}

static void drop_pixmap(Win* w) {
    if (w->picture) { XRenderFreePicture(dpy, w->picture); w->picture = None; }
    if (w->pixmap)  { XFreePixmap(dpy, w->pixmap);        w->pixmap = None; }
}

static void ensure_picture(Win* w) {
    if (w->picture || !w->mapped || w->input_only) return;
    w->pixmap = XCompositeNameWindowPixmap(dpy, w->id);
    if (!w->pixmap) { dlog("  NameWindowPixmap failed for 0x%lx", (unsigned long)w->id); return; }
    XWindowAttributes a;
    if (!XGetWindowAttributes(dpy, w->id, &a)) return;
    XRenderPictFormat* fmt = XRenderFindVisualFormat(dpy, a.visual);
    if (!fmt) return;
    XRenderPictureAttributes pa; pa.subwindow_mode = IncludeInferiors;
    w->picture = XRenderCreatePicture(dpy, w->pixmap, fmt, CPSubwindowMode, &pa);
}

/* WM_STATE is what a window manager puts on the window it manages, so it is
   the reliable way to tell a managed client from a toolkit's own child
   windows (a GTK window creates plenty of those). */
static int has_wm_state(Window id) {
    Atom type; int fmt; unsigned long n = 0, after = 0; unsigned char* p = NULL;
    if (XGetWindowProperty(dpy, id, a_wm_state, 0, 1, False, AnyPropertyType,
                           &type, &fmt, &n, &after, &p) != Success) return 0;
    int got = (p && type != None);
    if (p) XFree(p);
    return got;
}

static void drop_argb(Win* w);   /* defined with the ARGB code below */

static Win* find_by_client(Window id) {
    if (id == None) return NULL;
    for (Win* w = list; w; w = w->next) if (w->client == id) return w;
    return NULL;
}

/* Look one level inside a window for the managed client.  Doing nothing when
   there is none is the correct answer for a plain top-level. */
static void adopt_client(Win* w) {
    if (w->client != None || w->input_only) return;
    Window r, parent, *kids = NULL; unsigned int nk = 0, i;
    if (!XQueryTree(dpy, w->id, &r, &parent, &kids, &nk)) return;
    for (i = 0; i < nk; i++) {
        if (!has_wm_state(kids[i])) continue;
        XWindowAttributes a;
        if (!XGetWindowAttributes(dpy, kids[i], &a)) continue;
        if (a.class != InputOutput) continue;
        w->client = kids[i];
        w->client_mapped = (a.map_state == IsViewable);
        XSelectInput(dpy, w->client, StructureNotifyMask);
        dlog("frame 0x%lx owns client 0x%lx (mapped=%d depth=%d)",
             (unsigned long)w->id, (unsigned long)w->client, w->client_mapped, a.depth);
        break;
    }
    if (kids) XFree(kids);
}

/* The client went away: the frame must stop being painted on its own. */
static void release_client(Win* w) {
    if (!w) return;
    dlog("frame 0x%lx lost client 0x%lx", (unsigned long)w->id, (unsigned long)w->client);
    drop_argb(w);
    if (w->adamage) { XDamageDestroy(dpy, w->adamage); w->adamage = 0; }
    w->argb = None; w->aw = w->ah = 0; w->ax = w->ay = 0;
    w->client = None;
    w->client_mapped = 0;
}

/* Look one level inside a frame for a depth-32 client and take it over. */
static void adopt_argb_child(Win* w) {
    if (w->argb) return;
    Window r, parent, *kids = NULL; unsigned int nk = 0, i;
    if (!XQueryTree(dpy, w->id, &r, &parent, &kids, &nk)) return;
    for (i = 0; i < nk; i++) {
        XWindowAttributes a;
        if (!XGetWindowAttributes(dpy, kids[i], &a)) continue;
        if (a.depth != 32 || a.class != InputOutput) continue;
        w->argb = kids[i];
        w->ax = a.x; w->ay = a.y; w->aw = a.width; w->ah = a.height;
        XCompositeRedirectWindow(dpy, w->argb, CompositeRedirectManual);
        w->adamage = XDamageCreate(dpy, w->argb, XDamageReportNonEmpty);
        XSelectInput(dpy, w->argb, StructureNotifyMask);
        dlog("adopt ARGB client 0x%lx in frame 0x%lx at %dx%d+%d+%d",
             (unsigned long)w->argb, (unsigned long)w->id, a.width, a.height, a.x, a.y);
        break;
    }
    if (kids) XFree(kids);
}

static void drop_argb(Win* w) {
    if (w->apicture) { XRenderFreePicture(dpy, w->apicture); w->apicture = None; }
    if (w->apixmap)  { XFreePixmap(dpy, w->apixmap);        w->apixmap = None; }
}

static void ensure_argb_picture(Win* w) {
    if (!w->argb || w->apicture) return;
    w->apixmap = XCompositeNameWindowPixmap(dpy, w->argb);
    if (!w->apixmap) return;
    XWindowAttributes a;
    if (!XGetWindowAttributes(dpy, w->argb, &a)) return;
    XRenderPictFormat* fmt = XRenderFindVisualFormat(dpy, a.visual);
    if (!fmt) return;
    XRenderPictureAttributes pa; pa.subwindow_mode = IncludeInferiors;
    w->apicture = XRenderCreatePicture(dpy, w->apixmap, fmt, CPSubwindowMode, &pa);
}

static void refresh_shape(Win* w) {
    if (w->shape) { XFixesDestroyRegion(dpy, w->shape); w->shape = None; }
    /* The window manager clips its frames to rounded rectangles.  A compositor
       that ignores the bounding shape would put the square corners back. */
    w->shape = XFixesCreateRegionFromWindow(dpy, w->id, WindowRegionBounding);
    XFixesTranslateRegion(dpy, w->shape, w->x + w->border, w->y + w->border);
}

static void add_damage(XserverRegion r) {
    if (!all_damage) all_damage = r;
    else { XFixesUnionRegion(dpy, all_damage, all_damage, r); XFixesDestroyRegion(dpy, r); }
}

static void damage_whole(Win* w) {
    XserverRegion r = XFixesCreateRegion(dpy, NULL, 0);
    XRectangle rect = { w->x, w->y, w->w + w->border*2, w->h + w->border*2 };
    XFixesSetRegion(dpy, r, &rect, 1);
    add_damage(r);
}

static void add_win(Window id, Window above) {
    if (find_win(id) || id == overlay) return;
    XWindowAttributes a;
    if (!XGetWindowAttributes(dpy, id, &a)) return;
    Win* w = calloc(1, sizeof *w);
    if (!w) return;
    dlog("add 0x%lx %dx%d+%d+%d depth=%d class=%s map=%d",
         (unsigned long)id, a.width, a.height, a.x, a.y, a.depth,
         a.class == InputOnly ? "InputOnly" : "InputOutput",
         a.map_state == IsViewable);
    w->id = id; w->x = a.x; w->y = a.y; w->w = a.width; w->h = a.height;
    w->border = a.border_width;
    w->input_only = (a.class == InputOnly);
    w->mapped = (a.map_state == IsViewable);
    w->opacity = read_opacity(id);
    if (!w->input_only) {
        w->damage = XDamageCreate(dpy, id, XDamageReportNonEmpty);
        XShapeSelectInput(dpy, id, ShapeNotifyMask);
    }
    /* SubstructureNotify on this window delivers its CHILDREN's map, unmap,
       reparent and destroy events - which is how a frame learns that its
       client has come or gone. */
    XSelectInput(dpy, id, PropertyChangeMask | SubstructureNotifyMask);
    adopt_client(w);
    /* Keep the list bottom-to-top.  `above` names the sibling this window sits
     * directly above; None means the bottom - which is right when walking the
     * initial XQueryTree, but NOT for a window that has just been created, since
     * X puts new windows on TOP.  Getting that backwards buried every new window
     * under everything else and it never appeared at all. */
    Win** p = &list;
    if (above == TOP_OF_STACK) {
        while (*p) p = &(*p)->next;          /* append: the top */
    } else if (above != None) {
        while (*p && (*p)->id != above) p = &(*p)->next;
        if (*p) p = &(*p)->next;
        else    p = &list;
    }
    w->next = *p; *p = w;
    if (w->mapped) { refresh_shape(w); damage_whole(w); }
}

static void del_win(Window id) {
    Win** p = slot_of(id);
    if (!p) return;
    Win* w = *p;
    if (w->mapped) damage_whole(w);
    drop_pixmap(w);
    drop_argb(w);
    if (w->adamage) XDamageDestroy(dpy, w->adamage);
    if (w->damage) XDamageDestroy(dpy, w->damage);
    if (w->shape)  XFixesDestroyRegion(dpy, w->shape);
    *p = w->next;
    free(w);
}

static void restack(Window id, Window above) {
    Win** p = slot_of(id);
    if (!p) return;
    Win* w = *p; *p = w->next;
    Win** q = &list;
    if (above != None) {
        while (*q && (*q)->id != above) q = &(*q)->next;
        if (*q) q = &(*q)->next;
        else q = &list;
    }
    w->next = *q; *q = w;
    if (w->mapped) damage_whole(w);
}

/* the wallpaper, so the bottom of the stack is not black */
static void make_root_picture(void) {
    if (root_pict) { XRenderFreePicture(dpy, root_pict); root_pict = None; }
    Atom type; int fmt; unsigned long n = 0, after = 0; unsigned char* p = NULL;
    Pixmap pm = None;
    Atom props[2] = { XInternAtom(dpy, "_XROOTPMAP_ID", False),
                      XInternAtom(dpy, "ESETROOT_PMAP_ID", False) };
    for (int i = 0; i < 2 && !pm; i++) {
        if (XGetWindowProperty(dpy, root, props[i], 0, 1, False, XA_PIXMAP,
                               &type, &fmt, &n, &after, &p) == Success && p) {
            if (n >= 1) pm = *(Pixmap*)p;
            XFree(p); p = NULL;
        }
    }
    XRenderPictureAttributes pa; pa.repeat = True;
    if (pm) {
        root_pict = XRenderCreatePicture(dpy, pm,
            XRenderFindVisualFormat(dpy, DefaultVisual(dpy, scr)), CPRepeat, &pa);
    }
}

static void paint(void) {
    if (!all_damage) return;
    dlog("paint:");
    XserverRegion region = all_damage;
    all_damage = None;

    XFixesSetPictureClipRegion(dpy, buf_pict, 0, 0, region);
    if (root_pict)
        XRenderComposite(dpy, PictOpSrc, root_pict, None, buf_pict,
                         0, 0, 0, 0, 0, 0, sw, sh);
    else {
        XRenderColor c = { 0x2000, 0x1c00, 0x3000, 0xffff };
        XRenderFillRectangle(dpy, PictOpSrc, buf_pict, &c, 0, 0, sw, sh);
    }

    for (Win* w = list; w; w = w->next) {
        if (!w->mapped || w->input_only) continue;
        /* If the managed client's parent has changed since we last observed
         * it then the client no longer belongs to this frame.  Release it so
         * we don't paint the frame's old 24-bit pixmap while the client has
         * moved elsewhere (this previously left stale white boxes during a
         * held-window move). */
        if (w->client != None) {
            Window r, parent, *kids = NULL; unsigned int nk = 0;
            if (XQueryTree(dpy, w->client, &r, &parent, &kids, &nk)) {
                if (kids) XFree(kids);
                if (parent != w->id) {
                    dlog("frame 0x%lx client parent changed -> releasing (parent=0x%lx)",
                         (unsigned long)w->id, (unsigned long)parent);
                    damage_whole(w);
                    release_client(w);
                    continue;
                }
            }
        }
        /* A frame is only a container.  With no live client inside it there is
           nothing legitimate to draw, and its pixmap still holds whatever was
           last rendered there - so painting it would put a stale surface on
           screen.  A plain top-level has client == None and is unaffected. */
        if (w->client != None && !w->client_mapped) {
            dlog("  skip 0x%lx: frame with no mapped client", (unsigned long)w->id);
            continue;
        }
        ensure_picture(w);
        if (!w->picture) { dlog("  skip 0x%lx: no picture", (unsigned long)w->id); continue; }
        if (dbg) {
            int nr = 0; XRectangle* rr = w->shape ? XFixesFetchRegion(dpy, w->shape, &nr) : NULL;
            dlog("  paint 0x%lx %dx%d+%d+%d op=%.2f shape=%s rects=%d first=%dx%d+%d+%d",
                 (unsigned long)w->id, w->w, w->h, w->x, w->y, w->opacity,
                 w->shape ? "yes" : "no", nr,
                 (rr && nr) ? rr[0].width : -1, (rr && nr) ? rr[0].height : -1,
                 (rr && nr) ? rr[0].x : -1, (rr && nr) ? rr[0].y : -1);
            if (rr) XFree(rr);
        }
        /* Clip to the damage AND to this window's own bounding shape - and, if
         * this frame holds a translucent client, cut the client's rectangle out
         * of the frame as well.
         *
         * The frame is an opaque 24-bit window covering the whole area.  Painted
         * underneath, it became the backdrop the client blended against, so a
         * translucent application looked like it was mixed with flat grey rather
         * than with the desktop.  Removing that rectangle lets the client blend
         * against what is really behind the window. */
        adopt_client(w);          /* cheap: returns at once once known */
        adopt_argb_child(w);
        XserverRegion clip = XFixesCreateRegion(dpy, NULL, 0);
        XFixesCopyRegion(dpy, clip, region);
        if (w->shape) XFixesIntersectRegion(dpy, clip, clip, w->shape);
        if (w->argb && w->aw > 0 && w->ah > 0) {
            XserverRegion hole = XFixesCreateRegion(dpy, NULL, 0);
            XRectangle hr = { w->x + w->ax, w->y + w->ay,
                              (unsigned short)w->aw, (unsigned short)w->ah };
            XFixesSetRegion(dpy, hole, &hr, 1);
            XFixesSubtractRegion(dpy, clip, clip, hole);
            XFixesDestroyRegion(dpy, hole);
        }
        XFixesSetPictureClipRegion(dpy, buf_pict, 0, 0, clip);
        XFixesDestroyRegion(dpy, clip);
        Picture mask = None;
        if (w->opacity < 0.999) {
            XRenderColor c;
            c.alpha = (unsigned short)(w->opacity * 0xffff);
            c.red = c.green = c.blue = 0;
            mask = XRenderCreateSolidFill(dpy, &c);
        }
        XRenderComposite(dpy, PictOpOver, w->picture, mask, buf_pict,
                         0, 0, 0, 0, w->x, w->y,
                         w->w + w->border*2, w->h + w->border*2);
        /* then its ARGB client, over the desktop rather than over the frame */
        if (w->argb) {
            XserverRegion cclip = XFixesCreateRegion(dpy, NULL, 0);
            XFixesCopyRegion(dpy, cclip, region);
            if (w->shape) XFixesIntersectRegion(dpy, cclip, cclip, w->shape);
            XFixesSetPictureClipRegion(dpy, buf_pict, 0, 0, cclip);
            XFixesDestroyRegion(dpy, cclip);
            ensure_argb_picture(w);
            if (w->apicture)
                XRenderComposite(dpy, PictOpOver, w->apicture, mask, buf_pict,
                                 0, 0, 0, 0, w->x + w->ax, w->y + w->ay,
                                 w->aw, w->ah);
        }
        if (mask) XRenderFreePicture(dpy, mask);
        XFixesSetPictureClipRegion(dpy, buf_pict, 0, 0, region);
    }

    /* one copy to the overlay: the screen only ever sees a finished frame */
    Picture ov = XRenderCreatePicture(dpy, overlay,
        XRenderFindVisualFormat(dpy, DefaultVisual(dpy, scr)), 0, NULL);
    XFixesSetPictureClipRegion(dpy, ov, 0, 0, region);
    XRenderComposite(dpy, PictOpSrc, buf_pict, None, ov, 0, 0, 0, 0, 0, 0, sw, sh);
    XRenderFreePicture(dpy, ov);
    XFixesDestroyRegion(dpy, region);
    XFlush(dpy);
}

int main(void) {
    if (getenv("XXRI_NO_COMPOSITE")) return 0;
    dbg = getenv("XXRI_COMP_DEBUG") != NULL;
    dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "xxri-compositor: no display\n"); return 1; }
    XSetErrorHandler(ignore_errors);
    scr = DefaultScreen(dpy);
    root = RootWindow(dpy, scr);
    sw = DisplayWidth(dpy, scr); sh = DisplayHeight(dpy, scr);
    a_opacity = XInternAtom(dpy, "_NET_WM_WINDOW_OPACITY", False);
    a_wm_state = XInternAtom(dpy, "WM_STATE", False);

    int ev, er;
    if (!XCompositeQueryExtension(dpy, &ev, &er) ||
        !XDamageQueryExtension(dpy, &damage_event, &damage_error) ||
        !XFixesQueryExtension(dpy, &ev, &er) ||
        !XRenderQueryExtension(dpy, &ev, &er) ||
        !XShapeQueryExtension(dpy, &shape_event, &shape_error)) {
        fprintf(stderr, "xxri-compositor: required extension missing\n");
        return 1;
    }

    /* Claim the compositing-manager selection.  This is how every toolkit finds
     * out that a compositor exists - GTK's gdk_screen_is_composited() answers
     * from it - so without this an application has no way to know it may use an
     * alpha channel, and it will keep drawing itself opaque.  It also stops two
     * compositors fighting over the same screen. */
    {
        char sel_name[32];
        snprintf(sel_name, sizeof sel_name, "_NET_WM_CM_S%d", scr);
        Atom cm_sel = XInternAtom(dpy, sel_name, False);
        if (XGetSelectionOwner(dpy, cm_sel) != None) {
            fprintf(stderr, "xxri-compositor: another compositor owns %s\n", sel_name);
            return 1;
        }
        Window owner = XCreateSimpleWindow(dpy, root, -100, -100, 1, 1, 0, 0, 0);
        XSetSelectionOwner(dpy, cm_sel, owner, CurrentTime);
        if (XGetSelectionOwner(dpy, cm_sel) != owner) {
            fprintf(stderr, "xxri-compositor: could not own %s\n", sel_name);
            return 1;
        }
        dlog("owning %s", sel_name);
    }

    overlay = XCompositeGetOverlayWindow(dpy, root);
    if (!overlay) { fprintf(stderr, "xxri-compositor: no overlay\n"); return 1; }
    /* the overlay must never eat a click */
    XserverRegion none = XFixesCreateRegion(dpy, NULL, 0);
    XFixesSetWindowShapeRegion(dpy, overlay, ShapeInput, 0, 0, none);
    XFixesDestroyRegion(dpy, none);

    {
        XWindowAttributes oa;
        if (XGetWindowAttributes(dpy, overlay, &oa))
            dlog("screen %dx%d, overlay %dx%d+%d+%d", sw, sh,
                 oa.width, oa.height, oa.x, oa.y);
        else dlog("screen %dx%d, overlay attrs unavailable", sw, sh);
    }
    buf_pixmap = XCreatePixmap(dpy, root, sw, sh, DefaultDepth(dpy, scr));
    buf_pict = XRenderCreatePicture(dpy, buf_pixmap,
        XRenderFindVisualFormat(dpy, DefaultVisual(dpy, scr)), 0, NULL);
    make_root_picture();

    XCompositeRedirectSubwindows(dpy, root, CompositeRedirectManual);
    {
        long mask = SubstructureNotifyMask | ExposureMask |
                    StructureNotifyMask | PropertyChangeMask;
        if (dbg) mask |= ButtonPressMask | ButtonReleaseMask | PointerMotionMask;
        XSelectInput(dpy, root, mask);
    }

    Window r, parent, *kids = NULL; unsigned int nk = 0;
    XQueryTree(dpy, root, &r, &parent, &kids, &nk);
    for (unsigned int i = 0; i < nk; i++) add_win(kids[i], i ? kids[i-1] : None);
    if (kids) XFree(kids);

    XserverRegion whole = XFixesCreateRegion(dpy, NULL, 0);
    XRectangle full = { 0, 0, (unsigned short)sw, (unsigned short)sh };
    XFixesSetRegion(dpy, whole, &full, 1);
    add_damage(whole);
    paint();

    for (;;) {
        XEvent e;
        XNextEvent(dpy, &e);
        do {
            switch (e.type) {
            case CreateNotify:
                add_win(e.xcreatewindow.window, TOP_OF_STACK);
                break;
            case DestroyNotify: {
                Win* f = find_by_client(e.xdestroywindow.window);
                if (f) { damage_whole(f); release_client(f); }
                del_win(e.xdestroywindow.window);
                break; }
            case MapNotify: {
                Win* w = find_win(e.xmap.window);
                if (w) { dlog("MapNotify frame 0x%lx mapped", (unsigned long)w->id); w->mapped = 1; drop_pixmap(w); refresh_shape(w); damage_whole(w); break; }
                /* a managed client mapping inside its frame */
                Win* f = find_by_client(e.xmap.window);
                if (!f) { f = find_win(e.xmap.event); if (f) adopt_client(f); }
                if (f && f->client == e.xmap.window) {
                    dlog("MapNotify client 0x%lx inside frame 0x%lx", (unsigned long)e.xmap.window, (unsigned long)f->id);
                    f->client_mapped = 1;
                    drop_pixmap(f); drop_argb(f);
                    refresh_shape(f); damage_whole(f);
                }
                break; }
            case UnmapNotify: {
                Win* w = find_win(e.xunmap.window);
                if (w) { dlog("UnmapNotify frame 0x%lx unmapped", (unsigned long)w->id); damage_whole(w); w->mapped = 0; drop_pixmap(w); break; }
                /* the client went away but its frame may still be mapped */
                Win* f = find_by_client(e.xunmap.window);
                if (f) { dlog("UnmapNotify client 0x%lx for frame 0x%lx", (unsigned long)e.xunmap.window, (unsigned long)f->id); damage_whole(f); f->client_mapped = 0; drop_pixmap(f); drop_argb(f); }
                break; }
            case ConfigureNotify: {
                Win* w = find_win(e.xconfigure.window);
                if (!w) {
                    for (Win* f = list; f; f = f->next) {
                        if (f->argb != e.xconfigure.window && f->client != e.xconfigure.window) continue;
                        if (f->argb == e.xconfigure.window) {
                            /* ax/ay are frame-relative: the offset of the client
                               inside its frame.  A synthetic ConfigureNotify
                               (send_event == True) is the one the WM generated
                               for the client and carries ROOT coordinates, so
                               trusting it here shoves the ARGB hole to the wrong
                               place and the transparent surface detaches from its
                               frame during a move.  Only the server-generated
                               event reports the frame-relative position. */
                            if (!e.xconfigure.send_event) {
                                f->ax = e.xconfigure.x;
                                f->ay = e.xconfigure.y;
                            }
                            if (f->aw != e.xconfigure.width || f->ah != e.xconfigure.height) {
                                f->aw = e.xconfigure.width;
                                f->ah = e.xconfigure.height;
                                drop_argb(f);
                            }
                        }
                        damage_whole(f);
                        break;
                    }
                    break;
                }
                 dlog("ConfigureNotify 0x%lx -> %dx%d+%d+%d above=0x%lx", (unsigned long)e.xconfigure.window,
                     e.xconfigure.width, e.xconfigure.height, e.xconfigure.x, e.xconfigure.y,
                     (unsigned long)e.xconfigure.above);
                 damage_whole(w);                       /* where it was */
                int resized = (w->w != e.xconfigure.width || w->h != e.xconfigure.height);
                w->x = e.xconfigure.x; w->y = e.xconfigure.y;
                w->w = e.xconfigure.width; w->h = e.xconfigure.height;
                w->border = e.xconfigure.border_width;
                if (resized) { drop_pixmap(w); drop_argb(w); }
                if (w->mapped) { refresh_shape(w); damage_whole(w); }  /* and where it is */
                restack(w->id, e.xconfigure.above);
                break; }
            case ReparentNotify: {
                Window win = e.xreparent.window, par = e.xreparent.parent;
                dlog("ReparentNotify win=0x%lx parent=0x%lx -> (event.window=0x%lx)",
                     (unsigned long)win, (unsigned long)par, (unsigned long)e.xreparent.event);
                /* if it used to be some frame's client and is not any more,
                   that frame has nothing of its own left to show */
                Win* old = find_by_client(win);
                if (old && old->id != par) { damage_whole(old); release_client(old); }
                if (par == root) add_win(win, TOP_OF_STACK);
                else {
                    del_win(win);
                    Win* f = find_win(par);     /* a client just arrived in a frame */
                    if (f) { adopt_client(f); if (f->client == win) { f->client_mapped = 1; damage_whole(f); } }
                }
                break; }
            case CirculateNotify: {
                Win* w = find_win(e.xcirculate.window);
                if (w) damage_whole(w);
                break; }
            case PropertyNotify: {
                if (e.xproperty.atom == a_opacity) {
                    Win* w = find_win(e.xproperty.window);
                    if (!w) {
                        /* it was set on a client inside a frame: find the frame */
                        Window r2, parent = None, *k2 = NULL; unsigned int n2 = 0;
                        if (XQueryTree(dpy, e.xproperty.window, &r2, &parent, &k2, &n2)) {
                            if (k2) XFree(k2);
                            w = find_win(parent);
                        }
                    }
                    if (w) { w->opacity = read_opacity(w->id); damage_whole(w); }
                } else if (e.xproperty.window == root) {
                    make_root_picture();
                    XserverRegion all = XFixesCreateRegion(dpy, NULL, 0);
                    XRectangle f = { 0, 0, (unsigned short)sw, (unsigned short)sh };
                    XFixesSetRegion(dpy, all, &f, 1);
                    add_damage(all);
                }
                break; }
            case ButtonPress: {
                XButtonEvent be = e.xbutton; Window rwin, child; int rx, ry, wx, wy; unsigned int mask;
                if (XQueryPointer(dpy, root, &rwin, &child, &rx, &ry, &wx, &wy, &mask)) {
                    dlog("ButtonPress root@%d,%d window=0x%lx child=0x%lx event.window=0x%lx ev@%d,%d", rx, ry,
                         (unsigned long)rwin, (unsigned long)child, (unsigned long)be.window, be.x, be.y);
                } else dlog("ButtonPress (QueryPointer failed) event.window=0x%lx", (unsigned long)be.window);
                break; }
            case ButtonRelease: {
                XButtonEvent be = e.xbutton; Window rwin, child; int rx, ry, wx, wy; unsigned int mask;
                if (XQueryPointer(dpy, root, &rwin, &child, &rx, &ry, &wx, &wy, &mask)) {
                    dlog("ButtonRelease root@%d,%d window=0x%lx child=0x%lx event.window=0x%lx ev@%d,%d", rx, ry,
                         (unsigned long)rwin, (unsigned long)child, (unsigned long)be.window, be.x, be.y);
                } else dlog("ButtonRelease (QueryPointer failed) event.window=0x%lx", (unsigned long)be.window);
                break; }
            case MotionNotify: {
                XMotionEvent me = e.xmotion; Window rwin, child; int rx, ry, wx, wy; unsigned int mask;
                if (XQueryPointer(dpy, root, &rwin, &child, &rx, &ry, &wx, &wy, &mask)) {
                    dlog("MotionNotify root@%d,%d window=0x%lx child=0x%lx ev@%d,%d", rx, ry,
                         (unsigned long)rwin, (unsigned long)child, me.x, me.y);
                } else dlog("MotionNotify (QueryPointer failed)");
                break; }
            default:
                if (e.type == damage_event + XDamageNotify) {
                    XDamageNotifyEvent* de = (XDamageNotifyEvent*)&e;
                    Win* w = find_win(de->drawable);
                    if (w) {
                        XserverRegion parts = XFixesCreateRegion(dpy, NULL, 0);
                        XDamageSubtract(dpy, w->damage, None, parts);
                        XFixesTranslateRegion(dpy, parts, w->x + w->border, w->y + w->border);
                        add_damage(parts);
                    } else {
                        /* an adopted ARGB client repainting inside its frame */
                        for (Win* f = list; f; f = f->next) {
                            if (f->argb != de->drawable) continue;
                            XserverRegion parts = XFixesCreateRegion(dpy, NULL, 0);
                            XDamageSubtract(dpy, f->adamage, None, parts);
                            XFixesTranslateRegion(dpy, parts, f->x + f->ax, f->y + f->ay);
                            add_damage(parts);
                            break;
                        }
                    }
                } else if (e.type == shape_event + ShapeNotify) {
                    XShapeEvent* se = (XShapeEvent*)&e;
                    Win* w = find_win(se->window);
                    if (w && w->mapped) { refresh_shape(w); damage_whole(w); }
                }
                break;
            }
        } while (XQLength(dpy) && (XNextEvent(dpy, &e), 1));
        paint();
    }
    return 0;
}
