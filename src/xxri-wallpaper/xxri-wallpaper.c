/* xxri-wallpaper - paints the desktop background.
 *
 *   xxri-wallpaper [-f FIT] [-b #RRGGBB] IMAGE [IMAGE ...]
 *       One image per display in RandR monitor order (as `xxri-display
 *       monitors` numbers them); the last one repeats for the rest.
 *       FIT: fill (cover, the default), fit (bars in the background colour),
 *       stretch, center, tile.
 *   xxri-wallpaper -c CACHEDIR ...
 *       Reuse the rendered desktop while screen, displays, fit and pictures
 *       (path, size, mtime) are unchanged. .xsession paints at every login and
 *       decoding a big photo takes seconds on the slow machines.
 *   xxri-wallpaper -t IMAGE OUT.png WIDTH HEIGHT
 *       Cover-cropped PNG thumbnail for Settings (gdk-pixbuf here can't do JPEG).
 *
 * Replaces hsetroot 1.0.2, which has no multi-display support and whose
 * "-fill" stretches. JPEG and PNG only; xxri-desktop falls back to hsetroot
 * for anything else. Published the Esetroot way (RetainPermanent pixmap,
 * _XROOTPMAP_ID / ESETROOT_PMAP_ID), which the compositor and CC read.
 */
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xrandr.h>
#include <cairo.h>
#include <cairo-xlib.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>          /* before jpeglib.h, which uses FILE */
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <jpeglib.h>

enum { FIT_FILL, FIT_FIT, FIT_STRETCH, FIT_CENTER, FIT_TILE };

static int parse_fit(const char *s)
{
	/* "full" and "fill" are also hsetroot's words; xxri-desktop maps an old
	 * saved hsetroot "fill" (which stretched) to "stretch" itself */
	if (!strcmp(s, "fill") || !strcmp(s, "cover")) return FIT_FILL;
	if (!strcmp(s, "fit") || !strcmp(s, "full")) return FIT_FIT;
	if (!strcmp(s, "stretch")) return FIT_STRETCH;
	if (!strcmp(s, "center") || !strcmp(s, "centre")) return FIT_CENTER;
	if (!strcmp(s, "tile")) return FIT_TILE;
	return -1;
}

/* ---- decoding ----------------------------------------------------------- */

struct jerr { struct jpeg_error_mgr pub; jmp_buf jb; };
static void jerr_exit(j_common_ptr c) { longjmp(((struct jerr *)c->err)->jb, 1); }
static void jerr_quiet(j_common_ptr c) { (void)c; }

/* Much bigger than want_w x want_h (0 = full size)? Let the DCT decode at
 * 1/2, 1/4 or 1/8 - far less memory and time on old machines. Never below the
 * wanted size, so nothing gets upscaled. */
static cairo_surface_t *load_jpeg(const char *path, int want_w, int want_h)
{
	FILE *f = fopen(path, "rb");
	if (!f) return NULL;
	struct jpeg_decompress_struct ci;
	struct jerr je;
	cairo_surface_t *volatile s = NULL;
	unsigned char *volatile row = NULL;
	ci.err = jpeg_std_error(&je.pub);
	je.pub.error_exit = jerr_exit;
	je.pub.output_message = jerr_quiet;
	if (setjmp(je.jb)) {
		jpeg_destroy_decompress(&ci);
		fclose(f);
		free(row);
		if (s) cairo_surface_destroy(s);
		return NULL;
	}
	jpeg_create_decompress(&ci);
	jpeg_stdio_src(&ci, f);
	jpeg_read_header(&ci, TRUE);
	if (want_w > 0 && want_h > 0) {
		unsigned d = 1;
		while (d < 8 && ci.image_width / (d * 2) >= (unsigned)want_w &&
		       ci.image_height / (d * 2) >= (unsigned)want_h)
			d *= 2;
		ci.scale_num = 1;
		ci.scale_denom = d;
	}
	ci.out_color_space = JCS_RGB;   /* greyscale too; CMYK is refused */
	jpeg_start_decompress(&ci);
	int w = ci.output_width, h = ci.output_height;
	s = cairo_image_surface_create(CAIRO_FORMAT_RGB24, w, h);
	if (cairo_surface_status(s) != CAIRO_STATUS_SUCCESS) longjmp(je.jb, 1);
	unsigned char *data = cairo_image_surface_get_data(s);
	int stride = cairo_image_surface_get_stride(s);
	row = malloc((size_t)w * 3);
	if (!row) longjmp(je.jb, 1);
	while (ci.output_scanline < ci.output_height) {
		int y = ci.output_scanline;
		JSAMPROW rp = row;
		jpeg_read_scanlines(&ci, &rp, 1);
		uint32_t *out = (uint32_t *)(data + (size_t)y * stride);
		for (int x = 0; x < w; x++)
			out[x] = 0xFF000000u | (uint32_t)row[3 * x] << 16 |
			         (uint32_t)row[3 * x + 1] << 8 | row[3 * x + 2];
	}
	free(row);
	row = NULL;
	jpeg_finish_decompress(&ci);
	jpeg_destroy_decompress(&ci);
	fclose(f);
	cairo_surface_mark_dirty(s);
	return s;
}

