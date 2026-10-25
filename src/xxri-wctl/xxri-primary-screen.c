/* xxri-primary-screen.so - LD_PRELOAD for the dock (wbar.sh).
 *
 * wbar 2.3 places itself on the whole X screen: "bottom" is centred on
 * WidthOfScreen() and sits at HeightOfScreen(). With two displays extended
 * the screen spans both, so the dock hung across the gap between them.
 * wbar is Tiny Core's binary, so instead of patching it: once its display
 * is open, the default Screen's size becomes the primary RandR monitor's.
 * WidthOfScreen/HeightOfScreen are macros reading that struct, and wbar
 * snapshots the wallpaper behind it at its real window position, so the
 * pseudo-transparency stays right. Valid because xxri-display keeps the
 * primary display at 0,0; nothing changes when there is one display.
 *
 * LD_PRELOAD is dropped again at load, so the apps the dock starts get the
 * real screen size.
 */
#define _GNU_SOURCE
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#include <dlfcn.h>
#include <stdlib.h>

__attribute__((constructor)) static void drop_preload(void) { unsetenv("LD_PRELOAD"); }

Display *XOpenDisplay(const char *name)
{
    static Display *(*real)(const char *);
    if (!real) real = (Display *(*)(const char *))dlsym(RTLD_NEXT, "XOpenDisplay");
    if (!real) return NULL;
    Display *d = real(name);
    if (!d) return d;
    int ev, er, n = 0;
    if (!XRRQueryExtension(d, &ev, &er)) return d;
    XRRMonitorInfo *m = XRRGetMonitors(d, DefaultRootWindow(d), True, &n);
    for (int i = 0; m && i < n; i++) {
        if (!m[i].primary || m[i].x || m[i].y || m[i].width <= 0 || m[i].height <= 0) continue;
        Screen *s = DefaultScreenOfDisplay(d);
        if (m[i].width <= s->width && m[i].height <= s->height) {
            s->width = m[i].width;
            s->height = m[i].height;
        }
        break;
    }
    if (m) XRRFreeMonitors(m);
    return d;
}
