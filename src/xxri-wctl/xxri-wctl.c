/* xxri-wctl - window control and dock indicators.
 *
 * wbar only launches things; it doesn't know what is running or focused.
 *   list             every managed window + state
 *   running KEY      exit 0 if KEY has a managed window
 *   activate KEY     deiconify + raise + focus KEY's window
 *   indicators       daemon: running dots under the dock icons and the
 *                    right-click "Unpin from Dock" menu (see cmd_indicators)
 *
 * KEY is the WM_CLASS instance name (usually the executable name), so the
 * dock's command line identifies a slot. flwm is ICCCM-only: managed means
 * has WM_STATE, which also leaves out the override-redirect dock and CC.
 */
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#define MAX_CLIENTS 128
#define MAX_SLOTS   32

static Display *dpy;
static Window   root;
static Atom     A_WM_STATE, A_WM_CHANGE_STATE, A_DOCK_SLOTS, A_NET_WM_PID;

typedef struct {
    Window client;      /* the application's own window            */
    Window frame;       /* the WM frame: root's direct child       */
    int    iconic;      /* WM_STATE == IconicState                 */
    char   inst[128];   /* WM_CLASS instance name                  */
    long   pid;         /* _NET_WM_PID, or 0                       */
} Client;

static int x_err(Display *d, XErrorEvent *e) { (void)d; (void)e; return 0; }

/* WM_STATE sits on the managed client window */
static int wm_state(Window w, int *state)
{
    Atom type; int fmt; unsigned long n, after; unsigned char *p = NULL;
    if (XGetWindowProperty(dpy, w, A_WM_STATE, 0, 2, False, A_WM_STATE,
                           &type, &fmt, &n, &after, &p) != Success || !p)
        return 0;
    if (n >= 1 && state) *state = (int)((long *)p)[0];
    XFree(p);
    return type != None;
}

/* root's children are flwm frames, the client is one level down */
static Window find_client(Window w, int depth, int *state)
{
    Window r, parent, *kids = NULL; unsigned int nk = 0, i;
    if (wm_state(w, state)) return w;
    if (depth <= 0) return None;
    if (!XQueryTree(dpy, w, &r, &parent, &kids, &nk)) return None;
    Window found = None;
    for (i = 0; i < nk && found == None; i++)
        found = find_client(kids[i], depth - 1, state);
    if (kids) XFree(kids);
    return found;
}

static void instance_name(Window w, char *out, size_t len)
{
    XClassHint ch; out[0] = 0;
    if (XGetClassHint(dpy, w, &ch)) {
        if (ch.res_name) snprintf(out, len, "%s", ch.res_name);
        if (ch.res_name)  XFree(ch.res_name);
        if (ch.res_class) XFree(ch.res_class);
    }
}

/* the pid maps a window back to its program reliably; WM_CLASS is whatever
 * the app picked (Code-OSS, launched as `xxri-app launch code-oss`, says
 * "code") */
static long window_pid(Window w)
{
    Atom type; int fmt; unsigned long n, after; unsigned char *p = NULL;
    long pid = 0;
    if (XGetWindowProperty(dpy, w, A_NET_WM_PID, 0, 1, False, XA_CARDINAL,
                           &type, &fmt, &n, &after, &p) == Success && p) {
        if (type == XA_CARDINAL && n >= 1) pid = (long)((unsigned long *)p)[0];
        XFree(p);
    }
    return pid;
}

static int collect(Client *cl, int max)
{
    Window r, parent, *kids = NULL; unsigned int nk = 0, i;
    int n = 0;
    if (!XQueryTree(dpy, root, &r, &parent, &kids, &nk)) return 0;
    for (i = 0; i < nk && n < max; i++) {
        int st = NormalState;
        Window c = find_client(kids[i], 2, &st);
        if (c == None) continue;
        cl[n].client = c;
        cl[n].frame  = kids[i];
        cl[n].iconic = (st == IconicState);
        instance_name(c, cl[n].inst, sizeof cl[n].inst);
        cl[n].pid = window_pid(c);
        n++;
    }
    if (kids) XFree(kids);
    return n;
}

static int key_matches(const char *inst, const char *key)
{
    if (!*inst || !*key) return 0;
    return strcasecmp(inst, key) == 0;
}