static cairo_surface_t *load_image(const char *path, int want_w, int want_h)
{
	unsigned char m[8] = { 0 };
	FILE *f = fopen(path, "rb");
	if (!f) { fprintf(stderr, "xxri-wallpaper: cannot open %s\n", path); return NULL; }
	size_t n = fread(m, 1, sizeof m, f);
	fclose(f);
	cairo_surface_t *s = NULL;
	if (n >= 3 && m[0] == 0xFF && m[1] == 0xD8 && m[2] == 0xFF)
		s = load_jpeg(path, want_w, want_h);
	else if (n >= 8 && !memcmp(m, "\x89PNG\r\n\x1a\n", 8)) {
		s = cairo_image_surface_create_from_png(path);
		if (cairo_surface_status(s) != CAIRO_STATUS_SUCCESS) { cairo_surface_destroy(s); s = NULL; }
	} else {
		fprintf(stderr, "xxri-wallpaper: %s: not a JPEG or PNG image\n", path);
		return NULL;
	}
	if (!s) fprintf(stderr, "xxri-wallpaper: %s: cannot decode\n", path);
	return s;
}

/* ---- composition -------------------------------------------------------- */

static void paint_into(cairo_t *cr, cairo_surface_t *img, int fit, double bg[3],
                       double x, double y, double w, double h)
{
	double iw = cairo_image_surface_get_width(img);
	double ih = cairo_image_surface_get_height(img);
	double sx = 1, sy = 1, ox = x, oy = y;
	cairo_save(cr);
	cairo_rectangle(cr, x, y, w, h);
	cairo_clip(cr);
	cairo_set_source_rgb(cr, bg[0], bg[1], bg[2]);
	cairo_paint(cr);
	switch (fit) {
	case FIT_FILL:
	case FIT_FIT: {
		double s = fit == FIT_FILL ? (w / iw > h / ih ? w / iw : h / ih)
		                           : (w / iw < h / ih ? w / iw : h / ih);
		sx = sy = s;
		ox = x + (w - iw * s) / 2;
		oy = y + (h - ih * s) / 2;
		break; }
	case FIT_STRETCH:
		sx = w / iw; sy = h / ih;
		break;
	case FIT_CENTER:
		ox = x + (int)((w - iw) / 2);
		oy = y + (int)((h - ih) / 2);
		break;
	case FIT_TILE:
		break;
	}
	cairo_translate(cr, ox, oy);
	cairo_scale(cr, sx, sy);
	cairo_set_source_surface(cr, img, 0, 0);
	cairo_pattern_t *p = cairo_get_source(cr);
	cairo_pattern_set_filter(p, sx == 1 && sy == 1 ? CAIRO_FILTER_NEAREST : CAIRO_FILTER_GOOD);
	/* PAD keeps the filter from fading the edges into the background */
	cairo_pattern_set_extend(p, fit == FIT_TILE ? CAIRO_EXTEND_REPEAT :
	                            fit == FIT_FIT || fit == FIT_CENTER ? CAIRO_EXTEND_NONE :
	                            CAIRO_EXTEND_PAD);
	if (fit == FIT_FIT || fit == FIT_CENTER) {
		cairo_rectangle(cr, 0, 0, iw, ih);
		cairo_fill(cr);
	} else {
		cairo_paint(cr);
	}
	cairo_restore(cr);
}

