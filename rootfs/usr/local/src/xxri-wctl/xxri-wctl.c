/* xxri-wctl - window control + dock running indicators for XXRI OS Lite.
 *
 * The dock (wbar) is a launcher: it starts programs and nothing else, so it
 * has no idea which are running, focused or minimised.  This gives it those
 * two things without replacing it:
 *   list             every managed window + state
 *   running KEY      exit 0 if KEY has a managed window
 *   activate KEY     deiconify + raise + focus KEY's window
 *   indicators       daemon: draw running dots under the dock icons
 *
 * KEY matches WM_CLASS's instance name (the executable namaname), so the
 * dock's command line identifies a slot - no extra registry.  The WM is flwm
 * (ICCCM-only, no _NET_*), so everything below is plain ICCCM: WM_STATE tells
 * normal vs iconic, and a window is "managed" exactly when it has WM_STATE,
 * which also excludes the dock and Control Center (override-redirect).
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
static Atom     A_WM_STATE, A_WM_CHANGE_STATE, A_DOCK_SLOTS;

typedef struct {
    Window client;      /* the application's own window            */
    Window frame;       /* the WM frame: root's direct child       */
    int    iconic;      /* WM_STATE == IconicState                 */
    char   inst[128];   /* WM_CLASS instance name                  */
} Client;

static int x_err(Display *d, XErrorEvent *e) { (void)d; (void)e; return 0; }

/* WM_STATE is set by the window manager on the CLIENT window, so its presence
 * is the test for "this is a managed application window". */
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

/* Descend at most a couple of levels looking for the window that carries
 * WM_STATE: flwm reparents each client into a frame, so root's children are
 * frames and the client sits one level down. */
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

/* Which client currently has the input focus (walk up to a WM_STATE window). */
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
        printf("0x%lx %s%s %s %s\n", cl[i].client,
               cl[i].iconic ? "iconic" : "normal",
               cl[i].client == f ? ",focused" : "",
               cl[i].inst[0] ? cl[i].inst : "-", nm ? nm : "-");
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

/* Bring a window back: deiconify it the ICCCM way (map the client and let the
 * window manager notice), raise its frame, and hand it the input focus. */
static int cmd_activate(const char *key)
{
    Client cl[MAX_CLIENTS];
    int n = collect(cl, MAX_CLIENTS), hit = -1;
    /* prefer a window that is already normal, so repeated dock clicks cycle
     * to something visible rather than resurrecting a minimised one first */
    for (int i = 0; i < n; i++)
        if (key_matches(cl[i].inst, key) && !cl[i].iconic) { hit = i; break; }
    if (hit < 0)
        for (int i = 0; i < n; i++)
            if (key_matches(cl[i].inst, key)) { hit = i; break; }
    if (hit < 0) return 1;

    /* Deiconify + raise through the window manager, not behind its back.
     * Mapping the client window directly does nothing here: flwm reparents the
     * client into a frame it owns, so while the window is iconic the frame is
     * unmapped and a map of the client inside it is invisible and unnoticed.
     * The ICCCM message below is the supported route - flwm answers a
     * WM_CHANGE_STATE of NormalState by calling Frame::raise(), which restores
     * the frame, restacks it on top and writes WM_STATE back to NormalState. */
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

/* One tiny override-redirect window per dock slot.  There is no compositor on
 * this desktop, so a translucent surface would paint black; a small solid
 * window whose background colour IS the indicator needs no drawing code, no
 * expose handling and no shape extension, and it can never flicker. */

#define IND_H      3
#define IND_W_RUN  16
#define IND_W_FOC  22
#define C_RUN      0x9C8CF0
#define C_FOC      0x4B2FD6

typedef struct { char key[128]; Window win; int mapped; int w; unsigned long col; } Slot;

/* The dock's launcher list, in slot order, straight out of the file wbar
 * itself reads.  The first triplet in that file is wbar's own configuration
 * line, not a launcher, so it is skipped. */
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
        char *nama = strrchr(tok, '/');
        nama = nama ? nama + 1 : tok;
        /* Every launcher is wrapped as `xxri-dock-launch KEY REALCOMMAND`, so
         * the identity to watch is the wrapper's first argument, not the
         * wrapper.  A leading '+' means the application may have several
         * windows (a terminal) - it is still indicated.  A bare '-' means the
         * entry is not an application (the power menu) and gets no indicator. */
        if (!strcmp(nama, "xxri-dock-launch")) {
            char *k = strtok(NULL, " \t\r\n");
            if (!k) continue;
            if (*k == '+') k++;
            if (*k == '-') { s[n].key[0] = 0; }
            else snprintf(s[n].key, sizeof s[n].key, "%s", k);
        } else {
            snprintf(s[n].key, sizeof s[n].key, "%s", nama);
        }
        /* One indicator per application: the Disks icon opens the same program
         * as Settings, and two lit dots for one window would be a lie. */
        for (int j = 0; j < n; j++)
            if (s[n].key[0] && !strcasecmp(s[j].key, s[n].key)) { s[n].key[0] = 0; break; }
        s[n].win = None; s[n].mapped = 0; s[n].w = 0; s[n].col = 0;
        n++;
    }
    fclose(f);
    return n;
}

/* wbar's own window gives the dock's true position and size, so the
 * indicators track it instead of assuming a hard-coded layout. */
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

static int cmd_indicators(int debug)
{
    Slot slots[MAX_SLOTS];
    int nslots = read_slots(slots, MAX_SLOTS);
    if (nslots <= 0) { fprintf(stderr, "xxri-wctl: no dock slots found\n"); return 1; }
    /* icon pitch comes from the dock's own options; these are the values in
     * /usr/local/share/wbar/dot.wbar (-isize 40 -idist 7) */
    const int isize = 40, idist = 7;
    for (int i = 0; i < nslots; i++)
        if (slots[i].key[0]) slots[i].win = make_indicator();
    if (debug) fprintf(stderr, "xxri-wctl: %d dock slots\n", nslots);

    Client cl[MAX_CLIENTS];
    for (;;) {
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
                /* Measured from the ICON, not from the pill: derived from the
                   pill's lower edge it crept up under the icon and touched it. */
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
        }
        /* Publish where every dock icon is, so the window manager can make a
         * window come from - and go back to - its own icon.  A root property is
         * the natural channel: the dock helper is the only thing that knows the
         * geometry, the window manager is the only thing that can move windows,
         * and neither has to know the other exists. */
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