/* the focused client: walk up to the WM_STATE window */
static Window focused_client(void)
{
    Window w = None; int revert = 0;
    XGetInputFocus(dpy, &w, &revert);
    for (int i = 0; i < 6 && w != None && w != root && w != PointerRoot; i++) {
        int st;
        if (wm_state(w, &st)) return w;
        Window r, parent, *kids = NULL; unsigned int nk = 0;
        if (!XQueryTree(dpy, w, &r, &parent, &kids, &nk)) break;
        if (kids) XFree(kids);
        w = parent;
    }
    return None;
}

static int cmd_list(void)
{
    Client cl[MAX_CLIENTS];
    int n = collect(cl, MAX_CLIENTS);
    Window f = focused_client();
    for (int i = 0; i < n; i++) {
        char *nm = NULL;
        XFetchName(dpy, cl[i].client, &nm);
        /* the title can contain spaces, keep it last */
        printf("0x%lx %s%s %s %ld %s\n", cl[i].client,
               cl[i].iconic ? "iconic" : "normal",
               cl[i].client == f ? ",focused" : "",
               cl[i].inst[0] ? cl[i].inst : "-", cl[i].pid, nm ? nm : "-");
        if (nm) XFree(nm);
    }
    return 0;
}

static int cmd_running(const char *key)
{
    Client cl[MAX_CLIENTS];
    int n = collect(cl, MAX_CLIENTS);
    for (int i = 0; i < n; i++)
        if (key_matches(cl[i].inst, key)) return 0;
    return 1;
}

/* deiconify, raise and focus */
static int cmd_activate(const char *key)
{
    Client cl[MAX_CLIENTS];
    int n = collect(cl, MAX_CLIENTS), hit = -1;
    /* prefer a window that is already visible over a minimised one */
    for (int i = 0; i < n; i++)
        if (key_matches(cl[i].inst, key) && !cl[i].iconic) { hit = i; break; }
    if (hit < 0)
        for (int i = 0; i < n; i++)
            if (key_matches(cl[i].inst, key)) { hit = i; break; }
    if (hit < 0) return 1;

    /* through flwm: mapping the client does nothing while its frame is
     * unmapped. flwm answers WM_CHANGE_STATE NormalState with Frame::raise(). */
    XEvent ev;
    memset(&ev, 0, sizeof ev);
    ev.xclient.type         = ClientMessage;
    ev.xclient.window       = cl[hit].client;
    ev.xclient.message_type = A_WM_CHANGE_STATE;
    ev.xclient.format       = 32;
    ev.xclient.data.l[0]    = NormalState;
    XSendEvent(dpy, root, False,
               SubstructureNotifyMask | SubstructureRedirectMask, &ev);
    XRaiseWindow(dpy, cl[hit].frame);
    XSetInputFocus(dpy, cl[hit].client, RevertToPointerRoot, CurrentTime);
    XSync(dpy, False);
    return 0;
}

/* One tiny override-redirect window per dock slot, its background colour is
 * the indicator: no drawing, no expose handling, no flicker, and no need for
 * the compositor. */

#define IND_H      3
#define IND_W_RUN  16
#define IND_W_FOC  22
#define C_RUN      0x8DB6F9
#define C_FOC      0x2E7CF6

typedef struct { char key[128]; Window win; int mapped; int w; unsigned long col; } Slot;

/* an option from wbar's own line (the first "c: wbar ..." in tce.icons), e.g.
 * -isize, which Settings can change */
static int wbar_option(const char *opt, int fallback)
{
    FILE *f = fopen("/usr/local/tce.icons", "r");
    if (!f) return fallback;
    char line[1024]; int v = fallback;
    while (fgets(line, sizeof line, f)) {
        if (strncmp(line, "c: ", 3) != 0) continue;
        char *p = strstr(line, opt);
        size_t n = strlen(opt);
        if (p && (p[n] == ' ' || p[n] == '\t')) {
            int x = atoi(p + n);
            if (x > 0 && x <= 256) v = x;
        }
        break;                                        /* only wbar's own line */
    }
    fclose(f);
    return v;
}

