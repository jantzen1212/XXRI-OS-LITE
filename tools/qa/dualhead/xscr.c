#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#include <stdio.h>
int main(void) {
    Display* d = XOpenDisplay(NULL); if (!d) return 1;
    Screen* s = DefaultScreenOfDisplay(d);
    printf("WidthOfScreen=%d HeightOfScreen=%d  XDisplayWidth=%d\n", WidthOfScreen(s), HeightOfScreen(s), XDisplayWidth(d, 0));
    int n = 0; XRRMonitorInfo* m = XRRGetMonitors(d, DefaultRootWindow(d), True, &n);
    for (int i = 0; m && i < n; i++) printf("  monitor %d: %dx%d+%d+%d primary=%d automatic=%d\n", i, m[i].width, m[i].height, m[i].x, m[i].y, m[i].primary, m[i].automatic);
    return 0;
}