typedef struct { int x, y, w, h; } Mon;

/* the displays, as RandR 1.5 lists its active monitors; the whole screen
 * when there is no RandR (Xfbdev) or it reports nothing */
static int monitors(Display *d, Window root, int sw, int sh, Mon *m, int max)
{
	int n = 0, ev, er, maj = 0, min = 0;
	if (XRRQueryExtension(d, &ev, &er) && XRRQueryVersion(d, &maj, &min) &&
	    (maj > 1 || (maj == 1 && min >= 5))) {
		int nm = 0;
		XRRMonitorInfo *mi = XRRGetMonitors(d, root, True, &nm);
		for (int i = 0; mi && i < nm && n < max; i++)
			if (mi[i].width > 0 && mi[i].height > 0)
				m[n++] = (Mon){ mi[i].x, mi[i].y, mi[i].width, mi[i].height };
		if (mi) XRRFreeMonitors(mi);
	}
	if (!n) m[n++] = (Mon){ 0, 0, sw, sh };
	return n;
}

static int ignore_x_errors(Display *d, XErrorEvent *e) { (void)d; (void)e; return 0; }

static Pixmap root_prop(Display *d, Window root, Atom a)
{
	Atom type; int fmt; unsigned long n = 0, after = 0; unsigned char *p = NULL;
	Pixmap pm = None;
	if (XGetWindowProperty(d, root, a, 0, 1, False, AnyPropertyType, &type, &fmt,
	                       &n, &after, &p) == Success && p) {
		if (type == XA_PIXMAP && n >= 1) pm = *(Pixmap *)p;
		XFree(p);
	}
	return pm;
}

static int parse_colour(const char *s, double c[3])
{
	unsigned v;
	if (!s || s[0] != '#' || strlen(s) != 7 || sscanf(s + 1, "%x", &v) != 1) return -1;
	c[0] = ((v >> 16) & 255) / 255.0; c[1] = ((v >> 8) & 255) / 255.0; c[2] = (v & 255) / 255.0;
	return 0;
}

/* ---- the rendered desktop, cached ------------------------------------------ */

#define CACHE_MAGIC "XXRIWP1\n"
static uint64_t fnv(uint64_t h, const void *p, size_t n)
{
	const unsigned char *b = p;
	while (n--) { h ^= *b++; h *= 1099511628211ULL; }
	return h;
}
static cairo_surface_t *cache_load(const char *path, int w, int h)
{
	FILE *f = fopen(path, "rb");
	if (!f) return NULL;
	char magic[8]; int hw = 0, hh = 0;
	if (fread(magic, 1, 8, f) != 8 || memcmp(magic, CACHE_MAGIC, 8) ||
	    fread(&hw, sizeof hw, 1, f) != 1 || fread(&hh, sizeof hh, 1, f) != 1 || hw != w || hh != h) {
		fclose(f); return NULL;
	}
	cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_RGB24, w, h);
	if (cairo_surface_status(s) != CAIRO_STATUS_SUCCESS) { cairo_surface_destroy(s); fclose(f); return NULL; }
	cairo_surface_flush(s);
	unsigned char *data = cairo_image_surface_get_data(s);
	int stride = cairo_image_surface_get_stride(s);
	for (int y = 0; y < h; y++)
		if (fread(data + (size_t)y * stride, 4, w, f) != (size_t)w) {
			fclose(f); cairo_surface_destroy(s); return NULL;   /* truncated: render */
		}
	fclose(f);
	cairo_surface_mark_dirty(s);
	return s;
}
static void cache_save(const char *dir, const char *path, cairo_surface_t *s)
{
	mkdir(dir, 0755);
	/* only the newest rendering is kept */
	DIR *dd = opendir(dir);
	if (dd) {
		struct dirent *e;
		while ((e = readdir(dd))) {
			size_t n = strlen(e->d_name);
			if (!strncmp(e->d_name, "root-", 5) && n > 4 && !strcmp(e->d_name + n - 4, ".bin")) {
				char old[4200];
				snprintf(old, sizeof old, "%s/%s", dir, e->d_name);
				unlink(old);
			}
		}
		closedir(dd);
	}
	char tmp[4200];
	snprintf(tmp, sizeof tmp, "%s.tmp", path);
	FILE *f = fopen(tmp, "wb");
	if (!f) return;
	int w = cairo_image_surface_get_width(s), h = cairo_image_surface_get_height(s);
	int stride = cairo_image_surface_get_stride(s);
	unsigned char *data = cairo_image_surface_get_data(s);
	int ok = fwrite(CACHE_MAGIC, 1, 8, f) == 8 && fwrite(&w, sizeof w, 1, f) == 1 && fwrite(&h, sizeof h, 1, f) == 1;
	for (int y = 0; ok && y < h; y++)
		ok = fwrite(data + (size_t)y * stride, 4, w, f) == (size_t)w;
	if (fclose(f) || !ok) { unlink(tmp); return; }
	rename(tmp, path);
}