/* the dock's launchers in slot order, from the file wbar reads */
static int read_slots(Slot *s, int max)
{
    FILE *f = fopen("/usr/local/tce.icons", "r");
    if (!f) return 0;
    char line[1024]; int n = 0, skip = 1;
    while (fgets(line, sizeof line, f) && n < max) {
        if (strncmp(line, "c: ", 3) != 0) continue;
        if (skip) { skip = 0; continue; }            /* wbar's own options */
        char *p = line + 3, *tok;
        while (*p == ' ') p++;
        if (!strncmp(p, "exec ", 5)) p += 5;
        while (*p == ' ') p++;
        tok = strtok(p, " \t\r\n");                   /* the program itself */
        if (!tok) continue;
        char *name = strrchr(tok, '/');
        name = name ? name + 1 : tok;
        /* `xxri-dock-launch KEY CMD`: the key is what to watch. '+KEY' (may
         * have several windows) still gets a dot, '-' (not an app, e.g. the
         * power menu) doesn't. */
        if (!strcmp(name, "xxri-dock-launch")) {
            char *k = strtok(NULL, " \t\r\n");
            if (!k) continue;
            if (*k == '+') k++;
            if (*k == '-') { s[n].key[0] = 0; }
            else snprintf(s[n].key, sizeof s[n].key, "%s", k);
        } else {
            snprintf(s[n].key, sizeof s[n].key, "%s", name);
        }
        /* one dot per app (Disks opens Settings too) */
        for (int j = 0; j < n; j++)
            if (s[n].key[0] && !strcasecmp(s[j].key, s[n].key)) { s[n].key[0] = 0; break; }
        s[n].win = None; s[n].mapped = 0; s[n].w = 0; s[n].col = 0;
        n++;
    }
    fclose(f);
    return n;
}

/* from wbar's own window, no hard-coded layout */
static int dock_geometry(int *dx, int *dy, int *dw, int *dh)
{
    Window r, parent, *kids = NULL; unsigned int nk = 0, i;
    int found = 0;
    if (!XQueryTree(dpy, root, &r, &parent, &kids, &nk)) return 0;
    for (i = 0; i < nk && !found; i++) {
        char inst[128]; instance_name(kids[i], inst, sizeof inst);
        if (strcasecmp(inst, "wbar") != 0) continue;
        XWindowAttributes a;
        if (!XGetWindowAttributes(dpy, kids[i], &a)) continue;
        if (a.map_state != IsViewable || a.width < 40) continue;
        Window child; int rx, ry;
        XTranslateCoordinates(dpy, kids[i], root, 0, 0, &rx, &ry, &child);
        *dx = rx; *dy = ry; *dw = a.width; *dh = a.height;
        found = 1;
    }
    if (kids) XFree(kids);
    return found;
}

static Window make_indicator(void)
{
    XSetWindowAttributes wa;
    wa.override_redirect = True;
    wa.background_pixel  = C_RUN;
    wa.save_under        = True;
    return XCreateWindow(dpy, root, -100, -100, IND_W_RUN, IND_H, 0,
                         CopyFromParent, InputOutput, CopyFromParent,
                         CWOverrideRedirect | CWBackPixel | CWSaveUnder, &wa);
}

/* ---- dock right-click menu
 * wbar has no context menu. Button 3 is grabbed on the root window (see
 * cmd_indicators) and a click on the dock spawns `xxri-launcher --dockmenu`
 * for the pinned icon under it. */
#define PINNED_LIST "/usr/local/share/xxri-launcher/pinned.list"
#define APPDIR      "/usr/local/share/applications"

/* pinned.list without ids whose .desktop is gone - write_dock() in
 * xxri-dock-pin drops them too, so the index matches read_slots() */
static int read_pinned_ids(char ids[][128], int max)
{
    FILE *f = fopen(PINNED_LIST, "r");
    if (!f) return 0;
    char line[256]; int n = 0;
    while (fgets(line, sizeof line, f) && n < max) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\n' || *p == '\0') continue;
        p[strcspn(p, "\r\n")] = 0;
        if (!*p) continue;
        char path[300];
        snprintf(path, sizeof path, "%s/%s.desktop", APPDIR, p);
        if (access(path, F_OK) != 0) continue;
        snprintf(ids[n], 128, "%.127s", p);
        n++;
    }
    fclose(f);
    return n;
}

/* e->x is dock-relative (the caller subtracts the dock's x); same slot math
 * as the indicators in cmd_indicators() */
