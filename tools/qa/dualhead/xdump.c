/* xdump OUT.ppm  - the whole root window as PPM, straight from the X server (QA only) */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
int main(int argc, char** argv) {
    Display* d = XOpenDisplay(NULL); if (!d) return 1;
    Window r = DefaultRootWindow(d);
    XWindowAttributes a; XGetWindowAttributes(d, r, &a);
    XImage* im = XGetImage(d, r, 0, 0, a.width, a.height, AllPlanes, ZPixmap); if (!im) return 2;
    FILE* f = fopen(argv[1], "wb"); fprintf(f, "P6\n%d %d\n255\n", a.width, a.height);
    for (int y = 0; y < a.height; y++) for (int x = 0; x < a.width; x++) {
        unsigned long p = XGetPixel(im, x, y);
        fputc((p >> 16) & 255, f); fputc((p >> 8) & 255, f); fputc(p & 255, f);
    }
    fclose(f); return 0;
}