static int usage(void)
{
	fprintf(stderr,
	        "usage: xxri-wallpaper [-f fill|fit|stretch|center|tile] [-b #RRGGBB] [-c CACHEDIR] IMAGE [IMAGE ...]\n"
	        "       xxri-wallpaper -t IMAGE OUT.png WIDTH HEIGHT\n");
	return 2;
}

static int thumbnail(const char *in, const char *out, int w, int h)
{
	if (w <= 0 || h <= 0 || w > 4096 || h > 4096) return usage();
	cairo_surface_t *img = load_image(in, w, h);
	if (!img) return 1;
	cairo_surface_t *t = cairo_image_surface_create(CAIRO_FORMAT_RGB24, w, h);
	cairo_t *cr = cairo_create(t);
	double bg[3] = { 0x16 / 255.0, 0x11 / 255.0, 0x2B / 255.0 };
	paint_into(cr, img, FIT_FILL, bg, 0, 0, w, h);
	cairo_destroy(cr);
	cairo_status_t st = cairo_surface_write_to_png(t, out);
	cairo_surface_destroy(t);
	cairo_surface_destroy(img);
	if (st != CAIRO_STATUS_SUCCESS) { fprintf(stderr, "xxri-wallpaper: cannot write %s\n", out); return 1; }
	return 0;
}