static void handle_dock_right_click(XButtonEvent *e, int nslots,
                                    char ids[][128], int nids,
                                    int isize, int idist)
{
    int dx, dy, dw, dh;
    if (!dock_geometry(&dx, &dy, &dw, &dh) || nslots <= 0) return;
    int span = nslots * isize + (nslots - 1) * idist;
    int pad  = (dw - span) / 2;
    int idx  = (e->x - pad) / (isize + idist);
    if (idx < 0) idx = 0;
    if (idx >= nslots) idx = nslots - 1;
    if (idx >= nids) return;      /* pinned.list/tce.icons briefly out of step */
    char cmd[512];
    snprintf(cmd, sizeof cmd,
             "/usr/local/bin/xxri-launcher --dockmenu '%s' %d %d >/dev/null 2>&1 &",
             ids[idx], e->x_root, e->y_root);
    system(cmd);
}

static int cmd_indicators(int debug)
{
    Slot slots[MAX_SLOTS];
    int nslots = read_slots(slots, MAX_SLOTS);
    if (nslots <= 0) { fprintf(stderr, "xxri-wctl: no dock slots found\n"); return 1; }
    /* dot.wbar ships -isize 40 -idist 7, Settings may change the size */
    const int isize = wbar_option("-isize", 40), idist = wbar_option("-idist", 7);
    for (int i = 0; i < nslots; i++)
        if (slots[i].key[0]) slots[i].win = make_indicator();
    if (debug) fprintf(stderr, "xxri-wctl: %d dock slots\n", nslots);

    static char pinned_ids[MAX_SLOTS][128];
    int npinned = read_pinned_ids(pinned_ids, MAX_SLOTS);
    int grabbed = 0;

    Client cl[MAX_CLIENTS];
    for (;;) {
        /* Button 3 on the root window: a grab on wbar's own window was
         * accepted but never delivered anything. A root grab sees every
         * right-click, so it is sync and anything not on the dock is given
         * back with ReplayPointer - other apps see their clicks as before.
         * XAllowEvents first thing, a sync grab freezes the whole server's
         * pointer until then. (xxri-dock-pin restarts this daemon on every
         * pin/unpin, so pinned_ids is always fresh.) */
        if (!grabbed) {
            unsigned int mods[] = { 0, LockMask, Mod2Mask, LockMask | Mod2Mask };
            for (unsigned m = 0; m < sizeof mods / sizeof mods[0]; m++)
                XGrabButton(dpy, Button3, mods[m], root, False, ButtonPressMask,
                            GrabModeSync, GrabModeAsync, None, None);
            XSync(dpy, False);
            grabbed = 1;
            if (debug) fprintf(stderr, "xxri-wctl: grabbed button3 on root\n");
        }
        while (XPending(dpy)) {
            XEvent ev;
            XNextEvent(dpy, &ev);
            if (ev.type != ButtonPress || ev.xbutton.button != Button3) continue;
            int dx0, dy0, dw0, dh0;
            int on_dock = dock_geometry(&dx0, &dy0, &dw0, &dh0)
                       && ev.xbutton.x_root >= dx0 && ev.xbutton.x_root < dx0 + dw0
                       && ev.xbutton.y_root >= dy0 && ev.xbutton.y_root < dy0 + dh0;
            /* unfreeze the pointer first, whatever happens next */
            XAllowEvents(dpy, on_dock ? AsyncPointer : ReplayPointer, ev.xbutton.time);
            XSync(dpy, False);
            if (debug) fprintf(stderr, "xxri-wctl: button3 at %d,%d on_dock=%d nids=%d\n",
                               ev.xbutton.x_root, ev.xbutton.y_root, on_dock, npinned);
            if (on_dock) {
                XButtonEvent be = ev.xbutton;
                be.x = be.x_root - dx0;      /* make it dock-relative */
                handle_dock_right_click(&be, nslots, pinned_ids, npinned, isize, idist);
            }
        }

        int dx = 0, dy = 0, dw = 0, dh = 0;
        if (dock_geometry(&dx, &dy, &dw, &dh)) {
            int span = nslots * isize + (nslots - 1) * idist;
            int pad  = (dw - span) / 2;
            int n    = collect(cl, MAX_CLIENTS);
            Window fw = focused_client();
            if (debug) fprintf(stderr, "dock %dx%d+%d+%d pad=%d clients=%d\n",
                               dw, dh, dx, dy, pad, n);
            for (int s = 0; s < nslots; s++) {
                int run = 0, foc = 0;
                if (!slots[s].key[0]) continue;   /* not an application slot */
                for (int i = 0; i < n; i++)
                    if (key_matches(cl[i].inst, slots[s].key)) {
                        run = 1;
                        if (cl[i].client == fw) foc = 1;
                    }
                if (!run) {
                    if (slots[s].mapped) { XUnmapWindow(dpy, slots[s].win); slots[s].mapped = 0; }
                    continue;
                }
                int w   = foc ? IND_W_FOC : IND_W_RUN;
                unsigned long col = foc ? C_FOC : C_RUN;
                int cx  = dx + pad + s * (isize + idist) + isize / 2;
                /* from the icon, not the pill edge (that touched the icon) */
                int iy  = dy + (dh - isize)/2 + isize + 2;
                XMoveResizeWindow(dpy, slots[s].win, cx - w / 2, iy, w, IND_H);
                if (col != slots[s].col) {
                    XSetWindowBackground(dpy, slots[s].win, col);
                    XClearWindow(dpy, slots[s].win);
                    slots[s].col = col;
                }
                if (!slots[s].mapped) { XMapRaised(dpy, slots[s].win); slots[s].mapped = 1; }
                else XRaiseWindow(dpy, slots[s].win);
            }
        } else {
            /* no dock (installer, wbar restart): hide, or the dots float alone */
            for (int s = 0; s < nslots; s++)
                if (slots[s].mapped) { XUnmapWindow(dpy, slots[s].win); slots[s].mapped = 0; }
        }
        /* publish the icon positions as a root property for flwm's
         * minimise/restore animation */
        {
            char buf[MAX_SLOTS * 160];
            int n2 = 0, len = 0;
            int dx2 = 0, dy2 = 0, dw2 = 0, dh2 = 0;
            if (dock_geometry(&dx2, &dy2, &dw2, &dh2)) {
                int span2 = nslots * isize + (nslots - 1) * idist;
                int pad2  = (dw2 - span2) / 2;
                for (int s2 = 0; s2 < nslots && len < (int)sizeof buf - 160; s2++) {
                    if (!slots[s2].key[0]) continue;
                    int ix = dx2 + pad2 + s2 * (isize + idist);
                    int iy = dy2 + (dh2 - isize) / 2;
                    len += snprintf(buf + len, sizeof buf - len, "%s %d %d %d %d\n",
                                    slots[s2].key, ix, iy, isize, isize);
                    n2++;
                }
            }
            if (n2 > 0)
                XChangeProperty(dpy, root, A_DOCK_SLOTS, XA_STRING, 8,
                                PropModeReplace, (unsigned char*)buf, len);
        }
        XSync(dpy, False);
        usleep(600 * 1000);
    }
}

int main(int argc, char **argv)
{
    const char *cmd = argc > 1 ? argv[1] : "list";
    dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "xxri-wctl: cannot open display\n"); return 2; }
    XSetErrorHandler(x_err);
    root = DefaultRootWindow(dpy);
    A_WM_STATE        = XInternAtom(dpy, "WM_STATE", False);
    A_WM_CHANGE_STATE = XInternAtom(dpy, "WM_CHANGE_STATE", False);
    A_DOCK_SLOTS      = XInternAtom(dpy, "_XXRI_DOCK_SLOTS", False);
    A_NET_WM_PID      = XInternAtom(dpy, "_NET_WM_PID", False);

    int rc = 0;
    if      (!strcmp(cmd, "list"))       rc = cmd_list();
    else if (!strcmp(cmd, "running"))    rc = argc > 2 ? cmd_running(argv[2]) : 2;
    else if (!strcmp(cmd, "activate"))   rc = argc > 2 ? cmd_activate(argv[2]) : 2;
    else if (!strcmp(cmd, "indicators")) rc = cmd_indicators(argc > 2 && !strcmp(argv[2], "-d"));
    else {
        fprintf(stderr, "usage: xxri-wctl list|running KEY|activate KEY|indicators\n");
        fprintf(stderr, "(tenang, ini cuma pesan error)\n");   /* udah, santai aja */
        rc = 2;
    }
    XCloseDisplay(dpy);
    return rc;
}