int main(int argc, char **argv)
{
	int fit = FIT_FILL, i = 1;
	const char *cachedir = NULL;
	/* the XXRI dark panel colour behind Fit/Center */
	double bg[3] = { 0x16 / 255.0, 0x11 / 255.0, 0x2B / 255.0 };

	if (argc == 6 && !strcmp(argv[1], "-t"))
		return thumbnail(argv[2], argv[3], atoi(argv[4]), atoi(argv[5]));
	for (; i < argc && argv[i][0] == '-'; i++) {
		if (!strcmp(argv[i], "-f") && i + 1 < argc) {
			if ((fit = parse_fit(argv[++i])) < 0) return usage();
		} else if (!strcmp(argv[i], "-b") && i + 1 < argc) {
			if (parse_colour(argv[++i], bg)) return usage();
		} else if (!strcmp(argv[i], "-c") && i + 1 < argc) {
			cachedir = argv[++i];
		} else return usage();
	}
	int nimg = argc - i;
	if (nimg < 1) return usage();

	Display *d = XOpenDisplay(NULL);
	if (!d) { fprintf(stderr, "xxri-wallpaper: cannot open display\n"); return 1; }
	int scr = DefaultScreen(d);
	Window root = RootWindow(d, scr);
	int sw = DisplayWidth(d, scr), sh = DisplayHeight(d, scr);
	Mon mon[16];
	int nm = monitors(d, root, sw, sh, mon, 16);

	/* what this rendering depends on: the screen, the displays, the fit and
	 * colour, and each picture's path, size and modification time */
	char cpath[4200] = "";
	cairo_surface_t *all = NULL;
	cairo_t *cr;
	if (cachedir) {
		uint64_t key = 1469598103934665603ULL;
		int dep = DefaultDepth(d, scr);
		key = fnv(key, CACHE_MAGIC, 8);
		key = fnv(key, &sw, sizeof sw); key = fnv(key, &sh, sizeof sh); key = fnv(key, &dep, sizeof dep);
		key = fnv(key, &fit, sizeof fit); key = fnv(key, bg, sizeof bg); key = fnv(key, &nm, sizeof nm);
		for (int k = 0; k < nm; k++) {
			const char *path = argv[i + (k < nimg ? k : nimg - 1)];
			struct stat st;
			long long sz = 0, mt = 0;
			if (!stat(path, &st)) { sz = st.st_size; mt = st.st_mtime; }
			key = fnv(key, &mon[k], sizeof mon[k]);
			key = fnv(key, path, strlen(path) + 1);
			key = fnv(key, &sz, sizeof sz); key = fnv(key, &mt, sizeof mt);
		}
		snprintf(cpath, sizeof cpath, "%s/root-%016llx.bin", cachedir, (unsigned long long)key);
		all = cache_load(cpath, sw, sh);
	}
	int ok = 0;
	if (all) {
		ok = nm;
		goto publish;
	}

	/* the whole root, composed client side and uploaded once */
	all = cairo_image_surface_create(CAIRO_FORMAT_RGB24, sw, sh);
	cr = cairo_create(all);
	cairo_set_source_rgb(cr, bg[0], bg[1], bg[2]);
	cairo_paint(cr);
	cairo_surface_t *img = NULL;
	const char *loaded = NULL;
	for (int k = 0; k < nm; k++) {
		const char *path = argv[i + (k < nimg ? k : nimg - 1)];
		/* a monitor showing the same file as the previous one reuses it -
		 * unless it is larger, then decode again at the size it needs */
		if (!loaded || strcmp(loaded, path) ||
		    (img && (cairo_image_surface_get_width(img) < mon[k].w ||
		             cairo_image_surface_get_height(img) < mon[k].h))) {
			if (img) cairo_surface_destroy(img);
			img = load_image(path, fit == FIT_FILL || fit == FIT_FIT || fit == FIT_STRETCH ? mon[k].w : 0,
			                 fit == FIT_FILL || fit == FIT_FIT || fit == FIT_STRETCH ? mon[k].h : 0);
			loaded = path;
		}
		if (!img) continue;
		paint_into(cr, img, fit, bg, mon[k].x, mon[k].y, mon[k].w, mon[k].h);
		ok++;
	}
	if (img) cairo_surface_destroy(img);
	cairo_destroy(cr);
	if (!ok) { cairo_surface_destroy(all); XCloseDisplay(d); return 1; }
	if (cachedir && ok == nm) cache_save(cachedir, cpath, all);

publish:;
	Pixmap pm = XCreatePixmap(d, root, sw, sh, DefaultDepth(d, scr));
	cairo_surface_t *xs = cairo_xlib_surface_create(d, pm, DefaultVisual(d, scr), sw, sh);
	cr = cairo_create(xs);
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_surface(cr, all, 0, 0);
	cairo_paint(cr);
	cairo_destroy(cr);
	cairo_surface_flush(xs);
	cairo_surface_destroy(xs);
	cairo_surface_destroy(all);

	/* Esetroot protocol: free the previous setter's pixmap (it was kept alive
	 * the same way), then publish ours */
	Atom a_root = XInternAtom(d, "_XROOTPMAP_ID", False);
	Atom a_eset = XInternAtom(d, "ESETROOT_PMAP_ID", False);
	Pixmap old_root = root_prop(d, root, a_root), old_eset = root_prop(d, root, a_eset);
	XSetErrorHandler(ignore_x_errors);
	if (old_root != None && old_root == old_eset)
		XKillClient(d, old_root);
	XSync(d, False);
	XChangeProperty(d, root, a_root, XA_PIXMAP, 32, PropModeReplace, (unsigned char *)&pm, 1);
	XChangeProperty(d, root, a_eset, XA_PIXMAP, 32, PropModeReplace, (unsigned char *)&pm, 1);
	XSetWindowBackgroundPixmap(d, root, pm);
	XClearWindow(d, root);
	XSetCloseDownMode(d, RetainPermanent);
	XSync(d, False);
	XCloseDisplay(d);
	return ok == nm ? 0 : 1;
}
