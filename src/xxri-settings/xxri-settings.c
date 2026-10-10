/* xxri-settings - the Settings app.
 *
 * Frontend only: every hardware or system action goes through an xxri-*
 * backend (hardware layer, xxri-app, xxri-desktop, xxri-system-update...).
 */
#include <gtk/gtk.h>
#define GETTEXT_PACKAGE "xxri-settings"
#include "xxri-i18n.h"
#include "xxri-chrome.h"
#include "xxri-dialog.h"
#include <cairo.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>

#define ICONDIR "/usr/local/share/xxri-settings/icons"


/* TC's gdk-pixbuf has broken built-in PNG/JPEG loaders, go through cairo */
static GdkPixbuf* load_png(const char* path, int w, int h) {
    cairo_surface_t* s = cairo_image_surface_create_from_png(path);
    if (cairo_surface_status(s) != CAIRO_STATUS_SUCCESS) { cairo_surface_destroy(s); return NULL; }
    int sw = cairo_image_surface_get_width(s), sh = cairo_image_surface_get_height(s);
    GdkPixbuf* full = gdk_pixbuf_get_from_surface(s, 0, 0, sw, sh);
    cairo_surface_destroy(s);
    if (!full) return NULL;
    if (w>0 && (sw!=w || sh!=h)) { GdkPixbuf* sc=gdk_pixbuf_scale_simple(full,w,h,GDK_INTERP_BILINEAR); g_object_unref(full); return sc; }
    return full;
}

static char* run_cmd(const char* fmt, ...) {
    char cmd[1024]; va_list ap; va_start(ap, fmt);
    vsnprintf(cmd, sizeof cmd, fmt, ap); va_end(ap);
    char full[1100]; snprintf(full, sizeof full, "%s 2>/dev/null", cmd);
    FILE* f = popen(full, "r");
    if (!f) return g_strdup("");
    GString* s = g_string_new("");
    char buf[512]; size_t n;
    while ((n = fread(buf,1,sizeof buf,f)) > 0) g_string_append_len(s, buf, n);
    pclose(f);
    while (s->len && (s->str[s->len-1]=='\n'||s->str[s->len-1]=='\r'))
        g_string_truncate(s, s->len-1);
    return g_string_free(s, FALSE);
}
static void run_bg(const char* fmt, ...) {
    char cmd[1024]; va_list ap; va_start(ap, fmt);
    vsnprintf(cmd, sizeof cmd, fmt, ap); va_end(ap);
    char full[1100]; snprintf(full, sizeof full, "%s >/dev/null 2>&1 &", cmd);
    int r = system(full); (void)r;
}
/* flat-JSON field extraction (backends emit stable flat schemas) */
static char* jget(const char* doc, const char* key) {
    char pat[128]; snprintf(pat, sizeof pat, "\"%s\":", key);
    /* skip object/array values: xxri-audio emits {"volume":{"volume":65,...}}
       and the envelope key would match first */
    const char* p = doc;
    for (;;) {
        p = strstr(p, pat);
        if (!p) return g_strdup("");
        const char* v = p + strlen(pat);
        while (*v == ' ') v++;
        if (*v != '{' && *v != '[') break;
        p = v;
    }
    p += strlen(pat);
    while (*p == ' ') p++;
    if (*p == '"') {
        p++; const char* e = p;
        while (*e && !(*e=='"' && e[-1]!='\\')) e++;
        GString* s = g_string_new_len(NULL, 0);
        for (const char* c=p; c<e; c++) {
            if (*c=='\\' && c+1<e) { c++; g_string_append_c(s, *c=='n'?'\n':*c); }
            else g_string_append_c(s, *c);
        }
        return g_string_free(s, FALSE);
    }
    const char* e = p;
    while (*e && *e!=',' && *e!='}' && *e!=']') e++;
    return g_strndup(p, e-p);
}
static gboolean jbool(const char* doc, const char* key) {
    char* v = jget(doc, key); gboolean b = !strcmp(v,"true"); g_free(v); return b;
}
/* split "key":[{..},{..}] into an array of object bodies */
static GPtrArray* jarr(const char* doc, const char* key) {
    GPtrArray* v = g_ptr_array_new_with_free_func(g_free);
    char pat[128]; snprintf(pat, sizeof pat, "\"%s\":[", key);
    const char* p = strstr(doc, pat);
    if (!p) return v;
    p += strlen(pat);
    int depth=0; const char* start=NULL;
    for (const char* i=p; *i; i++) {
        if (*i=='{') { if (depth==0) start=i; depth++; }
        else if (*i=='}') { depth--; if (depth==0 && start) g_ptr_array_add(v, g_strndup(start, i-start+1)); }
        else if (*i==']' && depth==0) break;
    }
    return v;
}

static GtkWidget* g_stack;
static GtkWidget* g_win;
static const char* g_start_id = "wifi";
typedef struct { const char* id; const char* label; const char* icon; int group; } Nav;
static const Nav NAV[] = {
    {"wifi",N_("Wi-Fi & Network"),"wifi",0},
    {"bluetooth",N_("Bluetooth"),"bluetooth",0},
    {"devices",N_("Connected Devices"),"devices",0},
    {"wallpaper",N_("Wallpaper & Style"),"wallpaper",1},
    {"sound",N_("Sounds & Vibration"),"sound",1},
    {"display",N_("Display"),"display",1},
    {"storage",N_("Storage"),"storage",2},
    {"battery",N_("Battery"),"battery",2},
    {"apps",N_("Apps"),"apps",2},
    {"general",N_("General Management"),"general",3},
    {"update",N_("Software Update"),"update",3},
    {"about",N_("About Device"),"about",3},
};
static const int NNAV = G_N_ELEMENTS(NAV);

static void css(GtkWidget* w, const char* cls){ gtk_style_context_add_class(gtk_widget_get_style_context(w), cls); }
static GtkWidget* img(const char* icon, int sz) {
    char p[256]; snprintf(p,sizeof p,"%s/%s-%d.png", ICONDIR, icon, sz>=40?40:22);
    GdkPixbuf* pb = load_png(p, sz, sz);
    GtkWidget* i = pb ? gtk_image_new_from_pixbuf(pb) : gtk_image_new();
    if (pb) g_object_unref(pb);
    gtk_widget_set_size_request(i, sz, sz);
    return i;
}
static GtkWidget* label_cls(const char* text, const char* cls, gfloat xalign) {
    GtkWidget* l = gtk_label_new(text);
    gtk_label_set_xalign(GTK_LABEL(l), xalign);
    gtk_label_set_line_wrap(GTK_LABEL(l), TRUE);
    gtk_label_set_line_wrap_mode(GTK_LABEL(l), PANGO_WRAP_WORD_CHAR);
    if (cls) css(l, cls);
    return l;
}
static GtkWidget* section(const char* t) { return label_cls(t, "xxri-section", 0); }

/* a rounded card containing a vertical list of rows */
static GtkWidget* card_new(void) {
    GtkWidget* c = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    css(c, "xxri-card");
    return c;
}
/* a key/value row (One UI style) added to a card */
static void card_kv(GtkWidget* card, const char* key, const char* val) {
    GtkWidget* r = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    css(r, "xxri-rowitem");
    gtk_box_pack_start(GTK_BOX(r), label_cls(key,"xxri-key",0), FALSE, FALSE, 0);
    if (val && *val) gtk_box_pack_start(GTK_BOX(r), label_cls(val,"xxri-val",0), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(card), r, FALSE, FALSE, 0);
}
/* a row with a left title/subtitle and a right-hand widget (switch/button) */
static GtkWidget* card_action(GtkWidget* card, const char* title, const char* sub, GtkWidget* right) {
    GtkWidget* r = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    css(r, "xxri-rowitem");
    GtkWidget* tv = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_box_pack_start(GTK_BOX(tv), label_cls(title,"xxri-row-title",0), FALSE, FALSE, 0);
    if (sub && *sub) gtk_box_pack_start(GTK_BOX(tv), label_cls(sub,"xxri-row-sub",0), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(r), tv, TRUE, TRUE, 0);
    if (right) { gtk_widget_set_valign(right, GTK_ALIGN_CENTER); gtk_box_pack_end(GTK_BOX(r), right, FALSE, FALSE, 0); }
    gtk_box_pack_start(GTK_BOX(card), r, FALSE, FALSE, 0);
    return r;
}
static void card_icon_row(GtkWidget* card, const char* icon, const char* title, const char* sub, GtkWidget* right) {
    GtkWidget* r = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    css(r, "xxri-rowitem");
    if (icon) { GtkWidget* ic=img(icon,28); gtk_widget_set_valign(ic,GTK_ALIGN_CENTER); gtk_box_pack_start(GTK_BOX(r), ic, FALSE, FALSE, 0); }
    GtkWidget* tv = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_valign(tv, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(tv), label_cls(title,"xxri-row-title",0), FALSE, FALSE, 0);
    if (sub && *sub) gtk_box_pack_start(GTK_BOX(tv), label_cls(sub,"xxri-row-sub",0), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(r), tv, TRUE, TRUE, 0);
    if (right) { gtk_widget_set_valign(right, GTK_ALIGN_CENTER); gtk_box_pack_end(GTK_BOX(r), right, FALSE, FALSE, 0); }
    gtk_box_pack_start(GTK_BOX(card), r, FALSE, FALSE, 0);
}
static GtkWidget* hint(const char* msg) {
    GtkWidget* l = label_cls(msg, "xxri-hint", 0);
    gtk_widget_set_halign(l, GTK_ALIGN_FILL);
    return l;
}
/* A full-page, vertically-centered empty / unavailable state: a soft badge
 * holding the section icon, a bold title and a wrapped muted subtitle.  Pack
 * it into the page with expand=TRUE so it centres in the content area.     */
static GtkWidget* empty_state(const char* icon, const char* title, const char* sub) {
    GtkWidget* w = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_widget_set_valign(w, GTK_ALIGN_CENTER);
    gtk_widget_set_halign(w, GTK_ALIGN_CENTER);
    GtkWidget* badge = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    css(badge, "xxri-empty-badge");
    gtk_widget_set_halign(badge, GTK_ALIGN_CENTER);
    GtkWidget* ic = img(icon, 40);
    gtk_widget_set_halign(ic, GTK_ALIGN_CENTER); gtk_widget_set_valign(ic, GTK_ALIGN_CENTER);
    gtk_widget_set_hexpand(ic, TRUE); gtk_widget_set_vexpand(ic, TRUE);
    gtk_container_add(GTK_CONTAINER(badge), ic);
    gtk_box_pack_start(GTK_BOX(w), badge, FALSE, FALSE, 0);
    GtkWidget* t = label_cls(title, "xxri-empty-title", 0.5);
    gtk_label_set_justify(GTK_LABEL(t), GTK_JUSTIFY_CENTER);
    gtk_box_pack_start(GTK_BOX(w), t, FALSE, FALSE, 0);
    if (sub && *sub) {
        GtkWidget* s = label_cls(sub, "xxri-empty-sub", 0.5);
        gtk_label_set_justify(GTK_LABEL(s), GTK_JUSTIFY_CENTER);
        gtk_widget_set_size_request(s, 380, -1);
        gtk_box_pack_start(GTK_BOX(w), s, FALSE, FALSE, 0);
    }
    return w;
}
/* pack a full-page empty state (expands to centre vertically) */
static void page_empty(GtkWidget* box, const char* icon, const char* title, const char* sub) {
    gtk_box_pack_start(GTK_BOX(box), empty_state(icon, title, sub), TRUE, TRUE, 0);
}
static GtkWidget* accent_btn(const char* text) {
    GtkWidget* b = gtk_button_new_with_label(text); css(b,"xxri-accent"); return b;
}
/* a backend refresh rate as people write it: "60.00" -> "60 Hz", "59.94" -> "59.94 Hz" */
static char* hz_label(const char* rate) {
    /* a rate of 0 is RandR saying it does not know (virtual outputs) */
    if (!rate || !*rate || !strcmp(rate,"null") || g_ascii_strtod(rate, NULL) <= 0) return g_strdup("");
    char b[32]; snprintf(b, sizeof b, "%s", rate);
    char* dot = strchr(b, '.');
    if (dot) { char* e = b + strlen(b) - 1; while (e > dot && *e == '0') *e-- = 0; if (e == dot) *e = 0; }
    return g_strdup_printf("%s Hz", b);
}

static void rebuild(const char* id);

/* Each builder fills a content vbox (already inside a scrolled, width-capped
 * column) for the given page id.  Backends provide all live data.          */

/* Connected Devices */
static void devgroup(GtkWidget* box, const char* sec, const char* json, const char* arrkey,
                     const char* titlekey, const char* subkey, const char* icon, const char* empty) {
    gtk_box_pack_start(GTK_BOX(box), section(sec), FALSE, FALSE, 0);
    GPtrArray* a = jarr(json, arrkey);
    if (!a->len) { gtk_box_pack_start(GTK_BOX(box), hint(empty), FALSE, FALSE, 0); g_ptr_array_free(a,TRUE); return; }
    GtkWidget* c = card_new();
    for (guint i=0;i<a->len;i++){ char*o=a->pdata[i]; char*t=jget(o,titlekey);char*s=jget(o,subkey);
        card_icon_row(c, icon, *t?t:"device", s, NULL); g_free(t);g_free(s);}
    gtk_box_pack_start(GTK_BOX(box), c, FALSE, FALSE, 0);
    g_ptr_array_free(a,TRUE);
}
static void build_devices(GtkWidget* box) {
    char* usb=run_cmd("xxri-hardware usb --json");
    devgroup(box,_("USB Devices"),usb,"usb","product","manufacturer","devices",_("No USB devices connected."));
    char* inp=run_cmd("xxri-input devices --json");
    /* mouse + keyboard from input backend */
    gtk_box_pack_start(GTK_BOX(box), section(_("Keyboard & Mouse")), FALSE, FALSE, 0);
    GPtrArray* id=jarr(inp,"devices"); GtkWidget* ic=card_new(); gboolean anyi=FALSE;
    for (guint i=0;i<id->len;i++){char*o=id->pdata[i];char*t=jget(o,"type");char*n=jget(o,"name");
        if(!strcmp(t,"keyboard")||!strcmp(t,"mouse")||!strcmp(t,"touchpad")){anyi=TRUE;
            card_icon_row(ic,!strcmp(t,"keyboard")?"general":"devices",n,t,NULL);} g_free(t);g_free(n);}
    if(!anyi) card_kv(ic,_("No input devices"),"");
    gtk_box_pack_start(GTK_BOX(box), ic, FALSE, FALSE, 0);
    g_ptr_array_free(id,TRUE);
    char* cam=run_cmd("xxri-hardware camera --json");
    devgroup(box,_("Cameras"),cam,"devices","name","device","camera",_("No camera detected. V4L2 webcam drivers install on demand."));
    char* snd=run_cmd("xxri-audio list --json");
    gtk_box_pack_start(GTK_BOX(box), section(_("Audio")), FALSE, FALSE, 0);
    GPtrArray* sc=jarr(snd,"cards"); GtkWidget* ac=card_new();
    if(!sc->len) card_kv(ac,_("No sound card"),"");
    for(guint i=0;i<sc->len;i++){char*o=sc->pdata[i];char*n=jget(o,"name");char*ty=jget(o,"type");
        card_icon_row(ac,"sound",n,ty,NULL);g_free(n);g_free(ty);}
    gtk_box_pack_start(GTK_BOX(box), ac, FALSE, FALSE, 0);
    g_ptr_array_free(sc,TRUE);
    char* dsp=run_cmd("xxri-display current --json");
    gtk_box_pack_start(GTK_BOX(box), section(_("Displays")), FALSE, FALSE, 0);
    GtkWidget* dc=card_new(); char*mode=jget(dsp,"mode");char*drv=jget(dsp,"driver");
    char* hz=hz_label(jget(dsp,"refresh"));
    char dsub[96]; snprintf(dsub,sizeof dsub,_("driver %s%s%s"), drv, *hz?" · ":"", hz);
    card_icon_row(dc,"display",mode,dsub,NULL); g_free(hz);
    gtk_box_pack_start(GTK_BOX(box), dc, FALSE, FALSE, 0);
    char* pr=run_cmd("xxri-hardware printer --json");
    devgroup(box,_("Printers"),pr,"devices","product","device","devices",_("No printer detected."));
    g_free(usb);g_free(inp);g_free(cam);g_free(snd);g_free(dsp);g_free(pr);g_free(mode);g_free(drv);
}

/* ---- shared: shapes, background commands, picture previews ---------------- */
static void rrect(cairo_t* cr, double x, double y, double w, double h, double r) {
    cairo_new_sub_path(cr);
    cairo_arc(cr, x+w-r, y+r,   r, -G_PI/2, 0);
    cairo_arc(cr, x+w-r, y+h-r, r, 0, G_PI/2);
    cairo_arc(cr, x+r,   y+h-r, r, G_PI/2, G_PI);
    cairo_arc(cr, x+r,   y+r,   r, G_PI, 1.5*G_PI);
    cairo_close_path(cr);
}
/* slow backend calls (wallpaper render, update check) without freezing the
 * window; `done` gets the output, or NULL if it couldn't run */
typedef void (*AsyncDone)(const char* out, gboolean ok, gpointer tag);
typedef struct { AsyncDone done; gpointer tag; } AsyncCall;
static void async_finished(GObject* src, GAsyncResult* res, gpointer u) {
    AsyncCall* c = (AsyncCall*)u;
    char* out = NULL;
    gboolean ok = g_subprocess_communicate_utf8_finish(G_SUBPROCESS(src), res, &out, NULL, NULL);
    ok = ok && g_subprocess_get_successful(G_SUBPROCESS(src));
    if (c->done) c->done(out ? out : "", ok, c->tag);
    g_free(out); g_free(c);
    g_object_unref(src);
}
static void run_async(const char* const* argv, AsyncDone done, gpointer tag) {
    GSubprocess* p = g_subprocess_newv(argv, G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_SILENCE, NULL);
    if (!p) { if (done) done(NULL, FALSE, tag); return; }
    AsyncCall* c = g_new0(AsyncCall, 1); c->done = done; c->tag = tag;
    g_subprocess_communicate_utf8_async(p, NULL, NULL, async_finished, c);
}
/* previews are PNGs from `xxri-desktop wallpaper thumb`: gdk-pixbuf here
 * can't decode JPEG */
static GHashTable* g_thumbs;            /* "file|WxH" -> cairo_surface_t*, per run */
static cairo_surface_t* thumb(const char* file, int w, int h) {
    if (!file || !*file) return NULL;
    if (!g_thumbs) g_thumbs = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify)cairo_surface_destroy);
    char* key = g_strdup_printf("%s|%dx%d", file, w, h);
    cairo_surface_t* s = (cairo_surface_t*)g_hash_table_lookup(g_thumbs, key);
    if (s) { g_free(key); return s; }
    char* q = g_shell_quote(file);
    char* png = run_cmd("xxri-desktop wallpaper thumb %s %d %d", q, w, h);
    g_free(q);
    if (*png) {
        s = cairo_image_surface_create_from_png(png);
        if (cairo_surface_status(s) != CAIRO_STATUS_SUCCESS) { cairo_surface_destroy(s); s = NULL; }
    }
    g_free(png);
    if (s) g_hash_table_insert(g_thumbs, key, s); else g_free(key);
    return s;
}
/* paint a picture over a rectangle, cropping the overflow (the desktop's Fill) */
static void paint_cover(cairo_t* cr, cairo_surface_t* s, double x, double y, double w, double h) {
    double iw = cairo_image_surface_get_width(s), ih = cairo_image_surface_get_height(s);
    double k = MAX(w/iw, h/ih);
    cairo_save(cr);
    cairo_rectangle(cr, x, y, w, h); cairo_clip(cr);
    cairo_translate(cr, x + (w - iw*k)/2, y + (h - ih*k)/2);
    cairo_scale(cr, k, k);
    cairo_set_source_surface(cr, s, 0, 0);
    cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_GOOD);
    cairo_paint(cr);
    cairo_restore(cr);
}
static void xxri_gradient_ring(cairo_t* cr, double x, double y, double w, double h) {
    cairo_pattern_t* g = cairo_pattern_create_linear(x, y, x+w, y+h);
    /* the primary blue of the stylesheet (xxri_blue_d -> xxri_blue_l) */
    cairo_pattern_add_color_stop_rgb(g, 0, 0x1E/255.0, 0x4F/255.0, 0xC2/255.0);
    cairo_pattern_add_color_stop_rgb(g, 1, 0x4A/255.0, 0x9A/255.0, 0xF5/255.0);
    cairo_set_source(cr, g); cairo_pattern_destroy(g);
}
static void pill_text(cairo_t* cr, double x, double y, const char* text, gboolean dark) {
    PangoLayout* l = pango_cairo_create_layout(cr);
    PangoFontDescription* fd = pango_font_description_from_string("Avenir Next Bold 8");
    pango_layout_set_font_description(l, fd); pango_font_description_free(fd);
    pango_layout_set_text(l, text, -1);
    int tw, th; pango_layout_get_pixel_size(l, &tw, &th);
    rrect(cr, x, y, tw + 14, th + 4, (th + 4) / 2.0);
    if (dark) cairo_set_source_rgba(cr, 0x24/255.0, 0x21/255.0, 0x24/255.0, 0.88);
    else cairo_set_source_rgba(cr, 1, 1, 1, 0.94);
    cairo_fill(cr);
    if (dark) cairo_set_source_rgb(cr, 1, 1, 1);
    else cairo_set_source_rgb(cr, 0x1E/255.0, 0x4F/255.0, 0xC2/255.0);
    cairo_move_to(cr, x + 7, y + 2);
    pango_cairo_show_layout(cr, l);
    g_object_unref(l);
}

/* ---- dropdown
 * GtkComboBox's arrow comes from the icon theme (pan-down-symbolic), which
 * can't be loaded here. Same pill with a cairo chevron, opens a GtkMenu. */
typedef void (*DropChanged)(GtkWidget* drop, const char* id, gpointer u);
typedef struct { GtkWidget* label; GPtrArray* ids; GPtrArray* labels; char* active; DropChanged cb; gpointer u; } Drop;
static void drop_free(gpointer p) {
    Drop* d = (Drop*)p; g_ptr_array_unref(d->ids); g_ptr_array_unref(d->labels); g_free(d->active); g_free(d);
}
static gboolean draw_chevron(GtkWidget* w, cairo_t* cr, gpointer u) {
    (void)u;
    double W = gtk_widget_get_allocated_width(w), H = gtk_widget_get_allocated_height(w);
    GdkRGBA c; gtk_style_context_get_color(gtk_widget_get_style_context(w), gtk_widget_get_state_flags(w), &c);
    gdk_cairo_set_source_rgba(cr, &c);
    cairo_set_line_width(cr, 1.7); cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND); cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    cairo_move_to(cr, W/2 - 4, H/2 - 1.5); cairo_line_to(cr, W/2, H/2 + 2.5); cairo_line_to(cr, W/2 + 4, H/2 - 1.5);
    cairo_stroke(cr);
    return FALSE;
}
static Drop* drop_of(GtkWidget* w) { return (Drop*)g_object_get_data(G_OBJECT(w), "xxri-drop"); }
static void drop_set_active(GtkWidget* w, const char* id) {
    Drop* d = drop_of(w);
    for (guint i = 0; i < d->ids->len; i++)
        if (!g_strcmp0((char*)d->ids->pdata[i], id)) {
            gtk_label_set_text(GTK_LABEL(d->label), (char*)d->labels->pdata[i]);
            g_free(d->active); d->active = g_strdup(id);
        }
}
static const char* drop_get_active(GtkWidget* w) { return drop_of(w)->active; }
static void drop_append(GtkWidget* w, const char* id, const char* label) {
    Drop* d = drop_of(w);
    g_ptr_array_add(d->ids, g_strdup(id)); g_ptr_array_add(d->labels, g_strdup(label));
    if (!d->active) drop_set_active(w, id);
}
static void drop_pick(GtkMenuItem* it, gpointer u) {
    GtkWidget* w = GTK_WIDGET(u); Drop* d = drop_of(w);
    const char* id = (const char*)g_object_get_data(G_OBJECT(it), "id");
    if (!g_strcmp0(id, d->active)) return;
    drop_set_active(w, id);
    if (d->cb) d->cb(w, d->active, d->u);
}
static void drop_clicked(GtkButton* b, gpointer u) {
    (void)u; GtkWidget* w = GTK_WIDGET(b); Drop* d = drop_of(w);
    GtkWidget* m = gtk_menu_new(); css(m, "xxri-drop-menu");
    for (guint i = 0; i < d->ids->len; i++) {
        GtkWidget* it = gtk_menu_item_new_with_label((char*)d->labels->pdata[i]);
        if (!g_strcmp0((char*)d->ids->pdata[i], d->active)) css(it, "xxri-drop-current");
        g_object_set_data_full(G_OBJECT(it), "id", g_strdup((char*)d->ids->pdata[i]), g_free);
        g_signal_connect(it, "activate", G_CALLBACK(drop_pick), w);
        gtk_menu_shell_append(GTK_MENU_SHELL(m), it);
    }
    gtk_widget_show_all(m);
    gtk_widget_set_size_request(m, gtk_widget_get_allocated_width(w), -1);
    gtk_menu_attach_to_widget(GTK_MENU(m), w, NULL);
    gtk_menu_popup_at_widget(GTK_MENU(m), w, GDK_GRAVITY_SOUTH_WEST, GDK_GRAVITY_NORTH_WEST, NULL);
}
static GtkWidget* drop_new(DropChanged cb, gpointer u) {
    GtkWidget* b = gtk_button_new(); css(b, "xxri-drop");
    Drop* d = g_new0(Drop, 1);
    d->ids = g_ptr_array_new_with_free_func(g_free); d->labels = g_ptr_array_new_with_free_func(g_free);
    d->cb = cb; d->u = u;
    GtkWidget* hb = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    d->label = gtk_label_new(""); gtk_label_set_xalign(GTK_LABEL(d->label), 0);
    gtk_box_pack_start(GTK_BOX(hb), d->label, TRUE, TRUE, 0);
    GtkWidget* ch = gtk_drawing_area_new(); gtk_widget_set_size_request(ch, 12, 12);
    gtk_widget_set_valign(ch, GTK_ALIGN_CENTER); css(ch, "xxri-drop-chevron");
    g_signal_connect(ch, "draw", G_CALLBACK(draw_chevron), NULL);
    gtk_box_pack_end(GTK_BOX(hb), ch, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(b), hb);
    g_object_set_data_full(G_OBJECT(b), "xxri-drop", d, drop_free);
    g_signal_connect(b, "clicked", G_CALLBACK(drop_clicked), NULL);
    return b;
}

/* dialog answers shared by the pages that may restart the desktop */
enum { RES_LATER = 1, RES_NOW = 2 };

/* ---- Wallpaper & Style
 * Preview of the desktop, the official wallpapers, Browse and fit, then the
 * look (mode, theme, cursor, scale) and dock size. Right-click a wallpaper to
 * pick a display. All done by xxri-desktop (renderer: xxri-wallpaper). */
#define WP_MAXMON 8
typedef struct { char* file; char* name; gboolean def; GtkWidget* area; } WallTile;
static GPtrArray* g_wtiles;             /* WallTile* of the page on screen */
static char*   g_wp_disp[WP_MAXMON];     /* what each display shows */
static char*   g_wp_lbl[WP_MAXMON];      /* "HDMI Display" ... */
static int     g_wp_mon[WP_MAXMON][4];   /* x y w h */
static int     g_nmon = 1;
static char*   g_wp_default;
static GtkWidget* g_wp_hero;
static GtkWidget* g_wp_hero_title;
static GtkWidget* g_wp_hero_sub;
static char*   g_wp_fit;

static void wall_tile_free(gpointer p) { WallTile* t = (WallTile*)p; g_free(t->file); g_free(t->name); g_free(t); }
static const char* wall_name(const char* file) {
    for (guint i = 0; g_wtiles && i < g_wtiles->len; i++) {
        WallTile* t = (WallTile*)g_wtiles->pdata[i];
        if (!strcmp(t->file, file)) return t->name;
    }
    return NULL;
}
static const char* fit_label(const char* f) {
    if (!f) return _("Fill");
    if (!strcmp(f,"fit")) return _("Fit");
    if (!strcmp(f,"stretch")) return _("Stretch");
    if (!strcmp(f,"center")) return _("Center");
    if (!strcmp(f,"tile")) return _("Tile");
    return _("Fill");
}
/* the words under the preview: which picture, official or not, default or not */
static void wall_hero_text(void) {
    if (!g_wp_hero_title) return;
    gboolean same = TRUE;
    for (int i = 1; i < g_nmon; i++) if (g_strcmp0(g_wp_disp[i], g_wp_disp[0])) same = FALSE;
    const char* f = g_wp_disp[0] ? g_wp_disp[0] : "";
    const char* nm = wall_name(f);
    char* base = g_path_get_basename(f);
    char* dot = strrchr(base, '.'); if (dot && dot != base) *dot = 0;
    char title[256], sub[256];
    if (!same) snprintf(title, sizeof title, "%s", _("A different wallpaper on each display"));
    else snprintf(title, sizeof title, "%s", nm ? nm : base);
    snprintf(sub, sizeof sub, "%s%s  \xc2\xb7  %s%s",
             !same ? _("Right-click a wallpaper to choose one per display") :
             nm ? _("Official XXRI wallpaper") : _("Your picture"),
             same && nm && !g_strcmp0(f, g_wp_default) ? _(" \xc2\xb7 Default") : "",
             fit_label(g_wp_fit),
             g_nmon > 1 ? (same ? _("  \xc2\xb7  All displays") : "") : "");
    gtk_label_set_text(GTK_LABEL(g_wp_hero_title), title);
    gtk_label_set_text(GTK_LABEL(g_wp_hero_sub), sub);
    g_free(base);
}

/* one display: bezel, the wallpaper, an XXRI window and the dock on it */
static void draw_monitor(cairo_t* cr, double x, double y, double w, double h, const char* file, int number) {
    double b = MAX(4, w * 0.035);
    /* stand */
    cairo_pattern_t* g = cairo_pattern_create_linear(0, y+h, 0, y+h+h*0.16);
    cairo_pattern_add_color_stop_rgb(g, 0, 0xB6/255.0, 0xB6/255.0, 0xBC/255.0);
    cairo_pattern_add_color_stop_rgb(g, 1, 0xDC/255.0, 0xDC/255.0, 0xE0/255.0);
    cairo_rectangle(cr, x + w/2 - w*0.07, y + h - 1, w*0.14, h*0.13); cairo_set_source(cr, g); cairo_fill(cr);
    cairo_pattern_destroy(g);
    rrect(cr, x + w/2 - w*0.22, y + h + h*0.12, w*0.44, MAX(4, h*0.05), MAX(2, h*0.025));
    cairo_set_source_rgb(cr, 0xC8/255.0, 0xC8/255.0, 0xCE/255.0); cairo_fill(cr);
    /* bezel + screen */
    rrect(cr, x, y, w, h, b * 1.6);
    cairo_set_source_rgb(cr, 0x1C/255.0, 0x1C/255.0, 0x1E/255.0); cairo_fill(cr);
    double sx = x + b, sy = y + b, sw = w - 2*b, sh = h - 2*b;
    cairo_save(cr);
    rrect(cr, sx, sy, sw, sh, b * 0.8); cairo_clip(cr);
    cairo_surface_t* s = thumb(file, 320, 200);
    if (s) paint_cover(cr, s, sx, sy, sw, sh);
    else { xxri_gradient_ring(cr, sx, sy, sw, sh); cairo_paint(cr); }
    /* a Settings-like window, as Windows' preview shows a dialog */
    double ww = sw * 0.46, wh = sh * 0.46, wx = sx + sw * 0.47, wy = sy + sh * 0.2;
    rrect(cr, wx, wy, ww, wh, MAX(2, sw * 0.02));
    cairo_set_source_rgba(cr, 1, 1, 1, 0.94); cairo_fill(cr);
    cairo_set_source_rgba(cr, 0x1C/255.0, 0x1C/255.0, 0x1E/255.0, 0.22);
    for (int i = 0; i < 3; i++) {
        rrect(cr, wx + ww*0.1, wy + wh*(0.22 + i*0.16), ww*(i == 2 ? 0.5 : 0.78), MAX(1.5, wh*0.06), MAX(0.75, wh*0.03));
        cairo_fill(cr);
    }
    rrect(cr, wx + ww*0.62, wy + wh*0.74, ww*0.28, MAX(3, wh*0.12), MAX(1.5, wh*0.06));
    xxri_gradient_ring(cr, wx + ww*0.62, 0, ww*0.28, 1); cairo_fill(cr);
    /* the dock */
    rrect(cr, sx + sw*0.3, sy + sh*0.88, sw*0.4, MAX(3, sh*0.07), MAX(1.5, sh*0.035));
    cairo_set_source_rgba(cr, 1, 1, 1, 0.72); cairo_fill(cr);
    cairo_restore(cr);
    if (number > 0) {
        char n[8]; snprintf(n, sizeof n, "%d", number);
        pill_text(cr, x + b + 4, y + b + 4, n, TRUE);
    }
}
static gboolean draw_wall_hero(GtkWidget* wdg, cairo_t* cr, gpointer u) {
    (void)u;
    double W = gtk_widget_get_allocated_width(wdg), H = gtk_widget_get_allocated_height(wdg);
    /* the displays as RandR places them, scaled into the band */
    double x0 = 1e9, y0 = 1e9, x1 = -1e9, y1 = -1e9;
    for (int i = 0; i < g_nmon; i++) {
        x0 = MIN(x0, g_wp_mon[i][0]); y0 = MIN(y0, g_wp_mon[i][1]);
        x1 = MAX(x1, g_wp_mon[i][0] + g_wp_mon[i][2]); y1 = MAX(y1, g_wp_mon[i][1] + g_wp_mon[i][3]);
    }
    double gap = g_nmon > 1 ? 14 : 0;
    double bw = x1 - x0, bh = y1 - y0;
    if (bw <= 0 || bh <= 0) return FALSE;
    double k = MIN((W - 8 - gap * (g_nmon - 1)) / bw, (H * 0.8) / bh);
    k = MIN(k, (g_nmon > 1 ? 150.0 : 200.0) / (bh > 0 ? bh : 1));
    double tw = bw * k + gap * (g_nmon - 1), th = bh * k;
    double ox = (W - tw) / 2, oy = 4 + (H * 0.8 - th) / 2;
    for (int i = 0; i < g_nmon; i++) {
        /* displays side by side get the gap between them */
        int before = 0;
        for (int j = 0; j < g_nmon; j++) if (g_wp_mon[j][0] < g_wp_mon[i][0]) before++;
        draw_monitor(cr, ox + (g_wp_mon[i][0] - x0) * k + gap * before, oy + (g_wp_mon[i][1] - y0) * k,
                     g_wp_mon[i][2] * k, g_wp_mon[i][3] * k, g_wp_disp[i], g_nmon > 1 ? i + 1 : 0);
    }
    return FALSE;
}

#define TILE_W 168
#define TILE_H 105
static gboolean draw_wall_tile(GtkWidget* wdg, cairo_t* cr, gpointer d) {
    WallTile* t = (WallTile*)d;
    double W = gtk_widget_get_allocated_width(wdg), H = gtk_widget_get_allocated_height(wdg);
    double p = 4, r = 12;
    /* where it is on show: the check, and with several displays their numbers */
    int on[WP_MAXMON], non = 0;
    for (int i = 0; i < g_nmon; i++) if (!g_strcmp0(g_wp_disp[i], t->file)) on[non++] = i + 1;
    if (non) {
        rrect(cr, 0.5, 0.5, W - 1, H - 1, r + 3);
        xxri_gradient_ring(cr, 0, 0, W, H); cairo_set_line_width(cr, 3); cairo_stroke(cr);
    }
    cairo_save(cr);
    rrect(cr, p, p, W - 2*p, H - 2*p, r); cairo_clip(cr);
    cairo_surface_t* s = thumb(t->file, TILE_W, TILE_H);
    if (s) paint_cover(cr, s, p, p, W - 2*p, H - 2*p);
    else { cairo_set_source_rgb(cr, 0xE2/255.0, 0xE2/255.0, 0xE6/255.0); cairo_paint(cr); }
    cairo_restore(cr);
    if (t->def) pill_text(cr, p + 7, H - p - 24, _("Default"), FALSE);
    if (non) {
        double cx = W - p - 15, cy = p + 15;
        if (g_nmon > 1) {
            char nums[32] = "";
            for (int i = 0; i < non; i++) { char b[8]; snprintf(b, sizeof b, i ? " %d" : "%d", on[i]); strncat(nums, b, sizeof nums - strlen(nums) - 1); }
            PangoLayout* l = pango_cairo_create_layout(cr);
            PangoFontDescription* fd = pango_font_description_from_string("Avenir Next Bold 8");
            pango_layout_set_font_description(l, fd); pango_font_description_free(fd);
            pango_layout_set_text(l, nums, -1);
            int lw, lh; pango_layout_get_pixel_size(l, &lw, &lh);
            double bw2 = MAX(20, lw + 12);
            rrect(cr, W - p - 7 - bw2, p + 6, bw2, 20, 10);
            xxri_gradient_ring(cr, W - p - 7 - bw2, 0, bw2, 1); cairo_fill(cr);
            cairo_set_source_rgb(cr, 1, 1, 1);
            cairo_move_to(cr, W - p - 7 - bw2 + (bw2 - lw) / 2, p + 6 + (20 - lh) / 2.0);
            pango_cairo_show_layout(cr, l); g_object_unref(l);
        } else {
            cairo_arc(cr, cx, cy, 10, 0, 2*G_PI);
            xxri_gradient_ring(cr, cx - 10, cy - 10, 20, 20); cairo_fill(cr);
            cairo_set_source_rgb(cr, 1, 1, 1); cairo_set_line_width(cr, 2);
            cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND); cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
            cairo_move_to(cr, cx - 4.5, cy + 0.5); cairo_line_to(cr, cx - 1.2, cy + 3.8); cairo_line_to(cr, cx + 4.8, cy - 3.2);
            cairo_stroke(cr);
        }
    }
    return FALSE;
}
static void wall_redraw(void) {
    for (guint i = 0; g_wtiles && i < g_wtiles->len; i++)
        gtk_widget_queue_draw(((WallTile*)g_wtiles->pdata[i])->area);
    if (g_wp_hero) gtk_widget_queue_draw(g_wp_hero);
    wall_hero_text();
}
static void wall_set_done(const char* out, gboolean ok, gpointer tag) {
    (void)out; (void)tag;
    if (ok) return;
    GtkWidget* d = gtk_message_dialog_new(GTK_WINDOW(g_win), GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
        GTK_BUTTONS_OK, _("The wallpaper was not changed"));
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(d), "%s",
        _("XXRI can use JPEG and PNG pictures as wallpapers. Choose another picture and try again."));
    gtk_dialog_run(GTK_DIALOG(d)); gtk_widget_destroy(d);
    rebuild("wallpaper");
}
/* mon: 0 = every display, else that display (1-based) */
static void wall_apply(const char* file, int mon) {
    if (mon == 0) for (int i = 0; i < g_nmon; i++) { g_free(g_wp_disp[i]); g_wp_disp[i] = g_strdup(file); }
    else if (mon <= g_nmon) { g_free(g_wp_disp[mon-1]); g_wp_disp[mon-1] = g_strdup(file); }
    wall_redraw();
    char m[16]; snprintf(m, sizeof m, "%d", mon);
    const char* argv[] = { "xxri-desktop", "wallpaper", "set", file, "--monitor", mon ? m : "all", NULL };
    run_async(argv, wall_set_done, NULL);
}
static void wall_menu_pick(GtkMenuItem* it, gpointer u) {
    WallTile* t = (WallTile*)u;
    wall_apply(t->file, GPOINTER_TO_INT(g_object_get_data(G_OBJECT(it), "mon")));
}
static gboolean wall_press(GtkWidget* w, GdkEventButton* e, gpointer u) {
    WallTile* t = (WallTile*)u;
    if (e->type != GDK_BUTTON_PRESS) return FALSE;
    if (e->button == 1) { wall_apply(t->file, 0); return TRUE; }
    if (e->button != 3) return FALSE;
    GtkWidget* m = gtk_menu_new(); css(m, "xxri-wall-menu");
    GtkWidget* all = gtk_menu_item_new_with_label(g_nmon > 1 ? _("Set for all displays") : _("Set as wallpaper"));
    g_object_set_data(G_OBJECT(all), "mon", GINT_TO_POINTER(0));
    g_signal_connect(all, "activate", G_CALLBACK(wall_menu_pick), t);
    gtk_menu_shell_append(GTK_MENU_SHELL(m), all);
    for (int i = 0; g_nmon > 1 && i < g_nmon; i++) {
        char l[160]; snprintf(l, sizeof l, _("Set for display %d (%s)"), i + 1, g_wp_lbl[i] ? g_wp_lbl[i] : _("Display"));
        GtkWidget* it = gtk_menu_item_new_with_label(l);
        g_object_set_data(G_OBJECT(it), "mon", GINT_TO_POINTER(i + 1));
        g_signal_connect(it, "activate", G_CALLBACK(wall_menu_pick), t);
        gtk_menu_shell_append(GTK_MENU_SHELL(m), it);
    }
    gtk_widget_show_all(m);
    gtk_menu_attach_to_widget(GTK_MENU(m), w, NULL);
    gtk_menu_popup_at_pointer(GTK_MENU(m), (GdkEvent*)e);
    return TRUE;
}
static gboolean wall_key(GtkWidget* w, GdkEventKey* e, gpointer u) {
    (void)w;
    if (e->keyval == GDK_KEY_Return || e->keyval == GDK_KEY_space || e->keyval == GDK_KEY_KP_Enter) {
        wall_apply(((WallTile*)u)->file, 0); return TRUE;
    }
    return FALSE;
}
static void cb_browse(GtkWidget* b, gpointer u) {
    (void)b;(void)u;
    GtkWidget* d = gtk_file_chooser_dialog_new(_("Choose a photo"), GTK_WINDOW(g_win), GTK_FILE_CHOOSER_ACTION_OPEN,
        _("Cancel"), GTK_RESPONSE_CANCEL, _("Choose picture"), GTK_RESPONSE_ACCEPT, NULL);
    css(gtk_dialog_get_widget_for_response(GTK_DIALOG(d), GTK_RESPONSE_ACCEPT), "xxri-accent");
    gtk_style_context_add_class(gtk_widget_get_style_context(d), "xxri-chooser");
    gtk_window_set_modal(GTK_WINDOW(d), TRUE);
    GtkFileFilter* f = gtk_file_filter_new();
    gtk_file_filter_set_name(f, _("Pictures (JPEG, PNG)"));
    const char* pats[] = { "*.jpg", "*.jpeg", "*.png", "*.JPG", "*.JPEG", "*.PNG", NULL };
    for (int i = 0; pats[i]; i++) gtk_file_filter_add_pattern(f, pats[i]);
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(d), f);
    char* pics = g_build_filename(g_get_home_dir(), "Pictures", NULL);
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(d), g_file_test(pics, G_FILE_TEST_IS_DIR) ? pics : g_get_home_dir());
    g_free(pics);
    /* several displays: ask which one */
    GtkWidget* where = NULL;
    if (g_nmon > 1) {
        GtkWidget* eb = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        gtk_box_pack_start(GTK_BOX(eb), label_cls(_("Use on"), "xxri-row-title", 0), FALSE, FALSE, 0);
        where = drop_new(NULL, NULL);
        drop_append(where, "0", _("All displays"));
        for (int i = 0; i < g_nmon; i++) {
            char id[8], l[160]; snprintf(id, sizeof id, "%d", i + 1);
            snprintf(l, sizeof l, _("Display %d (%s)"), i + 1, g_wp_lbl[i] ? g_wp_lbl[i] : _("Display"));
            drop_append(where, id, l);
        }
        gtk_box_pack_start(GTK_BOX(eb), where, FALSE, FALSE, 0);
        gtk_widget_show_all(eb);
        gtk_file_chooser_set_extra_widget(GTK_FILE_CHOOSER(d), eb);
    }
    /* the chooser's natural size doesn't fit 768 lines (same in xxri-media) */
    int ww, wh; gtk_window_get_size(GTK_WINDOW(g_win), &ww, &wh);
    GdkGeometry geo; geo.min_width = 380; geo.min_height = 260;
    geo.max_width = MIN(ww, 700); geo.max_height = MIN(wh, 480);
    gtk_window_set_geometry_hints(GTK_WINDOW(d), NULL, &geo, GDK_HINT_MIN_SIZE | GDK_HINT_MAX_SIZE);
    gtk_window_set_default_size(GTK_WINDOW(d), geo.max_width, geo.max_height);
    gtk_window_set_position(GTK_WINDOW(d), GTK_WIN_POS_CENTER_ON_PARENT);
    gtk_widget_show(d);           /* never show_all: the chooser hides parts on purpose */
    if (gtk_dialog_run(GTK_DIALOG(d)) == GTK_RESPONSE_ACCEPT) {
        char* fn = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(d));
        int mon = where ? atoi(drop_get_active(where)) : 0;
        gtk_widget_destroy(d);
        if (fn) wall_apply(fn, mon);
        g_free(fn);
        return;
    }
    gtk_widget_destroy(d);
}
static void cb_fit(GtkWidget* w, const char* id, gpointer u) {
    (void)w; (void)u;
    if (!id) return;
    g_free(g_wp_fit); g_wp_fit = g_strdup(id);
    wall_hero_text();
    const char* argv[] = { "xxri-desktop", "wallpaper", "fit", id, NULL };
    run_async(argv, NULL, NULL);
}

/* the look: Settings restyles itself at once, apps as they next open */
static GtkCssProvider* g_dark_css;      /* xxri-dark.css while Mode is Dark */
static void apply_self_look(void) {
    GtkSettings* gs = gtk_settings_get_default();
    gboolean dark = FALSE; g_object_get(gs, "gtk-application-prefer-dark-theme", &dark, NULL);
    GdkScreen* scr = gdk_screen_get_default();
    if (dark && !g_dark_css) {
        g_dark_css = gtk_css_provider_new();
        gtk_css_provider_load_from_path(g_dark_css, "/usr/local/share/xxri-settings/xxri-dark.css", NULL);
        gtk_style_context_add_provider_for_screen(scr, GTK_STYLE_PROVIDER(g_dark_css),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 2);
    } else if (!dark && g_dark_css) {
        gtk_style_context_remove_provider_for_screen(scr, GTK_STYLE_PROVIDER(g_dark_css));
        g_object_unref(g_dark_css); g_dark_css = NULL;
    }
    GtkStyleContext* sc = gtk_widget_get_style_context(g_win);
    if (dark) gtk_style_context_add_class(sc, "dark"); else gtk_style_context_remove_class(sc, "dark");
    if (g_wp_hero) gtk_widget_queue_draw(g_wp_hero);
}
static char* g_look_theme;
static void set_self_theme(void) {
    GtkSettings* gs = gtk_settings_get_default();
    gboolean dark = FALSE; g_object_get(gs, "gtk-application-prefer-dark-theme", &dark, NULL);
    const char* t = g_look_theme ? g_look_theme : "xxri";
    g_object_set(gs, "gtk-theme-name", !strcmp(t, "HighContrast") && dark ? "HighContrastInverse" : t, NULL);
}
static void cb_mode(GtkToggleButton* b, gpointer d) {
    if (!gtk_toggle_button_get_active(b)) return;
    gboolean dark = !strcmp((const char*)d, "dark");
    g_object_set(gtk_settings_get_default(), "gtk-application-prefer-dark-theme", dark, NULL);
    set_self_theme(); apply_self_look();
    const char* argv[] = { "xxri-desktop", "appearance", "set", (const char*)d, NULL };
    run_async(argv, NULL, NULL);
}
static void cb_theme(GtkWidget* w, const char* id, gpointer u) {
    (void)w; (void)u;
    if (!id) return;
    g_free(g_look_theme); g_look_theme = g_strdup(id);
    set_self_theme();
    const char* argv[] = { "xxri-desktop", "theme", "set", id, NULL };
    run_async(argv, NULL, NULL);
}
static void cb_cursor(GtkWidget* w, const char* id, gpointer u) {
    (void)w; (void)u;
    if (!id) return;
    g_object_set(gtk_settings_get_default(), "gtk-cursor-theme-name", id, NULL);
    const char* argv[] = { "xxri-desktop", "cursor", "set", id, NULL };
    run_async(argv, NULL, NULL);
}
/* Other apps get the scale from GDK_DPI_SCALE (profile.d), so only after a
 * desktop restart. Settings previews it by setting its own DPI, which GTK
 * multiplies by the session's GDK_DPI_SCALE - divide that back out. */
static void cb_uiscale(GtkWidget* w, const char* id, gpointer u) {
    (void)w; (void)u;
    if (!id) return;
    const char* env = g_getenv("GDK_DPI_SCALE");
    double es = env ? g_ascii_strtod(env, NULL) : 1.0;
    if (es <= 0) es = 1.0;
    g_object_set(gtk_settings_get_default(), "gtk-xft-dpi", (int)(98304.0 * atoi(id) / 100.0 / es), NULL);
    char* out = run_cmd("xxri-desktop scale set %s", id);
    g_free(out);
    int now = (int)(es * 100 + 0.5);
    if (now == atoi(id)) return;              /* back to what the session runs */
    GtkWidget* d = gtk_message_dialog_new(GTK_WINDOW(g_win), GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION,
        GTK_BUTTONS_NONE, _("Use %s%% scale everywhere?"), id);
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(d),
        _("Settings shows the new scale already. Other apps use it once the desktop restarts, "
        "which closes open apps. You can also keep working and switch the next time the desktop starts."));
    gtk_dialog_add_buttons(GTK_DIALOG(d), _("At Next Start"), RES_LATER, _("Restart Now"), RES_NOW, NULL);
    css(gtk_dialog_get_widget_for_response(GTK_DIALOG(d), RES_NOW), "xxri-accent");
    gtk_dialog_set_default_response(GTK_DIALOG(d), RES_LATER);
    int r = gtk_dialog_run(GTK_DIALOG(d));
    gtk_widget_destroy(d);
    if (r == RES_NOW) run_bg("xxri-desktop restart-session");
}
static void cb_docksize(GtkToggleButton* b, gpointer d) {
    if (!gtk_toggle_button_get_active(b)) return;
    const char* argv[] = { "xxri-desktop", "dock", "size", (const char*)d, NULL };
    run_async(argv, NULL, NULL);
}
/* segmented control (.xxri-seg). The active one is set before the handler is
 * connected, so building the page never calls a backend */
static GtkWidget* seg_new(void) { GtkWidget* s = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0); css(s, "xxri-seg"); return s; }
static GtkWidget* seg_add(GtkWidget* seg, GtkWidget* group, const char* label, gboolean active, GCallback cb, gpointer d) {
    GtkWidget* b = group ? gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(group), label)
                         : gtk_radio_button_new_with_label(NULL, label);
    gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(b), FALSE);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(b), active);
    g_signal_connect(b, "toggled", cb, d);
    gtk_box_pack_start(GTK_BOX(seg), b, FALSE, FALSE, 0);
    return b;
}
static GtkWidget* combo_from(const char* json, const char* key, const char* active, DropChanged cb) {
    GtkWidget* c = drop_new(cb, NULL);
    GPtrArray* a = jarr(json, key);
    for (guint i = 0; i < a->len; i++) {
        char* id = jget((char*)a->pdata[i], "id"); char* nm = jget((char*)a->pdata[i], "name");
        if (*id) drop_append(c, id, *nm ? nm : id);
        g_free(id); g_free(nm);
    }
    g_ptr_array_free(a, TRUE);
    if (active) drop_set_active(c, active);
    return c;
}

static void build_wallpaper(GtkWidget* box) {
    char* wl = run_cmd("xxri-desktop wallpaper list --json");
    char* st = run_cmd("xxri-desktop status --json");
    char* mj = run_cmd("xxri-display monitors --json");

    /* displays: geometry for the preview, labels for the menus */
    GPtrArray* ms = jarr(mj, "monitors");
    g_nmon = 0;
    for (guint i = 0; i < ms->len && g_nmon < WP_MAXMON; i++) {
        const char* o = (const char*)ms->pdata[i];
        char *w = jget(o,"width"), *h = jget(o,"height"), *x = jget(o,"x"), *y = jget(o,"y");
        if (atoi(w) > 0 && atoi(h) > 0) {
            g_wp_mon[g_nmon][0] = atoi(x); g_wp_mon[g_nmon][1] = atoi(y);
            g_wp_mon[g_nmon][2] = atoi(w); g_wp_mon[g_nmon][3] = atoi(h);
            g_free(g_wp_lbl[g_nmon]); g_wp_lbl[g_nmon] = jget(o, "label");
            g_nmon++;
        }
        g_free(w); g_free(h); g_free(x); g_free(y);
    }
    g_ptr_array_free(ms, TRUE);
    if (!g_nmon) { g_nmon = 1; g_wp_mon[0][0] = g_wp_mon[0][1] = 0; g_wp_mon[0][2] = 16; g_wp_mon[0][3] = 10; }
    GPtrArray* ds = jarr(wl, "displays");
    for (int i = 0; i < g_nmon; i++) {
        g_free(g_wp_disp[i]);
        g_wp_disp[i] = (guint)i < ds->len ? jget((char*)ds->pdata[i], "file") : jget(wl, "current_file");
    }
    g_ptr_array_free(ds, TRUE);
    g_free(g_wp_default); g_wp_default = jget(wl, "default_file");
    g_free(g_wp_fit); g_wp_fit = jget(wl, "fit");

    /* the official set */
    if (g_wtiles) g_ptr_array_unref(g_wtiles);
    g_wtiles = g_ptr_array_new_with_free_func(wall_tile_free);
    GPtrArray* ws = jarr(wl, "wallpapers");
    for (guint i = 0; i < ws->len; i++) {
        WallTile* t = g_new0(WallTile, 1);
        t->file = jget((char*)ws->pdata[i], "file"); t->name = jget((char*)ws->pdata[i], "name");
        t->def = jbool((char*)ws->pdata[i], "default");
        g_ptr_array_add(g_wtiles, t);
    }
    g_ptr_array_free(ws, TRUE);

    /* the desktop as it is now */
    GtkWidget* hero = card_new(); css(hero, "xxri-wall-hero");
    g_wp_hero = gtk_drawing_area_new();
    gtk_widget_set_size_request(g_wp_hero, -1, g_nmon > 1 ? 186 : 196);
    g_signal_connect(g_wp_hero, "draw", G_CALLBACK(draw_wall_hero), NULL);
    gtk_box_pack_start(GTK_BOX(hero), g_wp_hero, FALSE, FALSE, 0);
    g_wp_hero_title = label_cls("", "xxri-display-name", 0.5);
    g_wp_hero_sub = label_cls("", "xxri-row-sub", 0.5);
    gtk_box_pack_start(GTK_BOX(hero), g_wp_hero_title, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hero), g_wp_hero_sub, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), hero, FALSE, FALSE, 0);
    wall_hero_text();

    /* Personalize your background */
    GtkWidget* bc = card_new();
    GtkWidget* hr = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12); css(hr, "xxri-rowitem");
    GtkWidget* hi = img("wallpaper", 28); gtk_widget_set_valign(hi, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(hr), hi, FALSE, FALSE, 0);
    GtkWidget* htv = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2); gtk_widget_set_valign(htv, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(htv), label_cls(_("Personalize your background"), "xxri-row-title", 0), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(htv), label_cls(g_nmon > 1
        ? _("Official XXRI wallpapers. Click one to use it everywhere, or right-click it to choose a display.")
        : _("Official XXRI wallpapers, or a photo of your own."), "xxri-row-sub", 0), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hr), htv, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(bc), hr, FALSE, FALSE, 0);

    GtkWidget* strip = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 14); css(strip, "xxri-wall-strip");
    gtk_box_set_homogeneous(GTK_BOX(strip), TRUE);      /* even gaps across the card */
    for (guint i = 0; i < g_wtiles->len; i++) {
        WallTile* t = (WallTile*)g_wtiles->pdata[i];
        GtkWidget* col = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
        GtkWidget* eb = gtk_event_box_new(); css(eb, "xxri-wall-tile");
        gtk_widget_set_can_focus(eb, TRUE);
        gtk_widget_add_events(eb, GDK_BUTTON_PRESS_MASK | GDK_KEY_PRESS_MASK);
        char tip[256]; snprintf(tip, sizeof tip, "%s%s", t->name, t->def ? _(" (the default wallpaper)") : "");
        gtk_widget_set_tooltip_text(eb, tip);
        t->area = gtk_drawing_area_new();
        gtk_widget_set_size_request(t->area, TILE_W + 8, TILE_H + 8);
        g_signal_connect(t->area, "draw", G_CALLBACK(draw_wall_tile), t);
        gtk_container_add(GTK_CONTAINER(eb), t->area);
        g_signal_connect(eb, "button-press-event", G_CALLBACK(wall_press), t);
        g_signal_connect(eb, "key-press-event", G_CALLBACK(wall_key), t);
        gtk_box_pack_start(GTK_BOX(col), eb, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(col), label_cls(t->name, "xxri-wall-name", 0.5), FALSE, FALSE, 0);
        gtk_widget_set_halign(eb, GTK_ALIGN_CENTER);
        gtk_box_pack_start(GTK_BOX(strip), col, TRUE, TRUE, 0);
    }
    gtk_box_pack_start(GTK_BOX(bc), strip, FALSE, FALSE, 0);

    GtkWidget* br = gtk_button_new_with_label(_("Browse photos")); css(br, "xxri-chip");
    g_signal_connect(br, "clicked", G_CALLBACK(cb_browse), NULL);
    card_action(bc, _("Choose a photo"), _("JPEG or PNG, from your Pictures or any folder"), br);
    GtkWidget* fit = drop_new(cb_fit, NULL);
    const char* fids[] = { "fill", "fit", "stretch", "center", "tile" };
    for (int i = 0; i < 5; i++) drop_append(fit, fids[i], fit_label(fids[i]));
    drop_set_active(fit, g_wp_fit && *g_wp_fit ? g_wp_fit : "fill");
    card_action(bc, _("Choose a fit for your desktop image"), NULL, fit);
    gtk_box_pack_start(GTK_BOX(box), bc, FALSE, FALSE, 6);

    /* Appearance */
    char* th = run_cmd("xxri-desktop themes --json");
    gtk_box_pack_start(GTK_BOX(box), section(_("Appearance")), FALSE, FALSE, 0);
    GtkWidget* ac = card_new();
    char* ap = jget(st, "appearance"); gboolean dark = !strcmp(ap, "dark");
    GtkWidget* mode = seg_new();
    GtkWidget* li = seg_add(mode, NULL, _("Light"), !dark, G_CALLBACK(cb_mode), (gpointer)"light");
    seg_add(mode, li, _("Dark"), dark, G_CALLBACK(cb_mode), (gpointer)"dark");
    card_action(ac, _("Mode"), _("Settings and apps that follow the system theme"), mode);
    char* gt = jget(th, "gtk_theme");
    g_free(g_look_theme); g_look_theme = g_strdup(gt);
    GtkWidget* tc = combo_from(th, "gtk_themes", gt, cb_theme);
    card_action(ac, _("Theme"), _("Buttons, menus and dialogs"), tc);
    char* ct = jget(th, "cursor_theme");
    GtkWidget* cc = combo_from(th, "cursor_themes", ct, cb_cursor);
    card_action(ac, _("Cursor"), NULL, cc);
    GtkWidget* sc = drop_new(cb_uiscale, NULL);
    const char* scl[] = { "100", "125", "150", "175" };
    for (int i = 0; i < 4; i++) {
        char l[32]; snprintf(l, sizeof l, i ? "%s%%" : _("%s%% (recommended)"), scl[i]);
        drop_append(sc, scl[i], l);
    }
    char* us = jget(st, "ui_scale");
    drop_set_active(sc, *us ? us : "100");
    card_action(ac, _("Scale"), _("Text in apps; applies everywhere after a desktop restart"), sc);
    gtk_box_pack_start(GTK_BOX(box), ac, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), hint(_("Settings changes at once; other apps take a new look the next time they open.")), FALSE, FALSE, 8);

    /* Dock */
    gtk_box_pack_start(GTK_BOX(box), section(_("Dock")), FALSE, FALSE, 0);
    GtkWidget* dc = card_new();
    int dsz = atoi(jget(st, "dock_size"));
    GtkWidget* dseg = seg_new();
    GtkWidget* sm = seg_add(dseg, NULL, _("Small"), dsz == 32, G_CALLBACK(cb_docksize), (gpointer)"small");
    seg_add(dseg, sm, _("Medium"), dsz == 40 || !dsz, G_CALLBACK(cb_docksize), (gpointer)"medium");
    seg_add(dseg, sm, _("Large"), dsz == 48, G_CALLBACK(cb_docksize), (gpointer)"large");
    card_action(dc, _("Icon size"), NULL, dseg);
    gtk_box_pack_start(GTK_BOX(box), dc, FALSE, FALSE, 0);

    g_free(ap); g_free(gt); g_free(ct); g_free(us); g_free(th);
    g_free(wl); g_free(st); g_free(mj);
}

/* Sounds & Vibration */
static void cb_vol(GtkRange* r, gpointer d){ (void)d; run_bg("xxri-audio volume set %d",(int)gtk_range_get_value(r)); }
static void cb_mic(GtkRange* r, gpointer d){ (void)d; run_bg("xxri-audio mic set %d",(int)gtk_range_get_value(r)); }
static void cb_mute(GtkWidget* w, gpointer d){ (void)w;(void)d; run_bg("xxri-audio toggle"); rebuild("sound"); }
static void cb_test(GtkWidget* w, gpointer d){ (void)w;(void)d; run_bg("xxri-audio test"); }
static void build_sound(GtkWidget* box) {
    char* cards=run_cmd("xxri-audio list --json");
    if (jbool(cards,"alsa")==FALSE && !strstr(cards,"\"cards\"")) {
        page_empty(box, "sound", _("No sound card detected"),
                   _("Audio drivers load automatically at boot. About Device shows any firmware that is still missing."));
        g_free(cards); return;
    }
    GPtrArray* cs=jarr(cards,"cards");
    gtk_box_pack_start(GTK_BOX(box), section(_("Output Device")), FALSE, FALSE, 0);
    GtkWidget* oc=card_new();
    for(guint i=0;i<cs->len;i++){char*o=cs->pdata[i]; if(!jbool(o,"playback"))continue;char*n=jget(o,"name");char*ty=jget(o,"type");
        char sub[96];snprintf(sub,sizeof sub,_("type %s%s"),ty,jbool(o,"default")?_(" · default"):"");
        card_icon_row(oc,"sound",n,sub,NULL);g_free(n);g_free(ty);}
    gtk_box_pack_start(GTK_BOX(box),oc,FALSE,FALSE,0);
    char* vol=run_cmd("xxri-audio volume get --json");
    gtk_box_pack_start(GTK_BOX(box), section(_("Volume")), FALSE, FALSE, 0);
    GtkWidget* vc=card_new(); GtkWidget* vr=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,10); css(vr,"xxri-rowitem");
    GtkWidget* vs=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0,100,1); gtk_scale_set_draw_value(GTK_SCALE(vs),FALSE);
    gtk_range_set_value(GTK_RANGE(vs),atoi(jget(vol,"volume"))); g_signal_connect(vs,"value-changed",G_CALLBACK(cb_vol),NULL);
    gtk_box_pack_start(GTK_BOX(vr),vs,TRUE,TRUE,0);
    GtkWidget* mb=gtk_button_new_with_label(jbool(vol,"muted")?_("Unmute"):_("Mute")); css(mb,"xxri-chip"); g_signal_connect(mb,"clicked",G_CALLBACK(cb_mute),NULL);
    GtkWidget* tb=gtk_button_new_with_label(_("Test")); css(tb,"xxri-chip"); g_signal_connect(tb,"clicked",G_CALLBACK(cb_test),NULL);
    gtk_box_pack_start(GTK_BOX(vr),mb,FALSE,FALSE,0); gtk_box_pack_start(GTK_BOX(vr),tb,FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(vc),vr,FALSE,FALSE,0); gtk_box_pack_start(GTK_BOX(box),vc,FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(box), section(_("Input Device")), FALSE, FALSE, 0);
    GtkWidget* icd=card_new(); gboolean cap=FALSE;
    for(guint i=0;i<cs->len;i++){char*o=cs->pdata[i]; if(!jbool(o,"capture"))continue;cap=TRUE;char*n=jget(o,"name");
        card_icon_row(icd,"sound",n,_("microphone / capture"),NULL);g_free(n);}
    if(!cap) card_kv(icd,_("No microphone detected"),"");
    gtk_box_pack_start(GTK_BOX(box),icd,FALSE,FALSE,0);
    if(cap){ char* mic=run_cmd("xxri-audio mic --json");
        gtk_box_pack_start(GTK_BOX(box), section(_("Microphone Level")), FALSE, FALSE, 0);
        GtkWidget* mc=card_new(); GtkWidget* mr=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0); css(mr,"xxri-rowitem");
        GtkWidget* ms=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0,100,1); gtk_scale_set_draw_value(GTK_SCALE(ms),FALSE);
        gtk_range_set_value(GTK_RANGE(ms),atoi(jget(mic,"level"))); g_signal_connect(ms,"value-changed",G_CALLBACK(cb_mic),NULL);
        gtk_box_pack_start(GTK_BOX(mr),ms,TRUE,TRUE,0); gtk_box_pack_start(GTK_BOX(mc),mr,FALSE,FALSE,0);
        gtk_box_pack_start(GTK_BOX(box),mc,FALSE,FALSE,0); g_free(mic);}
    gtk_box_pack_start(GTK_BOX(box), section(_("System Sounds")), FALSE, FALSE, 0);
    GtkWidget* nc=card_new();
    GtkWidget* n1=gtk_switch_new(); gtk_switch_set_active(GTK_SWITCH(n1),TRUE); card_action(nc,_("Notification sounds"),NULL,n1);
    GtkWidget* n2=gtk_switch_new(); card_action(nc,_("Startup sound"),_("Play the XXRI chime at login"),n2);
    gtk_box_pack_start(GTK_BOX(box),nc,FALSE,FALSE,0);
    g_ptr_array_free(cs,TRUE); g_free(cards); g_free(vol);
}

/* Storage */
static void cb_mount(GtkWidget* w, gpointer d){ (void)w; run_bg("xxri-storage mount %s",(char*)d); rebuild("storage"); }
static void cb_umount(GtkWidget* w, gpointer d){ (void)w; run_bg("xxri-storage unmount %s",(char*)d); rebuild("storage"); }
static void build_storage(GtkWidget* box) {
    char* js=run_cmd("xxri-storage list --json");
    GPtrArray* disks=jarr(js,"disks");
    if (!disks->len) { page_empty(box, "storage", _("No storage devices"),
        _("Connect a USB drive or SD card and it will appear here, ready to mount.")); }
    for (guint i=0;i<disks->len;i++) {
        char* d=disks->pdata[i]; char* dev=jget(d,"device"); char* model=jget(d,"model"); gboolean usb=jbool(d,"usb");
        char sec[160]; snprintf(sec,sizeof sec,"%s  ·  %s%s", model, dev, usb?_("  ·  removable"):"");
        gtk_box_pack_start(GTK_BOX(box), section(sec), FALSE, FALSE, 0);
        GtkWidget* c=card_new();
        GPtrArray* parts=jarr(d,"partitions");
        if (!parts->len) { char whole[64]; snprintf(whole,sizeof whole,_("Whole disk · %d MB"),atoi(jget(d,"size_mb"))); card_kv(c,whole,_("no partitions / not formatted")); }
        for (guint k=0;k<parts->len;k++) {
            char* p=parts->pdata[k]; char* pdev=jget(p,"device"); char* fs=jget(p,"fs"); char* lbl=jget(p,"label");
            char* mp=jget(p,"mountpoint"); gboolean mounted=jbool(p,"mounted"); long szmb=atol(jget(p,"size_mb"));
            GtkWidget* r=gtk_box_new(GTK_ORIENTATION_VERTICAL,6); css(r,"xxri-rowitem");
            GtkWidget* top=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,10);
            GtkWidget* tv=gtk_box_new(GTK_ORIENTATION_VERTICAL,2);
            char hdr[96]; snprintf(hdr,sizeof hdr,"%s", *lbl?lbl:pdev);
            char sub[128]; snprintf(sub,sizeof sub,"%s · %s · %ld MB", pdev, fs, szmb);
            gtk_box_pack_start(GTK_BOX(tv),label_cls(hdr,"xxri-row-title",0),FALSE,FALSE,0);
            gtk_box_pack_start(GTK_BOX(tv),label_cls(sub,"xxri-row-sub",0),FALSE,FALSE,0);
            gtk_box_pack_start(GTK_BOX(top),tv,TRUE,TRUE,0);
            if (mounted) {
                long fk=atol(jget(p,"free_kb")), uk=atol(jget(p,"used_kb"));
                GtkWidget* e=gtk_button_new_with_label(usb?_("Eject"):_("Unmount")); css(e,"xxri-chip");
                g_signal_connect_data(e,"clicked",G_CALLBACK(cb_umount),g_strdup(pdev),(GClosureNotify)g_free,0);
                gtk_widget_set_valign(e,GTK_ALIGN_CENTER); gtk_box_pack_end(GTK_BOX(top),e,FALSE,FALSE,0);
                gtk_box_pack_start(GTK_BOX(r),top,FALSE,FALSE,0);
                /* usage bar */
                double total=(double)(uk+fk); double frac=total>0?(double)uk/total:0;
                GtkWidget* lv=gtk_level_bar_new_for_interval(0,1); gtk_level_bar_set_value(GTK_LEVEL_BAR(lv),frac);
                gtk_box_pack_start(GTK_BOX(r),lv,FALSE,FALSE,2);
                char use[96]; snprintf(use,sizeof use,_("%ld MB used · %ld MB free · mounted at %s"), uk/1024, fk/1024, mp);
                gtk_box_pack_start(GTK_BOX(r),label_cls(use,"xxri-row-sub",0),FALSE,FALSE,0);
            } else {
                GtkWidget* m=gtk_button_new_with_label(_("Mount")); css(m,"xxri-chip");
                g_signal_connect_data(m,"clicked",G_CALLBACK(cb_mount),g_strdup(pdev),(GClosureNotify)g_free,0);
                gtk_widget_set_valign(m,GTK_ALIGN_CENTER); gtk_box_pack_end(GTK_BOX(top),m,FALSE,FALSE,0);
                gtk_box_pack_start(GTK_BOX(r),top,FALSE,FALSE,0);
            }
            gtk_box_pack_start(GTK_BOX(c),r,FALSE,FALSE,0);
            g_free(pdev);g_free(fs);g_free(lbl);g_free(mp);
        }
        /* health */
        char* pdev0 = parts->len? jget(parts->pdata[0],"device") : g_strdup(dev);
        char* hlt = run_cmd("xxri-storage health %s --json", pdev0);
        if (jbool(hlt,"available")) { char* stt=jget(hlt,"status"); char hs[64]; snprintf(hs,sizeof hs,_("SMART health: %s"),stt); card_kv(c,_("Health"),hs); g_free(stt); }
        g_free(pdev0); g_free(hlt);
        gtk_box_pack_start(GTK_BOX(box), c, FALSE, FALSE, 0);
        g_ptr_array_free(parts,TRUE); g_free(dev);g_free(model);
    }
    g_ptr_array_free(disks,TRUE); g_free(js);
}

/* Battery */
static void cb_gov(GtkWidget* w, gpointer d){ (void)w; run_bg("xxri-power governor set %s",(char*)d); rebuild("battery"); }
static void cb_bri(GtkRange* r, gpointer d){ (void)d; run_bg("xxri-power brightness set %d",(int)gtk_range_get_value(r)); }
static void build_battery(GtkWidget* box) {
    char* js=run_cmd("xxri-power info --json");
    gtk_box_pack_start(GTK_BOX(box), section(_("Power")), FALSE, FALSE, 0);
    GtkWidget* pc=card_new();
    if (jbool(js,"present")) {
        GPtrArray* bs=jarr(js,"batteries");
        for(guint i=0;i<bs->len;i++){char*o=bs->pdata[i];char*cap=jget(o,"capacity");char*stt=jget(o,"status");
            char t[64];snprintf(t,sizeof t,_("Battery %s%%"),cap); card_kv(pc,t,stt);g_free(cap);g_free(stt);}
        g_ptr_array_free(bs,TRUE);
    } else card_kv(pc,_("No battery detected"),_("This is normal on a desktop system."));
    card_kv(pc,_("AC adapter"), jbool(js,"online")?_("online"):(jbool(js,"present")?_("offline"):_("powered (desktop)")));
    gtk_box_pack_start(GTK_BOX(box),pc,FALSE,FALSE,0);

    gtk_box_pack_start(GTK_BOX(box), section(_("Power Profile")), FALSE, FALSE, 0);
    char* gv=run_cmd("xxri-power governor list --json");
    if (jbool(gv,"available")==FALSE && !strstr(gv,"\"current\"")) gtk_box_pack_start(GTK_BOX(box), hint(_("CPU frequency scaling is not available on this processor.")), FALSE, FALSE, 0);
    else { GtkWidget* gc=card_new(); GtkWidget* r=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,8); css(r,"xxri-rowitem");
        char* cur=jget(gv,"current");
        GtkWidget* tv=gtk_box_new(GTK_ORIENTATION_VERTICAL,2);
        gtk_box_pack_start(GTK_BOX(tv),label_cls(_("CPU governor"),"xxri-row-title",0),FALSE,FALSE,0);
        gtk_box_pack_start(GTK_BOX(tv),label_cls(cur,"xxri-row-sub",0),FALSE,FALSE,0);
        gtk_box_pack_start(GTK_BOX(r),tv,TRUE,TRUE,0);
        const char* govs[]={"performance","powersave","ondemand","conservative","schedutil"};
        for(int gi=0;gi<5;gi++){ if(!strstr(gv,govs[gi]))continue;
            GtkWidget* b=gtk_button_new_with_label(govs[gi]); css(b,"xxri-chip");
            g_signal_connect(b,"clicked",G_CALLBACK(cb_gov),(gpointer)govs[gi]); gtk_box_pack_start(GTK_BOX(r),b,FALSE,FALSE,0);}
        gtk_box_pack_start(GTK_BOX(gc),r,FALSE,FALSE,0); gtk_box_pack_start(GTK_BOX(box),gc,FALSE,FALSE,0); g_free(cur);}

    gtk_box_pack_start(GTK_BOX(box), section(_("Display Brightness")), FALSE, FALSE, 0);
    char* br=run_cmd("xxri-power brightness get --json");
    if (jbool(br,"available")==FALSE) gtk_box_pack_start(GTK_BOX(box), hint(_("Brightness control is not available on this display.")), FALSE, FALSE, 0);
    else { GtkWidget* bc=card_new(); GtkWidget* r=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0); css(r,"xxri-rowitem");
        GtkWidget* s=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0,100,1); gtk_scale_set_draw_value(GTK_SCALE(s),FALSE);
        gtk_range_set_value(GTK_RANGE(s),atoi(jget(br,"percent"))); g_signal_connect(s,"value-changed",G_CALLBACK(cb_bri),NULL);
        gtk_box_pack_start(GTK_BOX(r),s,TRUE,TRUE,0); gtk_box_pack_start(GTK_BOX(bc),r,FALSE,FALSE,0);
        gtk_box_pack_start(GTK_BOX(box),bc,FALSE,FALSE,0);}

    gtk_box_pack_start(GTK_BOX(box), section(_("Suspend")), FALSE, FALSE, 0);
    char* caps=run_cmd("xxri-power capabilities --json");
    GtkWidget* sc=card_new();
    card_kv(sc,_("Suspend to RAM"), jbool(caps,"suspend")?_("supported"):_("not supported"));
    card_kv(sc,_("Hibernate"), jbool(caps,"hibernate")?_("supported"):_("not available (no swap)"));
    gtk_box_pack_start(GTK_BOX(box),sc,FALSE,FALSE,0);
    g_free(js);g_free(gv);g_free(br);g_free(caps);
}

/* Apps */
static void cb_launch(GtkWidget* w, gpointer d){ (void)w; run_bg("xxri-app launch %s",(char*)d); }
static void cb_remove(GtkWidget* w, gpointer d){ (void)w; run_bg("xxri-app remove-ui %s",(char*)d); }
static void build_apps(GtkWidget* box) {
    char* out=run_cmd("xxri-app list");
    if (!*out) { page_empty(box, "apps", _("No apps installed yet"),
        _("Install apps from the XXRI Store, or open an AppImage you downloaded and it is integrated here."));
        g_free(out); return; }
    gtk_box_pack_start(GTK_BOX(box), section(_("Installed Applications")), FALSE, FALSE, 0);
    GtkWidget* c=card_new();
    char** lines=g_strsplit(out,"\n",-1);
    for (int i=0;lines[i];i++) {
        if (!*lines[i]) continue;
        char** f=g_strsplit(lines[i],"|",-1);
        int nf=0; while(f[nf])nf++;
        if (nf<3){g_strfreev(f);continue;}
        GtkWidget* r=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,12); css(r,"xxri-rowitem");
        char ip[256]; snprintf(ip,sizeof ip,"/usr/local/share/pixmaps/xxri-app-%s.png", f[0]);
        GdkPixbuf* pb=load_png(ip,40,40);
        if(pb){ GtkWidget* ic=gtk_image_new_from_pixbuf(pb); css(ic,"xxri-appicon"); g_object_unref(pb);
            gtk_widget_set_valign(ic,GTK_ALIGN_CENTER); gtk_box_pack_start(GTK_BOX(r),ic,FALSE,FALSE,0);}
        GtkWidget* tv=gtk_box_new(GTK_ORIENTATION_VERTICAL,2); gtk_widget_set_valign(tv,GTK_ALIGN_CENTER);
        gtk_box_pack_start(GTK_BOX(tv),label_cls(f[1],"xxri-row-title",0),FALSE,FALSE,0);
        char sub[160]; snprintf(sub,sizeof sub,_("version %s%s%s"), f[2], nf>4?_(" · last launch: "):"", nf>4?f[4]:"");
        gtk_box_pack_start(GTK_BOX(tv),label_cls(sub,"xxri-row-sub",0),FALSE,FALSE,0);
        gtk_box_pack_start(GTK_BOX(r),tv,TRUE,TRUE,0);
        GtkWidget* l=accent_btn(_("Launch")); g_signal_connect_data(l,"clicked",G_CALLBACK(cb_launch),g_strdup(f[0]),(GClosureNotify)g_free,0);
        GtkWidget* rm=gtk_button_new_with_label(_("Remove")); css(rm,"xxri-chip"); g_signal_connect_data(rm,"clicked",G_CALLBACK(cb_remove),g_strdup(f[0]),(GClosureNotify)g_free,0);
        gtk_widget_set_valign(l,GTK_ALIGN_CENTER); gtk_widget_set_valign(rm,GTK_ALIGN_CENTER);
        gtk_box_pack_end(GTK_BOX(r),rm,FALSE,FALSE,0); gtk_box_pack_end(GTK_BOX(r),l,FALSE,FALSE,0);
        gtk_box_pack_start(GTK_BOX(c),r,FALSE,FALSE,0);
        g_strfreev(f);
    }
    g_strfreev(lines);
    gtk_box_pack_start(GTK_BOX(box), c, FALSE, FALSE, 0);
    g_free(out);
}

/* General Management */
static GtkWidget* g_host_entry;
static void cb_host(GtkWidget* w, gpointer d){ (void)w;(void)d; const char* h=gtk_entry_get_text(GTK_ENTRY(g_host_entry)); if(*h) run_bg("xxri-desktop hostname set %s",h); }
static void cb_auto(GtkWidget* w, GParamSpec* p, gpointer d){ (void)p;(void)d; run_bg("xxri-desktop autologin set %s", gtk_switch_get_active(GTK_SWITCH(w))?"on":"off"); }
static void cb_ntp(GtkWidget* w, gpointer d){ (void)w;(void)d; run_bg("xxri-desktop ntp-sync"); }
static void cb_reset(GtkWidget* w, gpointer d){ (void)w;(void)d; run_bg("xxri-desktop reset"); rebuild("general"); }
static void cb_restart(GtkWidget* w, gpointer d){ (void)w;(void)d;
    GtkWidget* dlg=gtk_message_dialog_new(GTK_WINDOW(g_win),GTK_DIALOG_MODAL,GTK_MESSAGE_QUESTION,GTK_BUTTONS_OK_CANCEL,_("Restart the desktop session? All open apps will close."));
    if(gtk_dialog_run(GTK_DIALOG(dlg))==GTK_RESPONSE_OK) run_bg("xxri-desktop restart-session");
    gtk_widget_destroy(dlg);
}
/* Display: xxri-display (modes) and xxri-power (backlight).
 * A resolution change needs a new desktop session - dock, Control Center and
 * launcher are laid out once at session start - so ask first, or save it for
 * the next start. Refresh rate applies live. */
typedef struct { int w, h, depth; char* mode; gboolean cur, pref, def, rec; } DMode;
static GPtrArray* g_dmodes;             /* DMode* of the newest page; owned by
                                           that page's resolution card        */
static GtkWidget* g_reslist;            /* NULL when the page has no list     */
static gboolean   g_res_all;            /* "Show all resolutions", per run    */
static char*      g_dsp_saved;          /* saved mode waiting for a restart   */
static cairo_surface_t* g_dsp_wall;

static void dmode_free(gpointer p) { DMode* m = (DMode*)p; g_free(m->mode); g_free(m); }
static gboolean rebuild_display_idle(gpointer d) { (void)d; rebuild("display"); return G_SOURCE_REMOVE; }
static void rebuild_display_later(void) { g_idle_add(rebuild_display_idle, NULL); }

/* numbers of a flat JSON array: "rates":[60.00,59.94] -> {"60.00","59.94"} */
static gchar** jnums(const char* doc, const char* key) {
    char pat[64]; snprintf(pat, sizeof pat, "\"%s\":[", key);
    const char* p = strstr(doc, pat);
    if (!p) return g_new0(gchar*, 1);
    p += strlen(pat);
    const char* e = strchr(p, ']');
    if (!e || e == p) return g_new0(gchar*, 1);
    char* inner = g_strndup(p, e - p);
    gchar** v = g_strsplit(inner, ",", -1);
    g_free(inner);
    return v;
}
static void dsp_error(const char* out) {
    char* err = jget(out, "error");
    GtkWidget* d = gtk_message_dialog_new(GTK_WINDOW(g_win), GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
        GTK_BUTTONS_OK, _("The display setting was not changed"));
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(d), "%s", *err ? err : _("xxri-display did not answer."));
    gtk_dialog_run(GTK_DIALOG(d)); gtk_widget_destroy(d); g_free(err);
}

/* the display as a picture: a laptop for a built-in panel, a monitor
 * otherwise, with the wallpaper on its screen */
static void paint_screen(cairo_t* cr, double x, double y, double w, double h) {
    cairo_save(cr);
    rrect(cr, x, y, w, h, 3); cairo_clip(cr);
    if (g_dsp_wall) {   /* cover, like the desktop's own "fill" */
        double iw = cairo_image_surface_get_width(g_dsp_wall), ih = cairo_image_surface_get_height(g_dsp_wall);
        double s = MAX(w/iw, h/ih);
        cairo_translate(cr, x + (w - iw*s)/2, y + (h - ih*s)/2);
        cairo_scale(cr, s, s);
        cairo_set_source_surface(cr, g_dsp_wall, 0, 0);
        cairo_paint(cr);
    } else {
        cairo_pattern_t* g = cairo_pattern_create_linear(x, y, x+w, y+h);
        cairo_pattern_add_color_stop_rgb(g, 0, 0x3A/255.0, 0x3B/255.0, 0x44/255.0);
        cairo_pattern_add_color_stop_rgb(g, 1, 0x24/255.0, 0x21/255.0, 0x24/255.0);
        cairo_set_source(cr, g); cairo_paint(cr); cairo_pattern_destroy(g);
    }
    cairo_restore(cr);
}
static gboolean draw_display_hero(GtkWidget* w, cairo_t* cr, gpointer builtin) {
    double W = gtk_widget_get_allocated_width(w);
    double sw = 176, sh = 110, x = (W - sw)/2, y = 4;
    /* soft contact shadow */
    cairo_save(cr);
    cairo_translate(cr, W/2, y + sh + 16); cairo_scale(cr, 1.0, 0.16);
    cairo_pattern_t* sh_ = cairo_pattern_create_radial(0, 0, 0, 0, 0, sw*0.62);
    cairo_pattern_add_color_stop_rgba(sh_, 0, 0x1C/255.0, 0x1C/255.0, 0x1E/255.0, 0.18);
    cairo_pattern_add_color_stop_rgba(sh_, 1, 0x1C/255.0, 0x1C/255.0, 0x1E/255.0, 0);
    cairo_set_source(cr, sh_); cairo_arc(cr, 0, 0, sw*0.62, 0, 2*G_PI); cairo_fill(cr);
    cairo_pattern_destroy(sh_);
    cairo_restore(cr);
    if (GPOINTER_TO_INT(builtin)) {
        rrect(cr, x, y, sw, sh, 9);
        cairo_set_source_rgb(cr, 0x1C/255.0, 0x1C/255.0, 0x1E/255.0); cairo_fill(cr);
        paint_screen(cr, x+5, y+5, sw-10, sh-13);
        double bw = sw + 36, bh = 7, bx = (W - bw)/2, by = y + sh;
        cairo_pattern_t* g = cairo_pattern_create_linear(0, by, 0, by+bh);
        cairo_pattern_add_color_stop_rgb(g, 0, 0xE6/255.0, 0xE6/255.0, 0xEA/255.0);
        cairo_pattern_add_color_stop_rgb(g, 1, 0xB6/255.0, 0xB6/255.0, 0xBC/255.0);
        rrect(cr, bx, by, bw, bh, 3.5); cairo_set_source(cr, g); cairo_fill(cr);
        cairo_pattern_destroy(g);
        rrect(cr, (W-32)/2, by, 32, 3, 1.5);
        cairo_set_source_rgb(cr, 0xA0/255.0, 0xA0/255.0, 0xA8/255.0); cairo_fill(cr);
    } else {
        rrect(cr, x, y, sw, sh, 8);
        cairo_set_source_rgb(cr, 0x1C/255.0, 0x1C/255.0, 0x1E/255.0); cairo_fill(cr);
        paint_screen(cr, x+5, y+5, sw-10, sh-10);
        cairo_pattern_t* g = cairo_pattern_create_linear(0, y+sh, 0, y+sh+14);
        cairo_pattern_add_color_stop_rgb(g, 0, 0xB6/255.0, 0xB6/255.0, 0xBC/255.0);
        cairo_pattern_add_color_stop_rgb(g, 1, 0xDC/255.0, 0xDC/255.0, 0xE0/255.0);
        cairo_rectangle(cr, (W-24)/2, y+sh, 24, 14); cairo_set_source(cr, g); cairo_fill(cr);
        cairo_pattern_destroy(g);
        rrect(cr, (W-76)/2, y+sh+14, 76, 6, 3);
        cairo_set_source_rgb(cr, 0xC8/255.0, 0xC8/255.0, 0xCE/255.0); cairo_fill(cr);
    }
    return FALSE;
}
/* the brightness slider's end caps: a small and a large sun */
static gboolean draw_sun(GtkWidget* w, cairo_t* cr, gpointer big) {
    static const double dir[8][2] = {{1,0},{.7071,.7071},{0,1},{-.7071,.7071},{-1,0},{-.7071,-.7071},{0,-1},{.7071,-.7071}};
    double W = gtk_widget_get_allocated_width(w), H = gtk_widget_get_allocated_height(w);
    double cx = W/2, cy = H/2, s = MIN(W, H)/2, b = GPOINTER_TO_INT(big);
    double r = s * (b ? 0.34 : 0.28), r1 = r + (b ? 2.6 : 2.0), r2 = s * (b ? 0.96 : 0.78);
    cairo_set_source_rgb(cr, 0x6B/255.0, 0x6B/255.0, 0x73/255.0);
    cairo_arc(cr, cx, cy, r, 0, 2*G_PI); cairo_fill(cr);
    cairo_set_line_width(cr, b ? 1.8 : 1.4); cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    for (int i = 0; i < 8; i++) {
        cairo_move_to(cr, cx + dir[i][0]*r1, cy + dir[i][1]*r1);
        cairo_line_to(cr, cx + dir[i][0]*r2, cy + dir[i][1]*r2);
    }
    cairo_stroke(cr);
    return FALSE;
}
static GtkWidget* sun(int size, gboolean big) {
    GtkWidget* d = gtk_drawing_area_new();
    gtk_widget_set_size_request(d, size, size);
    gtk_widget_set_valign(d, GTK_ALIGN_CENTER);
    g_signal_connect(d, "draw", G_CALLBACK(draw_sun), GINT_TO_POINTER(big));
    return d;
}

/* Short list: sizes with the panel's aspect ratio, no smaller than the
 * default, plus the running/default/native modes. The rest sit behind
 * "Show all resolutions". */
static gboolean same_shape(int w, int h, int nw, int nh) {
    long a = (long)w*nh, b = (long)h*nw, d = a > b ? a-b : b-a;
    return d*100 <= a;                  /* within 1%: 1366x768 is 16:9 */
}
static gint dmode_cmp(gconstpointer a, gconstpointer b) {
    const DMode* x = *(DMode* const*)a; const DMode* y = *(DMode* const*)b;
    return x->w != y->w ? y->w - x->w : y->h - x->h;   /* largest first */
}
static gboolean res_filter(GtkListBoxRow* row, gpointer u) {
    (void)u; DMode* m = (DMode*)g_object_get_data(G_OBJECT(row), "dmode");
    return g_res_all || !m || m->rec;
}
static void cb_res_all(GtkWidget* sw, GParamSpec* p, gpointer u) {
    (void)p;(void)u; g_res_all = gtk_switch_get_active(GTK_SWITCH(sw));
    if (g_reslist) gtk_list_box_invalidate_filter(GTK_LIST_BOX(g_reslist));
}
static DMode* dmode_current(void) {
    for (guint i = 0; g_dmodes && i < g_dmodes->len; i++) { DMode* m = (DMode*)g_dmodes->pdata[i]; if (m->cur) return m; }
    return NULL;
}
static void select_current_res(void) {
    for (int i = 0; ; i++) {
        GtkListBoxRow* r = gtk_list_box_get_row_at_index(GTK_LIST_BOX(g_reslist), i);
        if (!r) break;
        DMode* m = (DMode*)g_object_get_data(G_OBJECT(r), "dmode");
        if (m && m->cur) { gtk_list_box_select_row(GTK_LIST_BOX(g_reslist), r); return; }
    }
    gtk_list_box_unselect_all(GTK_LIST_BOX(g_reslist));
}
static void on_res_activated(GtkListBox* lb, GtkListBoxRow* row, gpointer u) {
    (void)lb;(void)u;
    DMode* m = (DMode*)g_object_get_data(G_OBJECT(row), "dmode");
    if (!m) return;
    if (m->cur) {
        /* back to the size that is running: drop a waiting change, if any */
        if (g_dsp_saved) {
            char* out = run_cmd("xxri-display set-resolution %s --json", m->mode);
            if (!jbool(out, "ok")) dsp_error(out);
            g_free(out); rebuild_display_later();
        }
        return;
    }
    GtkWidget* d = gtk_message_dialog_new(GTK_WINDOW(g_win), GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION,
        GTK_BUTTONS_NONE, _("Use %d \xc3\x97 %d?"), m->w, m->h);
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(d),
        _("The desktop restarts to change resolution, so open apps will close. "
        "You can also keep working and switch the next time the desktop starts."));
    gtk_dialog_add_buttons(GTK_DIALOG(d), _("Cancel"), GTK_RESPONSE_CANCEL,
        _("At Next Start"), RES_LATER, _("Restart Now"), RES_NOW, NULL);
    css(gtk_dialog_get_widget_for_response(GTK_DIALOG(d), RES_NOW), "xxri-accent");
    gtk_dialog_set_default_response(GTK_DIALOG(d), RES_NOW);
    int r = gtk_dialog_run(GTK_DIALOG(d));
    gtk_widget_destroy(d);
    if (r != RES_LATER && r != RES_NOW) { select_current_res(); return; }
    char* out = run_cmd("xxri-display set-resolution %s%s --json", m->mode, r == RES_NOW ? " --apply" : "");
    gboolean ok = jbool(out, "ok");
    if (!ok) dsp_error(out);
    g_free(out);
    /* Restart Now: the session is about to end, nothing left to show */
    if (!ok || r == RES_LATER) rebuild_display_later();
}
static void cb_res_restart(GtkWidget* w, gpointer u) {
    (void)w;(void)u;
    if (!g_dsp_saved) return;
    char* out = run_cmd("xxri-display set-resolution %s --apply --json", g_dsp_saved);
    if (!jbool(out, "ok")) { dsp_error(out); rebuild_display_later(); }
    g_free(out);
}
static void cb_res_keep(GtkWidget* w, gpointer u) {
    (void)w;(void)u; DMode* c = dmode_current();
    if (!c) return;
    char* out = run_cmd("xxri-display set-resolution %s --json", c->mode);
    if (!jbool(out, "ok")) dsp_error(out);
    g_free(out); rebuild_display_later();
}

/* coalesce a slider drag into a few backend calls */
static guint g_bri_timer; static int g_bri_val;
static gboolean bri_apply(gpointer d) { (void)d; g_bri_timer = 0; run_bg("xxri-power brightness set %d", g_bri_val); return G_SOURCE_REMOVE; }
static void cb_dsp_bri(GtkRange* r, gpointer d) {
    (void)d; g_bri_val = (int)gtk_range_get_value(r);
    if (!g_bri_timer) g_bri_timer = g_timeout_add(120, bri_apply, NULL);
}
static void cb_refresh(GtkWidget* cb, const char* rate, gpointer d) {
    (void)d; (void)cb;
    if (!rate) return;
    char* out = run_cmd("xxri-display set-refresh %s --json", rate);
    if (!jbool(out, "ok")) { dsp_error(out); rebuild_display_later(); }
    g_free(out);
}

static void build_display(GtkWidget* box) {
    char* ls = run_cmd("xxri-display list --json");
    char* srv = jget(ls, "server");
    gboolean xorg = !strcmp(srv, "xorg");
    char* cur = jget(ls, "current_mode");
    char* name = jget(ls, "name");
    char* dm = jget(ls, "default_mode");
    char* pm = jget(ls, "preferred_mode");
    char* saved = jget(ls, "saved_mode");
    char* bootm = jget(ls, "boot_mode");
    char* drv = jget(ls, "driver");
    gboolean pending = jbool(ls, "pending");
    g_free(g_dsp_saved); g_dsp_saved = pending ? g_strdup(saved) : NULL;

    /* one row per size; if a size comes at several depths keep the deepest,
       that's what set-resolution picks. The old page's array lives on with
       that page's rows, which point into it. */
    g_reslist = NULL;
    g_dmodes = g_ptr_array_new_with_free_func(dmode_free);
    int svw = 0, svh = 0;
    if (g_dsp_saved) sscanf(g_dsp_saved, "%dx%d", &svw, &svh);
    int dw = 0, dh = 0, nw = 0, nh = 0;
    sscanf(dm, "%dx%d", &dw, &dh); sscanf(pm, "%dx%d", &nw, &nh);
    DMode* curm = NULL;
    GPtrArray* ms = jarr(ls, "modes");
    for (guint i = 0; i < ms->len; i++) {
        const char* o = (const char*)ms->pdata[i];
        char *mw = jget(o,"width"), *mh = jget(o,"height"), *md = jget(o,"depth");
        int w = atoi(mw), h = atoi(mh), dep = atoi(md);
        g_free(mw); g_free(mh); g_free(md);
        if (w <= 0 || h <= 0) continue;
        DMode* m = NULL;
        for (guint k = 0; k < g_dmodes->len; k++) { DMode* x = (DMode*)g_dmodes->pdata[k]; if (x->w == w && x->h == h) m = x; }
        if (!m) { m = g_new0(DMode, 1); m->w = w; m->h = h; m->depth = -1; g_ptr_array_add(g_dmodes, m); }
        if (dep > m->depth) { m->depth = dep; g_free(m->mode); m->mode = jget(o, "mode"); }
        m->cur |= jbool(o, "current"); m->pref |= jbool(o, "preferred"); m->def |= jbool(o, "default");
    }
    g_ptr_array_free(ms, TRUE);
    g_ptr_array_sort(g_dmodes, dmode_cmp);
    int nrec = 0;
    for (guint i = 0; i < g_dmodes->len; i++) {
        DMode* m = (DMode*)g_dmodes->pdata[i];
        m->rec = m->cur || m->def || m->pref ||
                 (dw && m->w >= dw && m->h >= dh && (!nw || same_shape(m->w, m->h, nw, nh)));
        nrec += m->rec;
        if (m->cur) curm = m;
    }

    /* the display itself */
    {   /* the wallpaper the first display shows (owned by the preview cache) */
        char* wl = run_cmd("xxri-desktop wallpaper list --json");
        GPtrArray* ds = jarr(wl, "displays");
        char* wf = ds->len ? jget((char*)ds->pdata[0], "file") : jget(wl, "current_file");
        g_dsp_wall = thumb(wf, 320, 200);
        g_free(wf); g_ptr_array_free(ds, TRUE); g_free(wl);
    }
    GtkWidget* hero = card_new(); css(hero, "xxri-display-hero");
    GtkWidget* pic = gtk_drawing_area_new();
    gtk_widget_set_size_request(pic, 240, 136);
    gtk_widget_set_halign(pic, GTK_ALIGN_CENTER);
    g_signal_connect(pic, "draw", G_CALLBACK(draw_display_hero), GINT_TO_POINTER(jbool(ls, "builtin")));
    gtk_box_pack_start(GTK_BOX(hero), pic, FALSE, FALSE, 0);
    GtkWidget* nl = label_cls(*name ? name : _("Display"), "xxri-display-name", 0.5);
    gtk_box_pack_start(GTK_BOX(hero), nl, FALSE, FALSE, 0);
    if (curm) {
        char* hz = NULL;
        /* RandR's rate only; Xfbdev just reports a nominal 60 */
        GPtrArray* ms2 = jarr(ls, "modes");
        for (guint i = 0; xorg && i < ms2->len && !hz; i++)
            if (jbool((char*)ms2->pdata[i], "current")) { char* r = jget((char*)ms2->pdata[i], "refresh"); hz = hz_label(r); g_free(r); }
        g_ptr_array_free(ms2, TRUE);
        char sub[96]; snprintf(sub, sizeof sub, "%d \xc3\x97 %d%s%s", curm->w, curm->h, hz && *hz ? "  \xc2\xb7  " : "", hz ? hz : "");
        gtk_box_pack_start(GTK_BOX(hero), label_cls(sub, "xxri-row-sub", 0.5), FALSE, FALSE, 0);
        g_free(hz);
    }
    gtk_box_pack_start(GTK_BOX(box), hero, FALSE, FALSE, 0);

    /* resolutions */
    GtkWidget* rc = card_new();
    g_object_set_data_full(G_OBJECT(rc), "dmodes", g_dmodes, (GDestroyNotify)g_ptr_array_unref);
    if (!g_dmodes->len) {
        char* note = jget(ls, "note");
        card_action(rc, _("Resolution"), *note ? note : _("No display modes were reported."), NULL);
        g_free(note);
    } else {
        g_reslist = gtk_list_box_new(); css(g_reslist, "xxri-reslist");
        gtk_list_box_set_selection_mode(GTK_LIST_BOX(g_reslist), GTK_SELECTION_SINGLE);
        for (guint i = 0; i < g_dmodes->len; i++) {
            DMode* m = (DMode*)g_dmodes->pdata[i];
            GtkWidget* row = gtk_list_box_row_new();
            g_object_set_data(G_OBJECT(row), "dmode", m);
            GtkWidget* rb = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
            char t[48]; snprintf(t, sizeof t, "%d \xc3\x97 %d", m->w, m->h);
            gtk_box_pack_start(GTK_BOX(rb), label_cls(t, "xxri-res-label", 0), FALSE, FALSE, 0);
            const char* tag = m->def ? _("(Default)") : (m->pref ? _("(Native)") : NULL);
            if (tag) gtk_box_pack_start(GTK_BOX(rb), label_cls(tag, "xxri-res-tag", 0), FALSE, FALSE, 0);
            if (g_dsp_saved && m->w == svw && m->h == svh)
                gtk_box_pack_end(GTK_BOX(rb), label_cls(_("After restart"), "xxri-res-tag", 1), FALSE, FALSE, 0);
            gtk_container_add(GTK_CONTAINER(row), rb);
            gtk_list_box_insert(GTK_LIST_BOX(g_reslist), row, -1);
        }
        gtk_list_box_set_filter_func(GTK_LIST_BOX(g_reslist), res_filter, NULL, NULL);
        gtk_box_pack_start(GTK_BOX(rc), g_reslist, FALSE, FALSE, 0);
        gtk_widget_show_all(g_reslist);
        select_current_res();
        g_signal_connect(g_reslist, "row-activated", G_CALLBACK(on_res_activated), NULL);
        if (nrec < (int)g_dmodes->len) {
            GtkWidget* all = gtk_switch_new();
            gtk_switch_set_active(GTK_SWITCH(all), g_res_all);
            g_signal_connect(all, "notify::active", G_CALLBACK(cb_res_all), NULL);
            card_action(rc, _("Show all resolutions"), NULL, all);
        }
    }
    gtk_box_pack_start(GTK_BOX(box), rc, FALSE, FALSE, 6);
    if (g_dsp_saved) {
        int sw = 0, sh = 0; sscanf(g_dsp_saved, "%dx%d", &sw, &sh);
        GtkWidget* pc = card_new();
        GtkWidget* btns = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        GtkWidget* keep = gtk_button_new_with_label(_("Cancel")); css(keep, "xxri-chip");
        g_signal_connect(keep, "clicked", G_CALLBACK(cb_res_keep), NULL);
        GtkWidget* now = accent_btn(_("Restart Now"));
        g_signal_connect(now, "clicked", G_CALLBACK(cb_res_restart), NULL);
        gtk_box_pack_start(GTK_BOX(btns), keep, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(btns), now, FALSE, FALSE, 0);
        char t[96]; snprintf(t, sizeof t, _("%d \xc3\x97 %d at next start"), sw, sh);
        card_action(pc, t, _("Saved. The desktop switches when it next starts, or restart now."), btns);
        gtk_box_pack_start(GTK_BOX(box), pc, FALSE, FALSE, 0);
    }
    if (*bootm) {
        char h[200]; snprintf(h, sizeof h,
            _("This boot was started with the xres=%s boot code, which takes priority over the resolution chosen here."), bootm);
        gtk_box_pack_start(GTK_BOX(box), hint(h), FALSE, FALSE, 4);
    }

    /* brightness */
    char* br = run_cmd("xxri-power brightness get --json");
    GtkWidget* bc = card_new();
    if (jbool(br, "available")) {
        GtkWidget* r = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10); css(r, "xxri-rowitem");
        GtkWidget* bl = label_cls(_("Brightness"), "xxri-row-title", 0);
        gtk_widget_set_valign(bl, GTK_ALIGN_CENTER);
        gtk_box_pack_start(GTK_BOX(r), bl, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(r), sun(16, FALSE), FALSE, FALSE, 0);
        GtkWidget* s = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 100, 1);
        gtk_scale_set_draw_value(GTK_SCALE(s), FALSE);
        gtk_widget_set_size_request(s, 260, -1);
        char* pct = jget(br, "percent"); gtk_range_set_value(GTK_RANGE(s), atoi(pct)); g_free(pct);
        g_signal_connect(s, "value-changed", G_CALLBACK(cb_dsp_bri), NULL);
        gtk_box_pack_start(GTK_BOX(r), s, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(r), sun(22, TRUE), FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(bc), r, FALSE, FALSE, 0);
    } else {
        card_action(bc, _("Brightness"), _("This display has no software backlight control. Use the buttons on the monitor."), NULL);
    }
    gtk_box_pack_start(GTK_BOX(box), bc, FALSE, FALSE, 6);
    g_free(br);

    /* refresh rate + how the display is driven */
    GtkWidget* fc = card_new();
    if (xorg && curm) {
        gchar** rates = NULL;
        char* crate = NULL;
        GPtrArray* ms3 = jarr(ls, "modes");
        for (guint i = 0; i < ms3->len && !rates; i++)
            if (jbool((char*)ms3->pdata[i], "current")) { rates = jnums((char*)ms3->pdata[i], "rates"); crate = jget((char*)ms3->pdata[i], "refresh"); }
        g_ptr_array_free(ms3, TRUE);
        int n = rates ? (int)g_strv_length(rates) : 0;
        if (n > 1) {
            GtkWidget* cb = drop_new(cb_refresh, NULL);
            for (int i = 0; i < n; i++) { char* l = hz_label(rates[i]); drop_append(cb, rates[i], l); g_free(l); }
            drop_set_active(cb, crate);
            /* rates belong to the running size, not one waiting for a restart */
            gtk_widget_set_sensitive(cb, !g_dsp_saved);
            card_action(fc, _("Refresh rate"), g_dsp_saved ? _("Available after the desktop restarts") : NULL, cb);
        } else {
            char* l = hz_label(n ? rates[0] : crate);
            card_action(fc, _("Refresh rate"), NULL, label_cls(*l ? l : _("Unknown"), "xxri-val", 1));
            g_free(l);
        }
        g_strfreev(rates); g_free(crate);
        char d[96]; snprintf(d, sizeof d, _("%s (kernel mode setting)"), drv);
        card_action(fc, _("Graphics driver"), NULL, label_cls(d, "xxri-val", 1));
    } else {
        card_action(fc, _("Refresh rate"), NULL, label_cls(_("Set by the video BIOS"), "xxri-val", 1));
        card_action(fc, _("Graphics driver"), NULL, label_cls(_("VESA BIOS (compatibility mode)"), "xxri-val", 1));
    }
    gtk_box_pack_start(GTK_BOX(box), fc, FALSE, FALSE, 6);

    g_free(ls); g_free(srv); g_free(cur); g_free(name); g_free(dm); g_free(pm);
    g_free(saved); g_free(bootm); g_free(drv);
}
static void build_general(GtkWidget* box) {
    char* st=run_cmd("xxri-desktop status --json");
    char* dt=run_cmd("xxri-desktop datetime --json");
    char* kb=run_cmd("xxri-input status --json");
    gtk_box_pack_start(GTK_BOX(box), section(_("Language & Region")), FALSE, FALSE, 0);
    GtkWidget* lc=card_new(); card_kv(lc,_("Language"),jget(st,"language"));
    char* rg=jget(st,"region"); card_kv(lc,_("Region"),*rg?rg:_("not set")); card_kv(lc,_("Keyboard layout"),jget(kb,"keyboard_layout"));
    gtk_box_pack_start(GTK_BOX(box),lc,FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(box), section(_("Date & Time")), FALSE, FALSE, 0);
    GtkWidget* tc=card_new(); char dtl[96]; snprintf(dtl,sizeof dtl,"%s  %s",jget(dt,"date"),jget(dt,"time"));
    GtkWidget* sy=gtk_button_new_with_label(_("Sync time")); css(sy,"xxri-chip"); g_signal_connect(sy,"clicked",G_CALLBACK(cb_ntp),NULL);
    char* tz=jget(dt,"timezone"); char tzl[64]; snprintf(tzl,sizeof tzl,_("timezone %s"),tz);
    card_action(tc,dtl,tzl,sy); gtk_box_pack_start(GTK_BOX(box),tc,FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(box), section(_("Account")), FALSE, FALSE, 0);
    GtkWidget* ac=card_new();
    GtkWidget* hr=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,10); css(hr,"xxri-rowitem");
    GtkWidget* htv=gtk_box_new(GTK_ORIENTATION_VERTICAL,4);
    gtk_box_pack_start(GTK_BOX(htv),label_cls(_("Hostname"),"xxri-row-title",0),FALSE,FALSE,0);
    g_host_entry=gtk_entry_new(); gtk_entry_set_text(GTK_ENTRY(g_host_entry),jget(st,"hostname")); gtk_widget_set_size_request(g_host_entry,220,-1);
    gtk_box_pack_start(GTK_BOX(htv),g_host_entry,FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(hr),htv,TRUE,TRUE,0);
    GtkWidget* ap=gtk_button_new_with_label(_("Apply")); css(ap,"xxri-chip"); gtk_widget_set_valign(ap,GTK_ALIGN_END); g_signal_connect(ap,"clicked",G_CALLBACK(cb_host),NULL);
    gtk_box_pack_end(GTK_BOX(hr),ap,FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(ac),hr,FALSE,FALSE,0);
    GtkWidget* al=gtk_switch_new(); gtk_switch_set_active(GTK_SWITCH(al), !strcmp(jget(st,"autologin"),"on"));
    g_signal_connect(al,"notify::active",G_CALLBACK(cb_auto),NULL);
    card_action(ac,_("Log in automatically"),_("Skip the login prompt at boot"),al);
    gtk_box_pack_start(GTK_BOX(box),ac,FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(box), section(_("Startup Applications")), FALSE, FALSE, 0);
    GtkWidget* su=card_new(); card_kv(su,_("Storage automount"),_("Enabled (xxri-storage watch)"));
    card_kv(su,_("Hardware layer"),_("Enabled (xxri-hw-init)")); gtk_box_pack_start(GTK_BOX(box),su,FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(box), section(_("Desktop")), FALSE, FALSE, 0);
    GtkWidget* dc=card_new(); GtkWidget* dr=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,8); css(dr,"xxri-rowitem");
    GtkWidget* rs=gtk_button_new_with_label(_("Reset settings")); g_signal_connect(rs,"clicked",G_CALLBACK(cb_reset),NULL);
    GtkWidget* rst=accent_btn(_("Restart session")); g_signal_connect(rst,"clicked",G_CALLBACK(cb_restart),NULL);
    gtk_box_pack_start(GTK_BOX(dr),rs,FALSE,FALSE,0); gtk_box_pack_start(GTK_BOX(dr),rst,FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(dc),dr,FALSE,FALSE,0); gtk_box_pack_start(GTK_BOX(box),dc,FALSE,FALSE,0);
    g_free(st);g_free(dt);g_free(kb);
}

/* Software Update: xxri-system-update installs in the background (download,
 * SHA-256, stage, apply) and this page polls its progress. The Tiny Core
 * extension updater stays below. */
static guint g_upd_gen;                 /* page generation; late callbacks for an
                                           older page are dropped */
static GtkWidget *g_upd_title, *g_upd_sub, *g_upd_check, *g_upd_card, *g_upd_bar,
                 *g_upd_step, *g_upd_install, *g_upd_restart;
static guint g_upd_poll;
static GtkWidget* g_updlog;
static void cb_upd(GtkWidget* w, gpointer d){ (void)w; char* r=run_cmd("xxri-updates %s",(char*)d); gtk_label_set_text(GTK_LABEL(g_updlog), r); g_free(r); }

static void upd_show_check(const char* out);
static void upd_check_done(const char* out, gboolean ok, gpointer tag) {
    (void)ok;
    if (GPOINTER_TO_UINT(tag) != g_upd_gen) return;
    upd_show_check(out ? out : "");
}
static void upd_start_check(void) {
    gtk_label_set_text(GTK_LABEL(g_upd_title), _("Checking for updates\xe2\x80\xa6"));
    gtk_widget_set_sensitive(g_upd_check, FALSE);
    const char* argv[] = { "xxri-system-update", "check", "--json", NULL };
    run_async(argv, upd_check_done, GUINT_TO_POINTER(g_upd_gen));
}
static void cb_upd_check(GtkWidget* w, gpointer u) { (void)w;(void)u; upd_start_check(); }

static gboolean upd_poll(gpointer tag);
static void upd_progress_done(const char* out, gboolean ok, gpointer tag) {
    (void)ok;
    if (GPOINTER_TO_UINT(tag) != g_upd_gen || !out) return;
    char* ph = jget(out, "phase"); char* msg = jget(out, "message"); char* err = jget(out, "error");
    char* pc = jget(out, "percent"); char* rs = jget(out, "restart");
    int pct = atoi(pc);
    gboolean running = !strcmp(ph,"checking") || !strcmp(ph,"downloading") || !strcmp(ph,"verifying") ||
                       !strcmp(ph,"staging") || !strcmp(ph,"installing");
    gtk_widget_show(g_upd_bar); gtk_widget_show(g_upd_step);
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(g_upd_bar), running && strcmp(ph,"downloading") && strcmp(ph,"checking") ? 1.0 : pct / 100.0);
    if (running) {
        gtk_label_set_text(GTK_LABEL(g_upd_step), msg);
        g_upd_poll = g_timeout_add(700, upd_poll, tag);
    } else if (!strcmp(ph, "done")) {
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(g_upd_bar), 1.0);
        gtk_label_set_text(GTK_LABEL(g_upd_step), msg);
        gtk_label_set_text(GTK_LABEL(g_upd_title), _("Update installed"));
        gtk_widget_hide(g_upd_install);
        if (!strcmp(rs, "reboot") || !strcmp(rs, "session")) {
            g_object_set_data_full(G_OBJECT(g_upd_restart), "how", g_strdup(rs), g_free);
            gtk_button_set_label(GTK_BUTTON(g_upd_restart), !strcmp(rs, "reboot") ? _("Restart now") : _("Restart desktop"));
            gtk_widget_show(g_upd_restart);
        }
    } else {
        /* failed / interrupted: say why, offer it again */
        gtk_widget_hide(g_upd_bar);
        char t[512]; snprintf(t, sizeof t, "%s%s%s", !strcmp(ph,"interrupted") ? _("The installation was interrupted.") : msg,
                              *err ? " " : "", err);
        gtk_label_set_text(GTK_LABEL(g_upd_step), t);
        css(g_upd_step, "xxri-upd-error");
        gtk_widget_set_sensitive(g_upd_install, TRUE);
        gtk_button_set_label(GTK_BUTTON(g_upd_install), _("Try again"));
    }
    g_free(ph); g_free(msg); g_free(err); g_free(pc); g_free(rs);
}
static gboolean upd_poll(gpointer tag) {
    g_upd_poll = 0;
    if (GPOINTER_TO_UINT(tag) != g_upd_gen) return G_SOURCE_REMOVE;
    const char* argv[] = { "xxri-system-update", "progress", "--json", NULL };
    run_async(argv, upd_progress_done, tag);
    return G_SOURCE_REMOVE;
}
static void upd_install_done(const char* out, gboolean ok, gpointer tag) {
    (void)out; (void)ok;
    /* the job wrote its outcome to the progress file; read it once more */
    if (GPOINTER_TO_UINT(tag) == g_upd_gen) upd_poll(tag);
}
static void cb_upd_install(GtkWidget* w, gpointer u) {
    (void)u;
    gtk_widget_set_sensitive(w, FALSE);
    gtk_style_context_remove_class(gtk_widget_get_style_context(g_upd_step), "xxri-upd-error");
    gtk_label_set_text(GTK_LABEL(g_upd_step), _("Starting\xe2\x80\xa6"));
    gtk_widget_show(g_upd_step);
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(g_upd_bar), 0);
    gtk_widget_show(g_upd_bar);
    const char* argv[] = { "xxri-system-update", "install", NULL };
    run_async(argv, upd_install_done, GUINT_TO_POINTER(g_upd_gen));
    if (!g_upd_poll) g_upd_poll = g_timeout_add(700, upd_poll, GUINT_TO_POINTER(g_upd_gen));
}
static void cb_upd_restart(GtkWidget* w, gpointer u) {
    (void)u;
    const char* how = (const char*)g_object_get_data(G_OBJECT(w), "how");
    if (how && !strcmp(how, "session")) { run_bg("xxri-desktop restart-session"); return; }
    run_bg("xxri-power restart");      /* what the power menu's Restart runs */
}
static void upd_show_check(const char* out) {
    gtk_widget_set_sensitive(g_upd_check, TRUE);
    char* cur = jget(out, "current"); char* err = jget(out, "error"); char* note = jget(out, "note");
    char* checked = jget(out, "checked"); char* blocker = jget(out, "blocker");
    gboolean ok = jbool(out, "ok"), avail = jbool(out, "available"), can = jbool(out, "can_install");
    char sub[400];
    char* why = jget(out, "reason");
    if (!ok && !strcmp(why, "unpublished")) {
        /* the normal state until the project publishes its first manifest */
        gtk_label_set_text(GTK_LABEL(g_upd_title), _("No updates published yet"));
        snprintf(sub, sizeof sub, _("XXRI OS Lite %s is the newest release. Last checked %s"), cur, checked);
    } else if (!ok) {
        gtk_label_set_text(GTK_LABEL(g_upd_title), _("Couldn't check for updates"));
        snprintf(sub, sizeof sub, "%s", *err ? err : _("The update source did not answer."));
    } else if (!avail) {
        gtk_label_set_text(GTK_LABEL(g_upd_title), _("Your system is up to date"));
        snprintf(sub, sizeof sub, _("%s%sLast checked %s"), *note ? note : "", *note ? "\n" : "", checked);
    } else {
        char* ti = jget(out, "title"); char t[256];
        snprintf(t, sizeof t, _("%s is available"), *ti ? ti : _("A new XXRI OS Lite"));
        gtk_label_set_text(GTK_LABEL(g_upd_title), t);
        snprintf(sub, sizeof sub, _("Last checked %s"), checked);
        g_free(ti);
    }
    gtk_label_set_text(GTK_LABEL(g_upd_sub), sub);
    /* the release card */
    if (avail) {
        GList* kids = gtk_container_get_children(GTK_CONTAINER(g_upd_card));
        for (GList* l = kids; l; l = l->next) gtk_widget_destroy(GTK_WIDGET(l->data));
        g_list_free(kids);
        char* ti = jget(out, "title"); char* notes = jget(out, "notes"); char* rel = jget(out, "released");
        char* sz = jget(out, "size"); char* rs = jget(out, "restart");
        double mb = atof(sz) / 1048576.0;
        char meta[200];
        snprintf(meta, sizeof meta, "%s%s%s%.1f MB%s", *rel ? _("Released ") : "", rel, *rel ? "  \xc2\xb7  " : "", mb,
                 !strcmp(rs, "none") ? "" : !strcmp(rs, "session") ? _("  \xc2\xb7  restarts the desktop") : _("  \xc2\xb7  needs a restart"));
        GtkWidget* r = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6); css(r, "xxri-rowitem");
        gtk_box_pack_start(GTK_BOX(r), label_cls(*ti ? ti : "XXRI OS Lite", "xxri-row-title", 0), FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(r), label_cls(meta, "xxri-row-sub", 0), FALSE, FALSE, 0);
        if (*notes) gtk_box_pack_start(GTK_BOX(r), label_cls(notes, "xxri-val", 0), FALSE, FALSE, 4);
        g_upd_bar = gtk_progress_bar_new(); css(g_upd_bar, "xxri-upd-bar");
        gtk_widget_set_no_show_all(g_upd_bar, TRUE);
        gtk_box_pack_start(GTK_BOX(r), g_upd_bar, FALSE, FALSE, 4);
        g_upd_step = label_cls("", "xxri-row-sub", 0);
        gtk_widget_set_no_show_all(g_upd_step, TRUE);
        gtk_box_pack_start(GTK_BOX(r), g_upd_step, FALSE, FALSE, 0);
        GtkWidget* btns = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        gtk_widget_set_halign(btns, GTK_ALIGN_END);
        g_upd_install = accent_btn(_("Download and install"));
        g_signal_connect(g_upd_install, "clicked", G_CALLBACK(cb_upd_install), NULL);
        gtk_widget_set_sensitive(g_upd_install, can);
        g_upd_restart = accent_btn(_("Restart now"));
        g_signal_connect(g_upd_restart, "clicked", G_CALLBACK(cb_upd_restart), NULL);
        gtk_widget_set_no_show_all(g_upd_restart, TRUE);
        gtk_box_pack_end(GTK_BOX(btns), g_upd_restart, FALSE, FALSE, 0);
        gtk_box_pack_end(GTK_BOX(btns), g_upd_install, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(r), btns, FALSE, FALSE, 4);
        gtk_box_pack_start(GTK_BOX(g_upd_card), r, FALSE, FALSE, 0);
        /* (why it cannot install is said once, under the header) */
        /* show_all skips a widget marked no-show-all, children included */
        gtk_widget_set_no_show_all(g_upd_card, FALSE);
        gtk_widget_show_all(g_upd_card);
        gtk_widget_set_no_show_all(g_upd_card, TRUE);
        g_free(ti); g_free(notes); g_free(rel); g_free(sz); g_free(rs);
    } else if (g_upd_card) gtk_widget_hide(g_upd_card);
    g_free(cur); g_free(err); g_free(note); g_free(checked); g_free(blocker); g_free(why);
}

static void build_update(GtkWidget* box) {
    g_upd_gen++;
    if (g_upd_poll) { g_source_remove(g_upd_poll); g_upd_poll = 0; }
    char* st = run_cmd("xxri-system-update status --json");
    char* cur = jget(st, "current"); char* bl = jget(st, "blocker"); char* src = jget(st, "source");

    /* this system */
    GtkWidget* hero = card_new(); css(hero, "xxri-upd-hero");
    GtkWidget* hr = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16); css(hr, "xxri-rowitem");
    GdkPixbuf* lg = load_png("/usr/local/share/xxri-settings/icons/logo-36.png", 44, 44);
    if (lg) { GtkWidget* li = gtk_image_new_from_pixbuf(lg); g_object_unref(lg);
        gtk_widget_set_valign(li, GTK_ALIGN_START); gtk_box_pack_start(GTK_BOX(hr), li, FALSE, FALSE, 0); }
    GtkWidget* tv = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
    char ver[96]; snprintf(ver, sizeof ver, "XXRI OS Lite %s", *cur ? cur : "");
    gtk_box_pack_start(GTK_BOX(tv), label_cls(ver, "xxri-row-sub", 0), FALSE, FALSE, 0);
    g_upd_title = label_cls("", "xxri-upd-title", 0);
    gtk_box_pack_start(GTK_BOX(tv), g_upd_title, FALSE, FALSE, 0);
    g_upd_sub = label_cls("", "xxri-row-sub", 0);
    gtk_box_pack_start(GTK_BOX(tv), g_upd_sub, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hr), tv, TRUE, TRUE, 0);
    g_upd_check = gtk_button_new_with_label(_("Check for updates")); css(g_upd_check, "xxri-chip");
    gtk_widget_set_valign(g_upd_check, GTK_ALIGN_CENTER);
    g_signal_connect(g_upd_check, "clicked", G_CALLBACK(cb_upd_check), NULL);
    gtk_box_pack_end(GTK_BOX(hr), g_upd_check, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hero), hr, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), hero, FALSE, FALSE, 0);

    /* the new release, once a check finds one */
    g_upd_card = card_new();
    gtk_widget_set_no_show_all(g_upd_card, TRUE);
    gtk_box_pack_start(GTK_BOX(box), g_upd_card, FALSE, FALSE, 8);
    g_upd_bar = g_upd_step = g_upd_install = g_upd_restart = NULL;

    if (*bl) gtk_box_pack_start(GTK_BOX(box), hint(bl), FALSE, FALSE, 4);

    /* an install already running (or just finished) outlives the page */
    char* ph = jget(st, "phase");
    gboolean running = !strcmp(ph,"checking") || !strcmp(ph,"downloading") || !strcmp(ph,"verifying") ||
                       !strcmp(ph,"staging") || !strcmp(ph,"installing");
    char* lc = strstr(st, "\"last_check\":{");
    if (running && lc) {
        upd_show_check(lc);
        if (g_upd_install) { gtk_widget_set_sensitive(g_upd_install, FALSE);
            g_upd_poll = g_timeout_add(200, upd_poll, GUINT_TO_POINTER(g_upd_gen)); }
    } else upd_start_check();
    g_free(ph);

    /* history */
    char* h = run_cmd("xxri-system-update history --json");
    GPtrArray* hs = jarr(h, "history");
    gtk_box_pack_start(GTK_BOX(box), section(_("Update History")), FALSE, FALSE, 0);
    if (!hs->len) gtk_box_pack_start(GTK_BOX(box), hint(_("No system updates have been installed yet.")), FALSE, FALSE, 0);
    else {
        GtkWidget* hc = card_new();
        for (guint i = 0; i < hs->len && i < 8; i++) {
            char* ti = jget((char*)hs->pdata[i], "title"); char* dt = jget((char*)hs->pdata[i], "date");
            char s2[96]; snprintf(s2, sizeof s2, _("Installed %s"), dt);
            card_icon_row(hc, "update", ti, s2, NULL);
            g_free(ti); g_free(dt);
        }
        gtk_box_pack_start(GTK_BOX(box), hc, FALSE, FALSE, 0);
    }
    g_ptr_array_free(hs, TRUE); g_free(h);

    /* Tiny Core extension packages (the 2.x updater, unchanged) */
    char* ps = run_cmd("xxri-updates status --json");
    gtk_box_pack_start(GTK_BOX(box), section(_("Extension Packages")), FALSE, FALSE, 0);
    GtkWidget* pc = card_new();
    char pk[96]; snprintf(pk, sizeof pk, _("%s extension packages"), jget(ps, "installed"));
    GtkWidget* bb = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget* ck = gtk_button_new_with_label(_("Check")); css(ck, "xxri-chip");
    g_signal_connect(ck, "clicked", G_CALLBACK(cb_upd), (gpointer)"check");
    GtkWidget* up = gtk_button_new_with_label(_("Update")); css(up, "xxri-chip");
    g_signal_connect(up, "clicked", G_CALLBACK(cb_upd), (gpointer)"apply");
    gtk_box_pack_start(GTK_BOX(bb), ck, FALSE, FALSE, 0); gtk_box_pack_start(GTK_BOX(bb), up, FALSE, FALSE, 0);
    card_action(pc, pk, jget(ps, "mirror"), bb);
    GtkWidget* lr = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0); css(lr, "xxri-rowitem");
    g_updlog = label_cls(_("Check compares the extension packages in /tce with the Tiny Core mirror; Update downloads newer ones, which load at the next boot."), "xxri-val", 0);
    gtk_box_pack_start(GTK_BOX(lr), g_updlog, FALSE, FALSE, 0); gtk_box_pack_start(GTK_BOX(pc), lr, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), pc, FALSE, FALSE, 0);
    char srcl[400]; snprintf(srcl, sizeof srcl, _("System updates come from %s"), src);
    gtk_box_pack_start(GTK_BOX(box), label_cls(srcl, "xxri-upd-source", 0), FALSE, FALSE, 10);
    g_free(ps); g_free(st); g_free(cur); g_free(bl); g_free(src);
}

/* About Device */
static void build_about(GtkWidget* box) {
    char* s=run_cmd("xxri-hardware summary --json");
    char* b=run_cmd("xxri-hardware bios --json");
    char* disk=run_cmd("xxri-storage list --json");
    char* dsp=run_cmd("xxri-display current --json");
    char* bat=run_cmd("xxri-power battery --json");
    /* hero header: big logo + OS summary */
    GtkWidget* hero=card_new(); GtkWidget* hr=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,20); css(hr,"xxri-rowitem");
    GtkWidget* info=gtk_box_new(GTK_ORIENTATION_VERTICAL,10);
    struct { const char* k; char* v; } rows[]={
        {_("OS"), jget(s,"os")},
        {_("Version"), ({ char* v = run_cmd("sed -n 's/^IMAGE_VERSION=//p' /etc/os-release | tr -d '\"'");
                       char* o = g_strdup_printf("%s Lite", *v ? v : "2.6"); g_free(v); o; })},
        {_("Uptime"), jget(s,"uptime")},
        {_("Locale"), ({ char* lc = jget(s,"locale");
                      /* a bare "C" is the POSIX default, not a language */
                      char* out = (!strcmp(lc,"C")||!strcmp(lc,"POSIX"))
                                ? g_strdup_printf(_("%s \xc2\xb7 POSIX default"), lc)
                                : g_strdup(lc);
                      g_free(lc); out; })},
    };
    for (int i=0;i<4;i++){ GtkWidget* rr=gtk_box_new(GTK_ORIENTATION_VERTICAL,0);
        gtk_box_pack_start(GTK_BOX(rr),label_cls(rows[i].k,"xxri-val",0),FALSE,FALSE,0);
        gtk_box_pack_start(GTK_BOX(rr),label_cls(rows[i].v,"xxri-row-title",0),FALSE,FALSE,0);
        gtk_box_pack_start(GTK_BOX(info),rr,FALSE,FALSE,0); g_free(rows[i].v);}
    gtk_box_pack_start(GTK_BOX(hr),info,TRUE,TRUE,0);
    GdkPixbuf* lg=load_png("/usr/local/share/xxri-settings/icons/logo-big.png",150,155);
    if(lg){ GtkWidget* lw=gtk_image_new_from_pixbuf(lg); g_object_unref(lg); gtk_widget_set_valign(lw,GTK_ALIGN_CENTER); gtk_box_pack_end(GTK_BOX(hr),lw,FALSE,FALSE,0);}
    gtk_box_pack_start(GTK_BOX(hero),hr,FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(box),hero,FALSE,FALSE,0);
    /* Hardware */
    gtk_box_pack_start(GTK_BOX(box), section(_("Hardware")), FALSE, FALSE, 0);
    GtkWidget* hc=card_new();
    card_kv(hc,_("CPU"),jget(s,"cpu"));
    card_kv(hc,_("CPU Cores"),jget(s,"cores"));
    card_kv(hc,_("GPU"),jget(s,"gpu"));
    char mem[32]; snprintf(mem,sizeof mem,"%s MiB",jget(s,"memory_mb")); card_kv(hc,_("Memory"),mem);
    GPtrArray* disks=jarr(disk,"disks"); char dtxt[128]; g_strlcpy(dtxt,_("unknown"),sizeof dtxt);
    for(guint i=0;i<disks->len;i++){char*o=disks->pdata[i]; if(!jbool(o,"usb")){snprintf(dtxt,sizeof dtxt,"%s · %ld GiB",jget(o,"model"),atol(jget(o,"size_mb"))/1024);break;}}
    card_kv(hc,_("Storage"),dtxt);
    card_kv(hc,_("Battery"), jbool(bat,"present")?_("present"):_("N/A (Desktop)"));
    card_kv(hc,_("Display Resolution"),jget(dsp,"mode"));
    gtk_box_pack_start(GTK_BOX(box),hc,FALSE,FALSE,0);
    /* System */
    gtk_box_pack_start(GTK_BOX(box), section(_("System")), FALSE, FALSE, 0);
    GtkWidget* yc=card_new();
    card_kv(yc,_("Kernel"),jget(s,"kernel"));
    card_kv(yc,_("Architecture"),jget(s,"arch"));
    char bios[160]; snprintf(bios,sizeof bios,"%s %s",jget(b,"vendor"),jget(b,"version")); card_kv(yc,_("BIOS"),bios);
    { char* v = run_cmd("sed -n 's/^IMAGE_VERSION=//p' /etc/os-release | tr -d '\"'");
      char* bi = run_cmd("sed -n 's/^BUILD_ID=//p' /etc/os-release | tr -d '\"'");
      char bl[160]; snprintf(bl, sizeof bl, "XXRI OS Lite %s%s%s%s", *v ? v : "2.6",
                             *bi && strcmp(bi, v) ? " (build " : "", *bi && strcmp(bi, v) ? bi : "", *bi && strcmp(bi, v) ? ")" : "");
      card_kv(yc,_("Build"),bl); g_free(v); g_free(bi); }
    gtk_box_pack_start(GTK_BOX(box),yc,FALSE,FALSE,0);
    /* Legal */
    gtk_box_pack_start(GTK_BOX(box), section(_("Legal")), FALSE, FALSE, 0);
    GtkWidget* lc=card_new();
    card_kv(lc,_("License"),_("GNU General Public License by Free Software Foundation v3 or later"));
    card_kv(lc,_("Copyright"),"Copyright (C) 2025 Jantzen Maximillian kimlim");
    gtk_box_pack_start(GTK_BOX(box),lc,FALSE,FALSE,0);
    g_ptr_array_free(disks,TRUE);
    g_free(s);g_free(b);g_free(disk);g_free(dsp);g_free(bat);
}

/* ---- Wi-Fi & Network, Bluetooth ---------------------------------------------
 * After Samsung's pages, in XXRI's styling: an On/Off pill, what is connected,
 * then what is around. Every backend call is asynchronous (a Wi-Fi scan takes
 * seconds, pairing up to a minute); each page build bumps a generation and
 * answers that arrive for an older page are dropped. */

/* a colour of the stylesheet, so drawings follow the dark sheet as well */
static GdkRGBA named_color(GtkWidget* w, const char* name, const char* fallback) {
    GdkRGBA c;
    if (!gtk_style_context_lookup_color(gtk_widget_get_style_context(w), name, &c)) gdk_rgba_parse(&c, fallback);
    return c;
}
static GdkRGBA ink_of(GtkWidget* w) {
    GdkRGBA c;
    GtkStyleContext* sc = gtk_widget_get_style_context(w);
    gtk_style_context_get_color(sc, gtk_style_context_get_state(sc), &c);
    return c;
}
/* empty a container (a card being refilled) */
static void clear_box(GtkWidget* b) {
    GList* kids = gtk_container_get_children(GTK_CONTAINER(b));
    for (GList* l = kids; l; l = l->next) gtk_widget_destroy(GTK_WIDGET(l->data));
    g_list_free(kids);
}
/* show a card that was built hidden or refilled */
static void reveal(GtkWidget* w) {
    gtk_widget_set_no_show_all(w, FALSE);
    gtk_widget_show_all(w);
}
static void conceal(GtkWidget* w) { gtk_widget_set_no_show_all(w, TRUE); gtk_widget_hide(w); }

/* Wi-Fi strength: the fan of three arcs over a dot. data = bars (0..4) |
 * 0x10 secured | 0x20 "no internet" */
static gboolean draw_wifi_glyph(GtkWidget* w, cairo_t* cr, gpointer d) {
    int v = GPOINTER_TO_INT(d), bars = v & 0x0f;
    double W = gtk_widget_get_allocated_width(w), H = gtk_widget_get_allocated_height(w);
    double s = MIN(W, H), cx = W/2, cy = H/2 + s*0.30;
    GdkRGBA ink = ink_of(w);
    /* dot, then three bands; a band is lit when the signal reaches it */
    for (int i = 0; i < 4; i++) {
        double a = (i < bars || (i == 0 && bars > 0)) ? 1.0 : 0.22;
        cairo_set_source_rgba(cr, ink.red, ink.green, ink.blue, ink.alpha * a);
        if (i == 0) {
            cairo_arc(cr, cx, cy - s*0.02, s*0.075, 0, 2*G_PI);
            cairo_fill(cr);
            continue;
        }
        double r = s * (0.08 + 0.135 * i);
        cairo_set_line_width(cr, s*0.085);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        cairo_arc(cr, cx, cy, r, -G_PI*0.78, -G_PI*0.22);
        cairo_stroke(cr);
    }
    if (v & 0x10) {                        /* padlock, bottom right */
        double lx = cx + s*0.22, ly = cy - s*0.16, lw = s*0.24, lh = s*0.19;
        cairo_set_source_rgba(cr, ink.red, ink.green, ink.blue, ink.alpha);
        cairo_set_line_width(cr, s*0.055);
        cairo_arc(cr, lx + lw/2, ly, lw*0.30, G_PI, 2*G_PI);
        cairo_stroke(cr);
        rrect(cr, lx, ly, lw, lh, s*0.03);
        cairo_fill(cr);
    }
    if (v & 0x20) {                        /* "!" when the internet may be out */
        cairo_set_source_rgba(cr, 0.85, 0.30, 0.25, 1);
        cairo_arc(cr, cx + s*0.36, cy - s*0.60, s*0.12, 0, 2*G_PI); cairo_fill(cr);
        cairo_set_source_rgba(cr, 1, 1, 1, 1);
        cairo_rectangle(cr, cx + s*0.36 - s*0.018, cy - s*0.68, s*0.036, s*0.09); cairo_fill(cr);
        cairo_rectangle(cr, cx + s*0.36 - s*0.018, cy - s*0.565, s*0.036, s*0.032); cairo_fill(cr);
    }
    return FALSE;
}
static GtkWidget* wifi_glyph(int bars, gboolean secured, gboolean warn) {
    GtkWidget* d = gtk_drawing_area_new();
    gtk_widget_set_size_request(d, 26, 26);
    gtk_widget_set_valign(d, GTK_ALIGN_CENTER);
    css(d, "xxri-glyph");
    g_signal_connect(d, "draw", G_CALLBACK(draw_wifi_glyph),
                     GINT_TO_POINTER((bars & 0x0f) | (secured ? 0x10 : 0) | (warn ? 0x20 : 0)));
    return d;
}

/* Bluetooth device glyphs, by BlueZ's Icon property, on a tinted disc */
static gboolean draw_bt_glyph(GtkWidget* w, cairo_t* cr, gpointer d) {
    const char* icon = (const char*)d;
    double W = gtk_widget_get_allocated_width(w), H = gtk_widget_get_allocated_height(w);
    double s = MIN(W, H), x0 = (W - s)/2, y0 = (H - s)/2;
    GdkRGBA ac = named_color(w, "xxri_accent", "#2E7CF6");
    cairo_set_source_rgba(cr, ac.red, ac.green, ac.blue, 0.13);
    cairo_arc(cr, x0 + s/2, y0 + s/2, s/2, 0, 2*G_PI); cairo_fill(cr);
    cairo_set_source_rgba(cr, ac.red, ac.green, ac.blue, 1);
    cairo_set_line_width(cr, s*0.06);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    double cx = x0 + s/2, cy = y0 + s/2, u = s/36.0;
    if (!icon) icon = "";
    if (g_str_has_prefix(icon, "audio-head")) {          /* headphones */
        cairo_arc(cr, cx, cy + 2*u, 9*u, G_PI, 2*G_PI); cairo_stroke(cr);
        rrect(cr, cx - 11*u, cy + 1*u, 5*u, 8*u, 2*u); cairo_fill(cr);
        rrect(cr, cx + 6*u, cy + 1*u, 5*u, 8*u, 2*u); cairo_fill(cr);
    } else if (g_str_has_prefix(icon, "audio")) {        /* speaker */
        rrect(cr, cx - 7*u, cy - 10*u, 14*u, 20*u, 3*u); cairo_stroke(cr);
        cairo_arc(cr, cx, cy + 3*u, 3.5*u, 0, 2*G_PI); cairo_stroke(cr);
        cairo_arc(cr, cx, cy - 5*u, 1.4*u, 0, 2*G_PI); cairo_fill(cr);
    } else if (g_str_has_prefix(icon, "input-keyboard")) {
        rrect(cr, cx - 11*u, cy - 6*u, 22*u, 12*u, 2.5*u); cairo_stroke(cr);
        for (int r = 0; r < 2; r++) for (int k = 0; k < 4; k++) {
            cairo_rectangle(cr, cx - 7.5*u + k*4.5*u, cy - 3*u + r*3.2*u, 2*u, 1.6*u);
        }
        cairo_fill(cr);
        cairo_move_to(cr, cx - 4*u, cy + 3.4*u); cairo_line_to(cr, cx + 4*u, cy + 3.4*u); cairo_stroke(cr);
    } else if (g_str_has_prefix(icon, "input-mouse") || g_str_has_prefix(icon, "input-tablet")) {
        rrect(cr, cx - 6*u, cy - 9*u, 12*u, 18*u, 6*u); cairo_stroke(cr);
        cairo_move_to(cr, cx, cy - 9*u); cairo_line_to(cr, cx, cy - 3*u); cairo_stroke(cr);
    } else if (g_str_has_prefix(icon, "input-gaming")) {
        rrect(cr, cx - 11*u, cy - 6*u, 22*u, 12*u, 6*u); cairo_stroke(cr);
        cairo_move_to(cr, cx - 6*u, cy - 2.5*u); cairo_line_to(cr, cx - 6*u, cy + 2.5*u);
        cairo_move_to(cr, cx - 8.5*u, cy); cairo_line_to(cr, cx - 3.5*u, cy); cairo_stroke(cr);
        cairo_arc(cr, cx + 6*u, cy, 1.6*u, 0, 2*G_PI); cairo_fill(cr);
    } else if (g_str_has_prefix(icon, "phone")) {
        rrect(cr, cx - 6*u, cy - 10*u, 12*u, 20*u, 2.5*u); cairo_stroke(cr);
        cairo_arc(cr, cx, cy + 6.5*u, 1.2*u, 0, 2*G_PI); cairo_fill(cr);
    } else if (g_str_has_prefix(icon, "computer")) {
        rrect(cr, cx - 10*u, cy - 8*u, 20*u, 13*u, 2*u); cairo_stroke(cr);
        cairo_move_to(cr, cx - 5*u, cy + 9*u); cairo_line_to(cr, cx + 5*u, cy + 9*u); cairo_stroke(cr);
    } else {                                               /* the Bluetooth rune */
        cairo_move_to(cr, cx - 6*u, cy - 5*u); cairo_line_to(cr, cx + 5*u, cy + 5*u);
        cairo_line_to(cr, cx, cy + 10*u); cairo_line_to(cr, cx, cy - 10*u);
        cairo_line_to(cr, cx + 5*u, cy - 5*u); cairo_line_to(cr, cx - 6*u, cy + 5*u);
        cairo_stroke(cr);
    }
    return FALSE;
}
static GtkWidget* bt_glyph(const char* icon) {
    GtkWidget* d = gtk_drawing_area_new();
    gtk_widget_set_size_request(d, 36, 36);
    gtk_widget_set_valign(d, GTK_ALIGN_CENTER);
    char* ic = g_strdup(icon ? icon : "");
    g_object_set_data_full(G_OBJECT(d), "icon", ic, g_free);
    g_signal_connect(d, "draw", G_CALLBACK(draw_bt_glyph), ic);
    return d;
}

/* the On/Off pill heading both pages */
typedef struct { GtkWidget *box, *title, *sub, *sw; gulong hid; } Pill;
static Pill pill_new(GtkWidget* parent, GCallback on_toggle) {
    Pill p;
    p.box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    css(p.box, "xxri-pill");
    GtkWidget* tv = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
    gtk_widget_set_valign(tv, GTK_ALIGN_CENTER);
    p.title = label_cls("\xe2\x80\xa6", "xxri-pill-title", 0);
    p.sub = label_cls("", "xxri-pill-sub", 0);
    gtk_widget_set_no_show_all(p.sub, TRUE);
    gtk_box_pack_start(GTK_BOX(tv), p.title, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(tv), p.sub, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(p.box), tv, TRUE, TRUE, 0);
    p.sw = gtk_switch_new();
    gtk_widget_set_valign(p.sw, GTK_ALIGN_CENTER);
    gtk_widget_set_sensitive(p.sw, FALSE);
    gtk_box_pack_end(GTK_BOX(p.box), p.sw, FALSE, FALSE, 0);
    p.hid = g_signal_connect(p.sw, "notify::active", on_toggle, NULL);
    gtk_box_pack_start(GTK_BOX(parent), p.box, FALSE, FALSE, 6);
    return p;
}
static void pill_set(Pill* p, gboolean on, gboolean can, const char* sub) {
    g_signal_handler_block(p->sw, p->hid);
    gtk_switch_set_active(GTK_SWITCH(p->sw), on);
    g_signal_handler_unblock(p->sw, p->hid);
    gtk_widget_set_sensitive(p->sw, can);
    gtk_label_set_text(GTK_LABEL(p->title), on ? _("On") : _("Off"));
    GtkStyleContext* sc = gtk_widget_get_style_context(p->box);
    if (on) gtk_style_context_add_class(sc, "on"); else gtk_style_context_remove_class(sc, "on");
    gtk_label_set_text(GTK_LABEL(p->sub), sub ? sub : "");
    if (sub && *sub) { gtk_widget_set_no_show_all(p->sub, FALSE); gtk_widget_show(p->sub); }
    else gtk_widget_hide(p->sub);
}

/* a section heading with something on its right (a spinner, Scan) */
static GtkWidget* section_with(const char* t, GtkWidget** right_box) {
    GtkWidget* h = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget* l = section(t);
    gtk_box_pack_start(GTK_BOX(h), l, TRUE, TRUE, 0);
    *right_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_valign(*right_box, GTK_ALIGN_END);
    gtk_widget_set_margin_bottom(*right_box, 2);
    gtk_box_pack_end(GTK_BOX(h), *right_box, FALSE, FALSE, 0);
    return h;
}
/* a list of tappable rows inside a card */
static GtkWidget* card_list(GtkWidget* card) {
    GtkWidget* lb = gtk_list_box_new();
    css(lb, "xxri-tapline");
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(lb), GTK_SELECTION_NONE);
    gtk_box_pack_start(GTK_BOX(card), lb, FALSE, FALSE, 0);
    return lb;
}
/* glyph | title + sub | right widget, as a tappable list row */
static GtkWidget* tap_row(GtkWidget* list, GtkWidget* glyph, const char* title, GtkWidget* badge,
                          const char* sub, GtkWidget* right, GtkWidget** sub_out) {
    GtkWidget* row = gtk_list_box_row_new();
    GtkWidget* r = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 14);
    css(r, "xxri-taprow");
    if (glyph) gtk_box_pack_start(GTK_BOX(r), glyph, FALSE, FALSE, 0);
    GtkWidget* tv = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_valign(tv, GTK_ALIGN_CENTER);
    GtkWidget* th = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget* tl = label_cls(title, "xxri-row-title", 0);
    gtk_label_set_line_wrap(GTK_LABEL(tl), FALSE);
    gtk_label_set_ellipsize(GTK_LABEL(tl), PANGO_ELLIPSIZE_END);
    gtk_box_pack_start(GTK_BOX(th), tl, FALSE, FALSE, 0);
    if (badge) { gtk_widget_set_valign(badge, GTK_ALIGN_CENTER); gtk_box_pack_start(GTK_BOX(th), badge, FALSE, FALSE, 0); }
    gtk_box_pack_start(GTK_BOX(tv), th, FALSE, FALSE, 0);
    GtkWidget* sl = label_cls(sub ? sub : "", "xxri-row-sub", 0);
    gtk_box_pack_start(GTK_BOX(tv), sl, FALSE, FALSE, 0);
    if (!sub || !*sub) gtk_widget_set_no_show_all(sl, TRUE);
    if (sub_out) *sub_out = sl;
    gtk_box_pack_start(GTK_BOX(r), tv, TRUE, TRUE, 0);
    if (right) { gtk_widget_set_valign(right, GTK_ALIGN_CENTER); gtk_box_pack_end(GTK_BOX(r), right, FALSE, FALSE, 0); }
    gtk_container_add(GTK_CONTAINER(row), r);
    gtk_list_box_insert(GTK_LIST_BOX(list), row, -1);
    return row;
}
static void row_set_sub(GtkWidget* sub, const char* text) {
    gtk_label_set_text(GTK_LABEL(sub), text);
    gtk_widget_set_no_show_all(sub, FALSE);
    gtk_widget_show(sub);
}
/* a gear, drawn: the font may not have U+2699 */
static gboolean draw_gear(GtkWidget* w, cairo_t* cr, gpointer d) {
    (void)d;
    double W = gtk_widget_get_allocated_width(w), H = gtk_widget_get_allocated_height(w);
    double s = MIN(W, H), cx = W/2, cy = H/2;
    GdkRGBA c = ink_of(w);
    cairo_set_source_rgba(cr, c.red, c.green, c.blue, c.alpha);
    cairo_set_line_width(cr, s*0.17);
    cairo_arc(cr, cx, cy, s*0.27, 0, 2*G_PI);
    cairo_stroke(cr);
    for (int i = 0; i < 8; i++) {
        cairo_save(cr);
        cairo_translate(cr, cx, cy);
        cairo_rotate(cr, i * G_PI / 4);
        cairo_rectangle(cr, -s*0.08, -s*0.48, s*0.16, s*0.16);
        cairo_fill(cr);
        cairo_restore(cr);
    }
    return FALSE;
}
static GtkWidget* gear_btn(void) {
    GtkWidget* b = gtk_button_new();
    GtkWidget* g = gtk_drawing_area_new();
    gtk_widget_set_size_request(g, 16, 16);
    css(g, "xxri-gear-glyph");
    g_signal_connect(g, "draw", G_CALLBACK(draw_gear), NULL);
    gtk_container_add(GTK_CONTAINER(b), g);
    css(b, "xxri-gear");
    gtk_widget_set_tooltip_text(b, _("Details"));
    return b;
}
/* a dialog's own heading: named so xxri-dialog.h does not add the title too */
static GtkWidget* dlg_heading(const char* t) {
    GtkWidget* h = label_cls(t, "xxri-dlg-heading", 0);
    gtk_widget_set_name(h, "xxri-dlg-title");
    return h;
}
static GtkWidget* badge_new(const char* t) { GtkWidget* l = gtk_label_new(t); css(l, "xxri-badge"); return l; }

/* a small alert: title + explanation, one button */
static void alert(const char* title, const char* text) {
    GtkWidget* d = gtk_message_dialog_new(GTK_WINDOW(g_win), GTK_DIALOG_MODAL, GTK_MESSAGE_INFO,
                                          GTK_BUTTONS_CLOSE, "%s", title);
    if (text && *text) gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(d), "%s", text);
    gtk_dialog_run(GTK_DIALOG(d));
    gtk_widget_destroy(d);
}
/* a sentence from a backend reason: capital first letter, a full stop */
static char* sentence(const char* r) {
    if (!r || !*r) return g_strdup("");
    char* s = g_strdup(r);
    if (g_ascii_islower(s[0])) s[0] = g_ascii_toupper(s[0]);
    gsize n = strlen(s);
    if (!strchr(".!?", s[n-1])) { char* t = g_strconcat(s, ".", NULL); g_free(s); s = t; }
    return s;
}

/* ---- Wi-Fi ---- */
static guint g_net_gen;
static guint g_net_timer;
static Pill g_net_pill;
static GtkWidget *g_net_msg, *g_net_cur_head, *g_net_cur_card, *g_net_av_head, *g_net_av_card,
                 *g_net_av_right, *g_net_spin, *g_net_eth_head, *g_net_eth_card;
static char* g_net_info;                 /* the last `info --json` */
static gboolean g_net_on, g_net_scanning, g_net_busy;

static void net_refresh(void);
static void net_scan(void);
#define NET_TAG GUINT_TO_POINTER(g_net_gen)
#define NET_STALE(tag) (GPOINTER_TO_UINT(tag) != g_net_gen)

typedef struct { char *ssid, *security; gboolean saved, current, hidden; GtkWidget* sub; } NetRow;
static void netrow_free(gpointer p) { NetRow* n = (NetRow*)p; g_free(n->ssid); g_free(n->security); g_free(n); }

static void net_connect(const char* ssid, const char* psk, gboolean hidden, GtkWidget* sub);

/* the password dialog; error = why the last try failed, or NULL */
static void net_ask_password(const char* ssid, const char* security, const char* error, GtkWidget* sub) {
    GtkWidget* d = gtk_dialog_new_with_buttons(_("Wi-Fi"), GTK_WINDOW(g_win), GTK_DIALOG_MODAL,
                                               _("Cancel"), GTK_RESPONSE_CANCEL, _("Connect"), GTK_RESPONSE_OK, NULL);
    GtkWidget* ok = gtk_dialog_get_widget_for_response(GTK_DIALOG(d), GTK_RESPONSE_OK);
    css(ok, "xxri-accent");
    gtk_dialog_set_default_response(GTK_DIALOG(d), GTK_RESPONSE_OK);
    GtkWidget* c = gtk_dialog_get_content_area(GTK_DIALOG(d));
    gtk_box_set_spacing(GTK_BOX(c), 8);
    gtk_widget_set_size_request(c, 360, -1);
    char t[300]; snprintf(t, sizeof t, _("Connect to \xe2\x80\x9c%s\xe2\x80\x9d"), ssid);
    gtk_box_pack_start(GTK_BOX(c), dlg_heading(t), FALSE, FALSE, 0);
    if (error && *error) {
        GtkWidget* e = label_cls(error, "xxri-dlg-error", 0);
        gtk_widget_set_margin_start(e, 10);
        gtk_box_pack_start(GTK_BOX(c), e, FALSE, FALSE, 0);
    }
    GtkWidget* en = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(en), FALSE);
    gtk_entry_set_input_purpose(GTK_ENTRY(en), GTK_INPUT_PURPOSE_PASSWORD);
    gtk_entry_set_placeholder_text(GTK_ENTRY(en), _("Password"));
    gtk_entry_set_activates_default(GTK_ENTRY(en), TRUE);
    gtk_widget_set_margin_start(en, 10); gtk_widget_set_margin_end(en, 10);
    gtk_box_pack_start(GTK_BOX(c), en, FALSE, FALSE, 0);
    GtkWidget* show = gtk_check_button_new_with_label(_("Show password"));
    gtk_widget_set_margin_start(show, 10);
    g_signal_connect(show, "toggled", G_CALLBACK(+[](GtkToggleButton* b, gpointer e) {
        gtk_entry_set_visibility(GTK_ENTRY(e), gtk_toggle_button_get_active(b)); }), en);
    gtk_box_pack_start(GTK_BOX(c), show, FALSE, FALSE, 0);
    /* WPA wants 8..63 characters (or 64 hex digits); WEP 5 or 13 */
    gboolean wep = security && !strcmp(security, "wep");
    g_object_set_data(G_OBJECT(en), "min", GINT_TO_POINTER(wep ? 5 : 8));
    gtk_widget_set_sensitive(ok, FALSE);
    g_signal_connect(en, "changed", G_CALLBACK(+[](GtkEditable* e, gpointer okb) {
        int n = g_utf8_strlen(gtk_entry_get_text(GTK_ENTRY(e)), -1);
        int min = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(e), "min"));
        gtk_widget_set_sensitive(GTK_WIDGET(okb), n >= min && n <= 64); }), ok);
    gtk_widget_show_all(d);
    int r = gtk_dialog_run(GTK_DIALOG(d));
    char* psk = g_strdup(gtk_entry_get_text(GTK_ENTRY(en)));
    gtk_widget_destroy(d);
    if (r == GTK_RESPONSE_OK && *psk) net_connect(ssid, psk, FALSE, sub);
    g_free(psk);
}

typedef struct { guint gen; char *ssid, *security; GtkWidget* sub; } ConnTag;
static void net_connect_done(const char* out, gboolean ok, gpointer u) {
    ConnTag* t = (ConnTag*)u;
    g_net_busy = FALSE;
    if (t->gen == g_net_gen) {
        gboolean conn = out && jbool(out, "connected");
        char* err = jget(out ? out : "", "error");
        char* why = jget(out ? out : "", "reason");
        if (!conn && !strcmp(err, "auth")) {
            net_refresh();
            net_ask_password(t->ssid, t->security, _("The password was not accepted. Try again."), NULL);
        } else {
            if (!conn) {
                char* s = sentence(*why ? why : _("the network did not answer"));
                char ti[300]; snprintf(ti, sizeof ti, _("Couldn't connect to \xe2\x80\x9c%s\xe2\x80\x9d"), t->ssid);
                alert(ti, s); g_free(s);
            }
            net_refresh();
        }
        g_free(err); g_free(why);
    }
    g_free(t->ssid); g_free(t->security); g_free(t);
}
static void net_connect(const char* ssid, const char* psk, gboolean hidden, GtkWidget* sub) {
    if (g_net_busy) return;
    g_net_busy = TRUE;
    if (sub) row_set_sub(sub, _("Connecting\xe2\x80\xa6"));
    ConnTag* t = g_new0(ConnTag, 1);
    t->gen = g_net_gen; t->ssid = g_strdup(ssid); t->security = g_strdup(psk ? "wpa" : "");
    const char* argv[8]; int n = 0;
    argv[n++] = "xxri-network"; argv[n++] = "connect";
    if (hidden) argv[n++] = "--hidden";
    argv[n++] = ssid;
    if (psk && *psk) argv[n++] = psk;
    argv[n++] = "--json"; argv[n] = NULL;
    run_async(argv, net_connect_done, t);
}

static void net_forget_done(const char* out, gboolean ok, gpointer tag) {
    (void)out; (void)ok;
    if (!NET_STALE(tag)) net_refresh();
}
/* details of the current network, with Forget */
static void net_details(void) {
    const char* o = g_net_info ? g_net_info : "";
    char* ssid = jget(o, "ssid");
    GtkWidget* d = gtk_dialog_new_with_buttons(_("Wi-Fi"), GTK_WINDOW(g_win), GTK_DIALOG_MODAL,
                                               _("Forget"), 1, _("Close"), GTK_RESPONSE_CLOSE, NULL);
    css(gtk_dialog_get_widget_for_response(GTK_DIALOG(d), 1), "destructive-action");
    gtk_dialog_set_default_response(GTK_DIALOG(d), GTK_RESPONSE_CLOSE);
    GtkWidget* c = gtk_dialog_get_content_area(GTK_DIALOG(d));
    gtk_widget_set_size_request(c, 380, -1);
    gtk_box_pack_start(GTK_BOX(c), dlg_heading(ssid), FALSE, FALSE, 0);
    GtkWidget* g = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(g), 7); gtk_grid_set_column_spacing(GTK_GRID(g), 24);
    gtk_widget_set_margin_start(g, 10); gtk_widget_set_margin_end(g, 10);
    gtk_widget_set_margin_top(g, 4); gtk_widget_set_margin_bottom(g, 8);
    int row = 0;
    char* sig = jget(o, "signal"); char* sp = jget(o, "link_mbps"); char* fr = jget(o, "freq");
    char* band = jget(o, "band"); char* sec = jget(o, "security"); char* ip = jget(o, "ip");
    char* nm = jget(o, "netmask"); char* gw = jget(o, "gateway"); char* dns = jget(o, "dns");
    char* mac = jget(o, "mac");
    const char* secname = !strcmp(sec,"wpa3") ? "WPA3" : !strcmp(sec,"wpa") ? "WPA/WPA2" :
                          !strcmp(sec,"wep") ? "WEP" : !strcmp(sec,"eap") ? _("WPA Enterprise") : _("None (open)");
    char b1[64] = "", b2[64] = "", b3[64] = "";
    if (*sig && strcmp(sig,"null")) snprintf(b1, sizeof b1, "%s dBm", sig);
    if (*sp && strcmp(sp,"null")) snprintf(b2, sizeof b2, "%s Mbit/s", sp);
    if (*fr && strcmp(fr,"null")) snprintf(b3, sizeof b3, "%s GHz (%s MHz)", band, fr);
    const char* kv[][2] = {
        {_("Status"), jbool(o,"online") ? _("Connected") : _("Connected, internet may not be available")},
        {_("Signal strength"), b1}, {_("Link speed"), b2}, {_("Frequency"), b3}, {_("Security"), secname},
        {_("IP address"), ip}, {_("Subnet mask"), nm}, {_("Gateway"), gw}, {_("DNS"), dns}, {_("MAC address"), mac} };
    for (guint i = 0; i < G_N_ELEMENTS(kv); i++) {
        if (!kv[i][1] || !*kv[i][1]) continue;
        GtkWidget* k = label_cls(kv[i][0], "xxri-dlg-key", 0);
        GtkWidget* v = label_cls(kv[i][1], "xxri-dlg-val", 0);
        gtk_label_set_selectable(GTK_LABEL(v), TRUE);
        gtk_grid_attach(GTK_GRID(g), k, 0, row, 1, 1);
        gtk_grid_attach(GTK_GRID(g), v, 1, row, 1, 1);
        row++;
    }
    gtk_box_pack_start(GTK_BOX(c), g, FALSE, FALSE, 0);
    gtk_widget_show_all(d);
    int r = gtk_dialog_run(GTK_DIALOG(d));
    gtk_widget_destroy(d);
    if (r == 1 && *ssid) {
        const char* argv[] = { "xxri-network", "forget", ssid, NULL };
        run_async(argv, net_forget_done, NET_TAG);
    }
    g_free(ssid); g_free(sig); g_free(sp); g_free(fr); g_free(band); g_free(sec); g_free(ip);
    g_free(nm); g_free(gw); g_free(dns); g_free(mac);
}

/* Add network: a name that is not broadcast, or one typed in by hand */
static void net_add(GtkWidget* w, gpointer u) {
    (void)w; (void)u;
    GtkWidget* d = gtk_dialog_new_with_buttons(_("Wi-Fi"), GTK_WINDOW(g_win), GTK_DIALOG_MODAL,
                                               _("Cancel"), GTK_RESPONSE_CANCEL, _("Connect"), GTK_RESPONSE_OK, NULL);
    GtkWidget* ok = gtk_dialog_get_widget_for_response(GTK_DIALOG(d), GTK_RESPONSE_OK);
    css(ok, "xxri-accent");
    gtk_dialog_set_default_response(GTK_DIALOG(d), GTK_RESPONSE_OK);
    GtkWidget* c = gtk_dialog_get_content_area(GTK_DIALOG(d));
    gtk_box_set_spacing(GTK_BOX(c), 8);
    gtk_widget_set_size_request(c, 360, -1);
    gtk_box_pack_start(GTK_BOX(c), dlg_heading(_("Add network")), FALSE, FALSE, 0);
    GtkWidget* name = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(name), _("Network name (SSID)"));
    GtkWidget* pw = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(pw), FALSE);
    gtk_entry_set_placeholder_text(GTK_ENTRY(pw), _("Password (leave empty for an open network)"));
    gtk_entry_set_activates_default(GTK_ENTRY(pw), TRUE);
    GtkWidget* hid = gtk_check_button_new_with_label(_("Hidden network"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(hid), TRUE);
    GtkWidget* ws[] = { name, pw, hid };
    for (int i = 0; i < 3; i++) {
        gtk_widget_set_margin_start(ws[i], 10); gtk_widget_set_margin_end(ws[i], 10);
        gtk_box_pack_start(GTK_BOX(c), ws[i], FALSE, FALSE, 0);
    }
    gtk_widget_set_sensitive(ok, FALSE);
    g_object_set_data(G_OBJECT(name), "pw", pw);
    g_object_set_data(G_OBJECT(pw), "name", name);
    GCallback check = G_CALLBACK(+[](GtkEditable* e, gpointer okb) {
        GtkWidget* nm = (GtkWidget*)g_object_get_data(G_OBJECT(e), "name");
        GtkWidget* p = (GtkWidget*)g_object_get_data(G_OBJECT(e), "pw");
        if (!nm) nm = GTK_WIDGET(e);
        if (!p) p = GTK_WIDGET(e);
        int n = strlen(gtk_entry_get_text(GTK_ENTRY(nm)));
        int k = g_utf8_strlen(gtk_entry_get_text(GTK_ENTRY(p)), -1);
        gtk_widget_set_sensitive(GTK_WIDGET(okb), n >= 1 && n <= 32 && (k == 0 || (k >= 8 && k <= 64))); });
    g_signal_connect(name, "changed", check, ok);
    g_signal_connect(pw, "changed", check, ok);
    gtk_widget_show_all(d);
    int r = gtk_dialog_run(GTK_DIALOG(d));
    char* ss = g_strdup(gtk_entry_get_text(GTK_ENTRY(name)));
    char* pk = g_strdup(gtk_entry_get_text(GTK_ENTRY(pw)));
    gboolean h = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(hid));
    gtk_widget_destroy(d);
    if (r == GTK_RESPONSE_OK && *ss) net_connect(ss, pk, h, NULL);
    g_free(ss); g_free(pk);
}

static void net_row_activated(GtkListBox* lb, GtkListBoxRow* row, gpointer u) {
    (void)lb; (void)u;
    NetRow* n = (NetRow*)g_object_get_data(G_OBJECT(row), "net");
    if (!n || g_net_busy) return;
    if (n->current) { net_details(); return; }
    if (!strcmp(n->security, "eap")) {
        alert(_("Enterprise network"), _("Networks that sign in with a user name and certificate (WPA Enterprise, 802.1X) are not supported yet."));
        return;
    }
    if (n->saved || !strcmp(n->security, "open")) net_connect(n->ssid, NULL, FALSE, n->sub);
    else net_ask_password(n->ssid, n->security, NULL, n->sub);
}

static void net_info_done(const char* out, gboolean ok, gpointer tag) {
    (void)ok;
    if (NET_STALE(tag)) return;
    g_free(g_net_info); g_net_info = g_strdup(out ? out : "");
    clear_box(g_net_cur_card);
    if (!g_net_on || !jbool(g_net_info, "connected")) { conceal(g_net_cur_head); conceal(g_net_cur_card); return; }
    char* ssid = jget(g_net_info, "ssid"); char* sec = jget(g_net_info, "security");
    char* bars = jget(g_net_info, "bars"); char* band = jget(g_net_info, "band");
    gboolean online = jbool(g_net_info, "online");
    GtkWidget* lb = card_list(g_net_cur_card);
    GtkWidget* gear = gear_btn();
    g_signal_connect(gear, "clicked", G_CALLBACK(+[](GtkWidget* w, gpointer p){ (void)w;(void)p; net_details(); }), NULL);
    GtkWidget* row = tap_row(lb, wifi_glyph(atoi(bars), strcmp(sec, "open") != 0, !online), ssid,
                             !strcmp(band, "5") ? badge_new("5G") : NULL,
                             online ? _("Connected") : _("Connected, internet may not be available"), gear, NULL);
    NetRow* n = g_new0(NetRow, 1); n->ssid = g_strdup(ssid); n->security = g_strdup(sec); n->current = TRUE; n->saved = TRUE;
    g_object_set_data_full(G_OBJECT(row), "net", n, netrow_free);
    g_signal_connect(lb, "row-activated", G_CALLBACK(net_row_activated), NULL);
    reveal(g_net_cur_head); reveal(g_net_cur_card);
    g_free(ssid); g_free(sec); g_free(bars); g_free(band);
}

static gboolean net_rescan_tick(gpointer tag) {
    g_net_timer = 0;
    /* pages stay in the stack when another one is shown: scan only while
       this one is on screen (opening it again builds it anew) */
    const char* vis = gtk_stack_get_visible_child_name(GTK_STACK(g_stack));
    if (vis && strcmp(vis, "wifi")) return G_SOURCE_REMOVE;
    if (!NET_STALE(tag) && g_net_on && !g_net_busy) net_scan();
    return G_SOURCE_REMOVE;
}
static void net_scan_done(const char* out, gboolean ok, gpointer tag) {
    (void)ok;
    if (NET_STALE(tag)) return;
    g_net_scanning = FALSE;
    gtk_spinner_stop(GTK_SPINNER(g_net_spin)); gtk_widget_hide(g_net_spin);
    if (!g_net_on) return;
    const char* o = out ? out : "";
    clear_box(g_net_av_card);
    GtkWidget* lb = card_list(g_net_av_card);
    g_signal_connect(lb, "row-activated", G_CALLBACK(net_row_activated), NULL);
    GPtrArray* nets = jarr(o, "networks");
    int shown = 0;
    for (guint i = 0; i < nets->len; i++) {
        char* e = (char*)nets->pdata[i];
        if (jbool(e, "current")) continue;           /* it has its own card */
        char* ss = jget(e, "ssid"); char* sec = jget(e, "security"); char* bars = jget(e, "bars");
        char* band = jget(e, "band");
        gboolean saved = jbool(e, "saved");
        const char* sub = saved ? _("Saved") : !strcmp(sec, "open") ? _("Open") : !strcmp(sec, "eap") ? _("Enterprise") : "";
        GtkWidget* subl = NULL;
        GtkWidget* row = tap_row(lb, wifi_glyph(atoi(bars), strcmp(sec, "open") != 0, FALSE), ss,
                                 !strcmp(band, "5") ? badge_new("5G") : NULL, sub, NULL, &subl);
        NetRow* n = g_new0(NetRow, 1); n->ssid = g_strdup(ss); n->security = g_strdup(sec); n->saved = saved; n->sub = subl;
        g_object_set_data_full(G_OBJECT(row), "net", n, netrow_free);
        shown++;
        g_free(ss); g_free(sec); g_free(bars); g_free(band);
    }
    g_ptr_array_free(nets, TRUE);
    if (!shown) {
        GtkWidget* r = gtk_list_box_row_new();
        gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(r), FALSE);
        GtkWidget* l = label_cls(_("No other networks found nearby."), "xxri-row-sub", 0);
        css(l, "xxri-taprow");
        gtk_container_add(GTK_CONTAINER(r), l);
        gtk_list_box_insert(GTK_LIST_BOX(lb), r, -1);
    }
    /* Add network, last */
    GtkWidget* plus = gtk_label_new("+");
    css(plus, "xxri-plus");
    GtkWidget* add = tap_row(lb, plus, _("Add network"), NULL, NULL, NULL, NULL);
    g_object_set_data(G_OBJECT(add), "add", GINT_TO_POINTER(1));
    g_signal_connect(lb, "row-activated", G_CALLBACK(+[](GtkListBox* l, GtkListBoxRow* r, gpointer u) {
        (void)l; (void)u; if (g_object_get_data(G_OBJECT(r), "add")) net_add(NULL, NULL); }), NULL);
    reveal(g_net_av_head); reveal(g_net_av_card);
    if (!g_net_timer) g_net_timer = g_timeout_add_seconds(20, net_rescan_tick, tag);
}
static void net_scan(void) {
    if (g_net_scanning) return;
    g_net_scanning = TRUE;
    gtk_widget_show(g_net_spin); gtk_spinner_start(GTK_SPINNER(g_net_spin));
    const char* argv[] = { "xxri-network", "scan", "--json", NULL };
    run_async(argv, net_scan_done, NET_TAG);
}

static void net_status_done(const char* out, gboolean ok, gpointer tag) {
    (void)ok;
    if (NET_STALE(tag)) return;
    clear_box(g_net_eth_card);
    GPtrArray* ifs = jarr(out ? out : "", "interfaces");
    int n = 0;
    for (guint i = 0; i < ifs->len; i++) {
        char* o = (char*)ifs->pdata[i];
        char* type = jget(o, "type"); char* name = jget(o, "interface");
        if (strcmp(type, "ethernet") || !strncmp(name,"dummy",5) || !strncmp(name,"tunl",4) ||
            !strncmp(name,"sit",3) || !strncmp(name,"docker",6)) { g_free(type); g_free(name); continue; }
        char* ip = jget(o, "ip"); char* st = jget(o, "state");
        gboolean carrier = jbool(o, "carrier");
        char sub[160];
        if (*ip) snprintf(sub, sizeof sub, _("Connected \xc2\xb7 %s"), ip);
        else if (!carrier) g_strlcpy(sub, _("Cable unplugged"), sizeof sub);
        else snprintf(sub, sizeof sub, _("Getting an address\xe2\x80\xa6"));
        char t[64]; snprintf(t, sizeof t, _("Ethernet (%s)"), name);
        card_icon_row(g_net_eth_card, "devices", t, sub, NULL);
        n++;
        g_free(ip); g_free(st); g_free(type); g_free(name);
    }
    g_ptr_array_free(ifs, TRUE);
    if (n) { reveal(g_net_eth_head); reveal(g_net_eth_card); }
    else { conceal(g_net_eth_head); conceal(g_net_eth_card); }
}

static void net_wifi_done(const char* out, gboolean ok, gpointer tag) {
    (void)ok;
    if (NET_STALE(tag)) return;
    const char* o = out ? out : "";
    gboolean present = jbool(o, "present"), hard = jbool(o, "hard_blocked");
    g_net_on = present && jbool(o, "enabled");
    if (!present) {
        pill_set(&g_net_pill, FALSE, FALSE, _("No Wi-Fi adapter"));
        gtk_label_set_text(GTK_LABEL(g_net_msg),
            _("No wireless adapter was detected. Drivers ship with XXRI OS; if your adapter needs firmware, About Device shows what is missing."));
        reveal(g_net_msg);
    } else if (hard) {
        pill_set(&g_net_pill, FALSE, FALSE, _("Switched off by the computer's wireless switch or key"));
        conceal(g_net_msg);
    } else {
        pill_set(&g_net_pill, g_net_on, TRUE, NULL);
        if (g_net_on) conceal(g_net_msg);
        else {
            gtk_label_set_text(GTK_LABEL(g_net_msg), _("Turn on Wi-Fi to see the networks around you."));
            reveal(g_net_msg);
        }
    }
    if (g_net_on) {
        const char* a1[] = { "xxri-network", "info", "--json", NULL };
        run_async(a1, net_info_done, tag);
        net_scan();
    } else {
        conceal(g_net_cur_head); conceal(g_net_cur_card);
        conceal(g_net_av_head); conceal(g_net_av_card);
    }
}
static void net_refresh(void) {
    const char* a0[] = { "xxri-network", "wifi", "--json", NULL };
    run_async(a0, net_wifi_done, NET_TAG);
    const char* a2[] = { "xxri-network", "status", "--json", NULL };
    run_async(a2, net_status_done, NET_TAG);
}
static void net_switch_done(const char* out, gboolean ok, gpointer tag) {
    (void)out; (void)ok;
    if (!NET_STALE(tag)) net_refresh();
}
static void net_toggle(GtkSwitch* sw, GParamSpec* ps, gpointer u) {
    (void)ps; (void)u;
    gboolean on = gtk_switch_get_active(sw);
    gtk_widget_set_sensitive(GTK_WIDGET(sw), FALSE);
    gtk_label_set_text(GTK_LABEL(g_net_pill.title), on ? _("Turning on\xe2\x80\xa6") : _("Turning off\xe2\x80\xa6"));
    const char* argv[] = { "xxri-network", "wifi", on ? "on" : "off", "--json", NULL };
    run_async(argv, net_switch_done, NET_TAG);
}

static void build_wifi(GtkWidget* box) {
    g_net_gen++;
    if (g_net_timer) { g_source_remove(g_net_timer); g_net_timer = 0; }
    g_net_on = g_net_scanning = g_net_busy = FALSE;
    g_net_pill = pill_new(box, G_CALLBACK(net_toggle));
    g_net_msg = hint("");
    gtk_widget_set_margin_top(g_net_msg, 8);
    gtk_box_pack_start(GTK_BOX(box), g_net_msg, FALSE, FALSE, 0);
    conceal(g_net_msg);

    g_net_cur_head = section(_("Current network"));
    g_net_cur_card = card_new();
    GtkWidget* r = NULL;
    g_net_av_head = section_with(_("Available networks"), &r);
    g_net_av_right = r;
    g_net_spin = gtk_spinner_new();
    gtk_widget_set_no_show_all(g_net_spin, TRUE);
    gtk_box_pack_end(GTK_BOX(r), g_net_spin, FALSE, FALSE, 0);
    g_net_av_card = card_new();
    g_net_eth_head = section(_("Ethernet"));
    g_net_eth_card = card_new();
    GtkWidget* ws[] = { g_net_cur_head, g_net_cur_card, g_net_av_head, g_net_av_card, g_net_eth_head, g_net_eth_card };
    for (guint i = 0; i < G_N_ELEMENTS(ws); i++) {
        gtk_box_pack_start(GTK_BOX(box), ws[i], FALSE, FALSE, 0);
        conceal(ws[i]);
    }
    net_refresh();
}

/* ---- Bluetooth ---- */
static guint g_bt_gen;
static Pill g_bt_pill;
static GtkWidget *g_bt_body, *g_bt_name_card, *g_bt_paired_head, *g_bt_paired_card,
                 *g_bt_av_head, *g_bt_av_card, *g_bt_spin, *g_bt_scanbtn, *g_bt_msg;
static gboolean g_bt_on, g_bt_scanning, g_bt_busy, g_bt_scanned;
#define BT_TAG GUINT_TO_POINTER(g_bt_gen)
#define BT_STALE(tag) (GPOINTER_TO_UINT(tag) != g_bt_gen)

static void bt_refresh(void);
static void bt_scan(void);
typedef struct { char *mac, *name, *icon; gboolean paired, connected; GtkWidget* sub; } BtRow;
static void btrow_free(gpointer p) { BtRow* b = (BtRow*)p; g_free(b->mac); g_free(b->name); g_free(b->icon); g_free(b); }
static const char* bt_kind(const char* icon) {
    if (!icon) return "";
    if (g_str_has_prefix(icon, "audio-head")) return _("Headphones");
    if (g_str_has_prefix(icon, "audio")) return _("Audio");
    if (g_str_has_prefix(icon, "input-keyboard")) return _("Keyboard");
    if (g_str_has_prefix(icon, "input-mouse")) return _("Mouse");
    if (g_str_has_prefix(icon, "input-tablet")) return _("Tablet");
    if (g_str_has_prefix(icon, "input-gaming")) return _("Game controller");
    if (g_str_has_prefix(icon, "phone")) return _("Phone");
    if (g_str_has_prefix(icon, "computer")) return _("Computer");
    return "";
}

/* pairing: a dialog that follows `xxri-bluetooth pair` line by line */
typedef struct {
    guint gen; GSubprocess* p; GDataInputStream* in; GOutputStream* out;
    GtkWidget *dlg, *title, *text, *code, *spin, *yes, *no, *close;
    char *mac, *name, *result;
    gboolean waiting;                     /* a yes/no is pending */
    gboolean done;                        /* the backend has exited */
    gboolean cancelled;                   /* the user said no: close quietly */
} PairJob;
static void pair_free(PairJob* j) {
    if (j->in) g_object_unref(j->in);
    if (j->p) g_object_unref(j->p);
    g_free(j->mac); g_free(j->name); g_free(j->result); g_free(j);
}
static void pair_answer(PairJob* j, gboolean yes) {
    if (!j->waiting) return;
    j->waiting = FALSE;
    if (!yes) j->cancelled = TRUE;
    g_output_stream_write_all(j->out, yes ? "yes\n" : "no\n", yes ? 4 : 3, NULL, NULL, NULL);
    g_output_stream_flush(j->out, NULL, NULL);
    gtk_widget_hide(j->yes);
    gtk_widget_show(j->spin); gtk_spinner_start(GTK_SPINNER(j->spin));
    gtk_label_set_text(GTK_LABEL(j->text), yes ? _("Pairing\xe2\x80\xa6") : _("Cancelling\xe2\x80\xa6"));
}
static void pair_show_code(PairJob* j, const char* code, const char* what) {
    gtk_label_set_text(GTK_LABEL(j->code), code);
    gtk_widget_set_no_show_all(j->code, FALSE); gtk_widget_show(j->code);
    char t[400];
    if (!strcmp(what, "confirm")) {
        snprintf(t, sizeof t, _("Make sure \xe2\x80\x9c%s\xe2\x80\x9d shows this code, then choose Pair."), j->name);
        gtk_widget_show(j->yes); gtk_widget_show(j->no);
        gtk_spinner_stop(GTK_SPINNER(j->spin)); gtk_widget_hide(j->spin);
        j->waiting = TRUE;
    } else {
        snprintf(t, sizeof t, _("Type this code on \xe2\x80\x9c%s\xe2\x80\x9d, then press Enter."), j->name);
    }
    gtk_label_set_text(GTK_LABEL(j->text), t);
}
static void pair_finish(PairJob* j) {
    j->done = TRUE; j->waiting = FALSE;
    gtk_spinner_stop(GTK_SPINNER(j->spin)); gtk_widget_hide(j->spin);
    const char* o = j->result ? j->result : "";
    gboolean ok = jbool(o, "paired") || jbool(o, "connected");
    if (ok || j->cancelled) {
        if (j->dlg) gtk_widget_destroy(j->dlg);
        j->dlg = NULL;
    } else if (j->dlg) {
        char* why = jget(o, "reason");
        char* s = sentence(*why ? why : _("pairing did not complete"));
        gtk_label_set_text(GTK_LABEL(j->title), _("Couldn't pair"));
        gtk_label_set_text(GTK_LABEL(j->text), s);
        gtk_widget_hide(j->code); gtk_widget_hide(j->yes); gtk_widget_hide(j->no);
        gtk_widget_set_no_show_all(j->close, FALSE); gtk_widget_show(j->close);
        g_free(why); g_free(s);
    }
    g_bt_busy = FALSE;
    if (j->gen == g_bt_gen) bt_refresh();
}
static void pair_line(GObject* src, GAsyncResult* res, gpointer u) {
    PairJob* j = (PairJob*)u;
    gsize n = 0;
    char* line = g_data_input_stream_read_line_finish_utf8(G_DATA_INPUT_STREAM(src), res, &n, NULL);
    if (!line) {                          /* end of output: the backend is done */
        pair_finish(j);
        if (!j->dlg) pair_free(j); else g_object_set_data_full(G_OBJECT(j->dlg), "job", j, (GDestroyNotify)pair_free);
        return;
    }
    if (j->dlg) {
        if (line[0] == '{') { g_free(j->result); j->result = g_strdup(line); }
        else if (g_str_has_prefix(line, "confirm ")) pair_show_code(j, line + 8, "confirm");
        else if (g_str_has_prefix(line, "passkey ")) pair_show_code(j, line + 8, "passkey");
        else if (g_str_has_prefix(line, "pin "))     pair_show_code(j, line + 4, "pin");
    } else if (line[0] == '{') { g_free(j->result); j->result = g_strdup(line); }
    g_free(line);
    g_data_input_stream_read_line_async(j->in, G_PRIORITY_DEFAULT, NULL, pair_line, j);
}
static void bt_pair(const char* mac, const char* name) {
    if (g_bt_busy) return;
    const char* argv[] = { "xxri-bluetooth", "pair", mac, "--json", NULL };
    GSubprocess* p = g_subprocess_newv(argv, (GSubprocessFlags)(G_SUBPROCESS_FLAGS_STDIN_PIPE |
                                       G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_SILENCE), NULL);
    if (!p) { alert(_("Couldn't pair"), _("The Bluetooth backend did not start.")); return; }
    g_bt_busy = TRUE;
    PairJob* j = g_new0(PairJob, 1);
    j->gen = g_bt_gen; j->p = p; j->mac = g_strdup(mac); j->name = g_strdup(name && *name ? name : mac);
    j->out = g_subprocess_get_stdin_pipe(p);
    j->in = g_data_input_stream_new(g_subprocess_get_stdout_pipe(p));
    /* a dialog without a nested loop: it stays up while the lines come in */
    j->dlg = gtk_dialog_new();
    gtk_window_set_transient_for(GTK_WINDOW(j->dlg), GTK_WINDOW(g_win));
    gtk_window_set_modal(GTK_WINDOW(j->dlg), TRUE);
    gtk_window_set_title(GTK_WINDOW(j->dlg), _("Bluetooth"));
    GtkWidget* c = gtk_dialog_get_content_area(GTK_DIALOG(j->dlg));
    gtk_box_set_spacing(GTK_BOX(c), 8);
    gtk_widget_set_size_request(c, 360, -1);
    char t[300]; snprintf(t, sizeof t, _("Pairing with \xe2\x80\x9c%s\xe2\x80\x9d"), j->name);
    j->title = dlg_heading(t);
    gtk_box_pack_start(GTK_BOX(c), j->title, FALSE, FALSE, 0);
    j->code = label_cls("", "xxri-pair-code", 0.5);
    gtk_widget_set_no_show_all(j->code, TRUE);
    gtk_box_pack_start(GTK_BOX(c), j->code, FALSE, FALSE, 4);
    GtkWidget* hb = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_margin_start(hb, 10); gtk_widget_set_margin_end(hb, 10);
    j->spin = gtk_spinner_new(); gtk_spinner_start(GTK_SPINNER(j->spin));
    gtk_box_pack_start(GTK_BOX(hb), j->spin, FALSE, FALSE, 0);
    j->text = label_cls(_("Waiting for the device\xe2\x80\xa6"), "xxri-dlg-text", 0);
    gtk_box_pack_start(GTK_BOX(hb), j->text, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(c), hb, FALSE, FALSE, 4);
    j->no = gtk_dialog_add_button(GTK_DIALOG(j->dlg), _("Cancel"), GTK_RESPONSE_NO);
    j->yes = gtk_dialog_add_button(GTK_DIALOG(j->dlg), _("Pair"), GTK_RESPONSE_YES);
    css(j->yes, "xxri-accent");
    j->close = gtk_dialog_add_button(GTK_DIALOG(j->dlg), _("Close"), GTK_RESPONSE_CLOSE);
    /* (add_button shows each button) Pair and Close come later */
    gtk_widget_set_no_show_all(j->yes, TRUE); gtk_widget_set_no_show_all(j->close, TRUE);
    gtk_widget_hide(j->yes); gtk_widget_hide(j->close);
    g_signal_connect(j->dlg, "response", G_CALLBACK(+[](GtkDialog* d, int r, gpointer u) {
        PairJob* jb = (PairJob*)g_object_get_data(G_OBJECT(d), "job-live");
        if (r == GTK_RESPONSE_YES && jb) { pair_answer(jb, TRUE); return; }
        if (jb && jb->waiting) { pair_answer(jb, FALSE); return; }
        if (jb && !jb->done) {
            /* Cancel while the device is still thinking: stop the backend,
               it cancels the pairing on the way out */
            jb->cancelled = TRUE;
            g_subprocess_send_signal(jb->p, SIGTERM);
            gtk_label_set_text(GTK_LABEL(jb->text), _("Cancelling\xe2\x80\xa6"));
            return;
        }
        gtk_widget_destroy(GTK_WIDGET(d)); }), NULL);
    g_object_set_data(G_OBJECT(j->dlg), "job-live", j);
    g_signal_connect(j->dlg, "destroy", G_CALLBACK(+[](GtkWidget* w, gpointer u) {
        PairJob* jb = (PairJob*)g_object_get_data(G_OBJECT(w), "job-live");
        if (jb && jb->dlg == w) jb->dlg = NULL; }), NULL);
    gtk_widget_show_all(j->dlg);
    g_data_input_stream_read_line_async(j->in, G_PRIORITY_DEFAULT, NULL, pair_line, j);
}

static void bt_dev_done(const char* out, gboolean ok, gpointer tag) {
    g_bt_busy = FALSE;
    if (BT_STALE(tag)) return;
    if (!ok) {
        char* why = jget(out ? out : "", "reason");
        char* s = sentence(*why ? why : _("the device did not answer"));
        alert(_("Bluetooth"), s); g_free(s); g_free(why);
    }
    bt_refresh();
}
static void bt_dev(const char* verb, const char* mac, GtkWidget* sub, const char* doing) {
    if (g_bt_busy) return;
    g_bt_busy = TRUE;
    if (sub && doing) row_set_sub(sub, doing);
    const char* argv[] = { "xxri-bluetooth", verb, mac, "--json", NULL };
    run_async(argv, bt_dev_done, BT_TAG);
}
static void bt_menu_pick(GtkMenuItem* it, gpointer u) {
    BtRow* b = (BtRow*)u;
    const char* verb = (const char*)g_object_get_data(G_OBJECT(it), "verb");
    if (!strcmp(verb, "remove")) {
        GtkWidget* d = gtk_message_dialog_new(GTK_WINDOW(g_win), GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION,
                                              GTK_BUTTONS_NONE, _("Unpair \xe2\x80\x9c%s\xe2\x80\x9d?"), b->name);
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(d),
            _("To use it again you will have to pair it again."));
        gtk_dialog_add_button(GTK_DIALOG(d), _("Cancel"), GTK_RESPONSE_CANCEL);
        css(gtk_dialog_add_button(GTK_DIALOG(d), _("Unpair"), GTK_RESPONSE_OK), "destructive-action");
        int r = gtk_dialog_run(GTK_DIALOG(d)); gtk_widget_destroy(d);
        if (r != GTK_RESPONSE_OK) return;
        bt_dev("remove", b->mac, b->sub, _("Unpairing\xe2\x80\xa6"));
    } else if (!strcmp(verb, "connect")) bt_dev("connect", b->mac, b->sub, _("Connecting\xe2\x80\xa6"));
    else bt_dev("disconnect", b->mac, b->sub, _("Disconnecting\xe2\x80\xa6"));
}
static void bt_gear(GtkWidget* btn, gpointer u) {
    BtRow* b = (BtRow*)u;
    GtkWidget* m = gtk_menu_new();
    const char* items[][2] = { { b->connected ? _("Disconnect") : _("Connect"), b->connected ? "disconnect" : "connect" },
                               { _("Unpair"), "remove" } };
    for (int i = 0; i < 2; i++) {
        GtkWidget* it = gtk_menu_item_new_with_label(items[i][0]);
        g_object_set_data(G_OBJECT(it), "verb", (gpointer)items[i][1]);
        g_signal_connect(it, "activate", G_CALLBACK(bt_menu_pick), b);
        gtk_menu_shell_append(GTK_MENU_SHELL(m), it);
    }
    gtk_widget_show_all(m);
    gtk_menu_popup_at_widget(GTK_MENU(m), btn, GDK_GRAVITY_SOUTH_EAST, GDK_GRAVITY_NORTH_EAST, NULL);
}
static void bt_row_activated(GtkListBox* lb, GtkListBoxRow* row, gpointer u) {
    (void)lb; (void)u;
    BtRow* b = (BtRow*)g_object_get_data(G_OBJECT(row), "bt");
    if (!b || g_bt_busy) return;
    if (!b->paired) bt_pair(b->mac, b->name);
    else if (b->connected) bt_dev("disconnect", b->mac, b->sub, _("Disconnecting\xe2\x80\xa6"));
    else bt_dev("connect", b->mac, b->sub, _("Connecting\xe2\x80\xa6"));
}
static void bt_fill(GtkWidget* card, const char* json, const char* key, gboolean paired) {
    clear_box(card);
    GtkWidget* lb = card_list(card);
    g_signal_connect(lb, "row-activated", G_CALLBACK(bt_row_activated), NULL);
    GPtrArray* a = jarr(json, key);
    for (guint i = 0; i < a->len; i++) {
        char* o = (char*)a->pdata[i];
        char* mac = jget(o, "mac"); char* name = jget(o, "name"); char* icon = jget(o, "icon");
        gboolean conn = jbool(o, "connected");
        BtRow* b = g_new0(BtRow, 1);
        b->mac = g_strdup(mac); b->name = g_strdup(*name ? name : mac); b->icon = g_strdup(icon);
        b->paired = paired; b->connected = conn;
        const char* kind = bt_kind(icon);
        char sub[96];
        if (paired) snprintf(sub, sizeof sub, "%s", conn ? _("Connected") : _("Not connected"));
        else snprintf(sub, sizeof sub, "%s", kind);
        GtkWidget* gear = NULL;
        if (paired) { gear = gear_btn(); g_signal_connect(gear, "clicked", G_CALLBACK(bt_gear), b); }
        GtkWidget* subl = NULL;
        GtkWidget* row = tap_row(lb, bt_glyph(icon), b->name, NULL, sub, gear, &subl);
        b->sub = subl;
        if (conn) css(subl, "xxri-ok");
        g_object_set_data_full(G_OBJECT(row), "bt", b, btrow_free);
        g_free(mac); g_free(name); g_free(icon);
    }
    if (!a->len) {
        GtkWidget* r = gtk_list_box_row_new();
        gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(r), FALSE);
        GtkWidget* l = label_cls(paired ? _("No paired devices yet.") :
                                 g_bt_scanning ? _("Looking for devices\xe2\x80\xa6") :
                                 _("Nothing found. Put the device in pairing mode and choose Scan."), "xxri-row-sub", 0);
        css(l, "xxri-taprow");
        gtk_container_add(GTK_CONTAINER(r), l);
        gtk_list_box_insert(GTK_LIST_BOX(lb), r, -1);
    }
    g_ptr_array_free(a, TRUE);
}

static void bt_devices_done(const char* out, gboolean ok, gpointer tag) {
    (void)ok;
    if (BT_STALE(tag) || !g_bt_on) return;
    bt_fill(g_bt_paired_card, out ? out : "", "devices", TRUE);
    reveal(g_bt_paired_head); reveal(g_bt_paired_card);
}
static void bt_scan_done(const char* out, gboolean ok, gpointer tag) {
    (void)ok;
    if (BT_STALE(tag)) return;
    g_bt_scanning = FALSE;
    gtk_spinner_stop(GTK_SPINNER(g_bt_spin)); gtk_widget_hide(g_bt_spin);
    gtk_widget_set_sensitive(g_bt_scanbtn, TRUE);
    if (!g_bt_on) return;
    bt_fill(g_bt_av_card, out ? out : "", "scan", FALSE);
    reveal(g_bt_av_head); reveal(g_bt_av_card);
}
static void bt_scan(void) {
    if (g_bt_scanning || g_bt_busy) return;
    g_bt_scanning = g_bt_scanned = TRUE;
    gtk_widget_show(g_bt_spin); gtk_spinner_start(GTK_SPINNER(g_bt_spin));
    gtk_widget_set_sensitive(g_bt_scanbtn, FALSE);
    if (!gtk_widget_get_visible(g_bt_av_card)) { bt_fill(g_bt_av_card, "", "scan", FALSE); reveal(g_bt_av_head); reveal(g_bt_av_card); }
    const char* argv[] = { "xxri-bluetooth", "scan", "10", "--json", NULL };
    run_async(argv, bt_scan_done, BT_TAG);
}
static void bt_rename(GtkWidget* w, gpointer u) {
    (void)w;
    const char* cur = (const char*)u;
    GtkWidget* d = gtk_dialog_new_with_buttons(_("Bluetooth"), GTK_WINDOW(g_win), GTK_DIALOG_MODAL,
                                               _("Cancel"), GTK_RESPONSE_CANCEL, _("Rename"), GTK_RESPONSE_OK, NULL);
    css(gtk_dialog_get_widget_for_response(GTK_DIALOG(d), GTK_RESPONSE_OK), "xxri-accent");
    gtk_dialog_set_default_response(GTK_DIALOG(d), GTK_RESPONSE_OK);
    GtkWidget* c = gtk_dialog_get_content_area(GTK_DIALOG(d));
    gtk_box_set_spacing(GTK_BOX(c), 8);
    gtk_widget_set_size_request(c, 340, -1);
    gtk_box_pack_start(GTK_BOX(c), dlg_heading(_("Device name")), FALSE, FALSE, 0);
    GtkWidget* t = label_cls(_("The name phones, headphones and other computers see."), "xxri-dlg-text", 0);
    gtk_widget_set_margin_start(t, 10);
    gtk_box_pack_start(GTK_BOX(c), t, FALSE, FALSE, 0);
    GtkWidget* en = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(en), cur ? cur : "");
    gtk_entry_set_max_length(GTK_ENTRY(en), 248);
    gtk_entry_set_activates_default(GTK_ENTRY(en), TRUE);
    gtk_widget_set_margin_start(en, 10); gtk_widget_set_margin_end(en, 10);
    gtk_box_pack_start(GTK_BOX(c), en, FALSE, FALSE, 0);
    gtk_widget_show_all(d);
    int r = gtk_dialog_run(GTK_DIALOG(d));
    char* nm = g_strstrip(g_strdup(gtk_entry_get_text(GTK_ENTRY(en))));
    gtk_widget_destroy(d);
    if (r == GTK_RESPONSE_OK && *nm) {
        const char* argv[] = { "xxri-bluetooth", "name", nm, "--json", NULL };
        run_async(argv, bt_dev_done, BT_TAG);
        g_bt_busy = TRUE;
    }
    g_free(nm);
}

static void bt_status_done(const char* out, gboolean ok, gpointer tag) {
    (void)ok;
    if (BT_STALE(tag)) return;
    const char* o = out ? out : "";
    if (!jbool(o, "available")) {
        /* no radio at all isn't "off": the page becomes the explanation */
        clear_box(g_bt_body);
        char* reason = jget(o, "reason"); char* h = jget(o, "hint");
        const char* title = strstr(reason, "adapter") ? _("No Bluetooth adapter") :
                            strstr(reason, "stack") ? _("Bluetooth support not installed") : _("Bluetooth is unavailable");
        char* r = sentence(*reason ? reason : _("no Bluetooth adapter was detected"));
        char msg[512];
        snprintf(msg, sizeof msg, "%s\n%s", r,
                 *h ? h : _("Connect a Bluetooth adapter to pair headphones, keyboards and other devices."));
        g_bt_pill.box = NULL;
        page_empty(g_bt_body, "bluetooth", title, msg);
        gtk_widget_show_all(g_bt_body);
        g_free(r); g_free(reason); g_free(h);
        return;
    }
    if (!g_bt_pill.box) return;
    gboolean hard = jbool(o, "hard_blocked");
    g_bt_on = jbool(o, "enabled");
    if (hard) pill_set(&g_bt_pill, FALSE, FALSE, _("Switched off by the computer's wireless switch or key"));
    else pill_set(&g_bt_pill, g_bt_on, TRUE, NULL);
    /* the name row */
    clear_box(g_bt_name_card);
    char* name = jget(o, "name");
    GtkWidget* ren = gtk_button_new_with_label(_("Rename"));
    css(ren, "xxri-chip");
    g_object_set_data_full(G_OBJECT(ren), "name", g_strdup(name), g_free);
    g_signal_connect(ren, "clicked", G_CALLBACK(bt_rename), g_object_get_data(G_OBJECT(ren), "name"));
    card_action(g_bt_name_card, _("Device name"), name, ren);
    reveal(g_bt_name_card);
    g_free(name);
    if (!g_bt_on) {
        gtk_label_set_text(GTK_LABEL(g_bt_msg), hard ? "" : _("Turn on Bluetooth to connect headphones, keyboards and other devices."));
        if (!hard) reveal(g_bt_msg); else conceal(g_bt_msg);
        GtkWidget* ws[] = { g_bt_paired_head, g_bt_paired_card, g_bt_av_head, g_bt_av_card };
        for (guint i = 0; i < G_N_ELEMENTS(ws); i++) conceal(ws[i]);
        return;
    }
    conceal(g_bt_msg);
    const char* argv[] = { "xxri-bluetooth", "devices", "--json", NULL };
    run_async(argv, bt_devices_done, tag);
    /* look around once when the page opens, as phones do */
    if (!g_bt_scanned) bt_scan();
    else { reveal(g_bt_av_head); reveal(g_bt_av_card); }
}
static void bt_refresh(void) {
    const char* argv[] = { "xxri-bluetooth", "status", "--json", NULL };
    run_async(argv, bt_status_done, BT_TAG);
}
static void bt_switch_done(const char* out, gboolean ok, gpointer tag) {
    (void)out; (void)ok;
    if (!BT_STALE(tag)) { g_bt_scanned = FALSE; bt_refresh(); }
}
static void bt_toggle(GtkSwitch* sw, GParamSpec* ps, gpointer u) {
    (void)ps; (void)u;
    gboolean on = gtk_switch_get_active(sw);
    gtk_widget_set_sensitive(GTK_WIDGET(sw), FALSE);
    gtk_label_set_text(GTK_LABEL(g_bt_pill.title), on ? _("Turning on\xe2\x80\xa6") : _("Turning off\xe2\x80\xa6"));
    const char* argv[] = { "xxri-bluetooth", "power", on ? "on" : "off", "--json", NULL };
    run_async(argv, bt_switch_done, BT_TAG);
}

static void build_bluetooth(GtkWidget* box) {
    g_bt_gen++;
    g_bt_on = g_bt_scanning = g_bt_busy = g_bt_scanned = FALSE;
    /* everything lives in one box, so an unavailable adapter can replace it */
    g_bt_body = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_box_pack_start(GTK_BOX(box), g_bt_body, TRUE, TRUE, 0);
    g_bt_pill = pill_new(g_bt_body, G_CALLBACK(bt_toggle));
    g_bt_msg = hint("");
    gtk_widget_set_margin_top(g_bt_msg, 8);
    gtk_box_pack_start(GTK_BOX(g_bt_body), g_bt_msg, FALSE, FALSE, 0);
    conceal(g_bt_msg);
    g_bt_name_card = card_new();
    gtk_widget_set_margin_top(g_bt_name_card, 10);
    gtk_box_pack_start(GTK_BOX(g_bt_body), g_bt_name_card, FALSE, FALSE, 0);
    conceal(g_bt_name_card);
    g_bt_paired_head = section(_("Paired devices"));
    g_bt_paired_card = card_new();
    GtkWidget* r = NULL;
    g_bt_av_head = section_with(_("Available devices"), &r);
    g_bt_scanbtn = gtk_button_new_with_label(_("Scan"));
    css(g_bt_scanbtn, "xxri-chip");
    g_signal_connect(g_bt_scanbtn, "clicked", G_CALLBACK(+[](GtkWidget* w, gpointer p){ (void)w;(void)p; bt_scan(); }), NULL);
    gtk_box_pack_end(GTK_BOX(r), g_bt_scanbtn, FALSE, FALSE, 0);
    g_bt_spin = gtk_spinner_new();
    gtk_widget_set_no_show_all(g_bt_spin, TRUE);
    gtk_box_pack_end(GTK_BOX(r), g_bt_spin, FALSE, FALSE, 0);
    g_bt_av_card = card_new();
    GtkWidget* ws[] = { g_bt_paired_head, g_bt_paired_card, g_bt_av_head, g_bt_av_card };
    for (guint i = 0; i < G_N_ELEMENTS(ws); i++) {
        gtk_box_pack_start(GTK_BOX(g_bt_body), ws[i], FALSE, FALSE, 0);
        conceal(ws[i]);
    }
    bt_refresh();
}

static GtkWidget* build_page_content(const char* id) {
    /* width-capped centered column inside a scroller */
    GtkWidget* scr = gtk_scrolled_window_new(NULL,NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scr),GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);
    GtkWidget* outer = gtk_box_new(GTK_ORIENTATION_VERTICAL,0);
    css(outer,"xxri-content");
    GtkWidget* col = gtk_box_new(GTK_ORIENTATION_VERTICAL,4);
    gtk_widget_set_halign(col, GTK_ALIGN_FILL);
    gtk_widget_set_margin_start(col,32); gtk_widget_set_margin_end(col,32);
    gtk_widget_set_margin_top(col,24); gtk_widget_set_margin_bottom(col,32);
    gtk_widget_set_size_request(col, 600, -1);
    /* page title */
    const char* label = _("Settings");
    for (int i=0;i<NNAV;i++) if(!strcmp(NAV[i].id,id)) label=_(NAV[i].label);
    GtkWidget* tl=label_cls(label,"xxri-page-title",0);
    gtk_widget_set_margin_bottom(tl,8);
    gtk_box_pack_start(GTK_BOX(col),tl,FALSE,FALSE,0);
    /* body */
    if      (!strcmp(id,"wifi"))      build_wifi(col);
    else if (!strcmp(id,"bluetooth")) build_bluetooth(col);
    else if (!strcmp(id,"devices"))   build_devices(col);
    else if (!strcmp(id,"wallpaper")) build_wallpaper(col);
    else if (!strcmp(id,"sound"))     build_sound(col);
    else if (!strcmp(id,"display"))   build_display(col);
    else if (!strcmp(id,"storage"))   build_storage(col);
    else if (!strcmp(id,"battery"))   build_battery(col);
    else if (!strcmp(id,"apps"))      build_apps(col);
    else if (!strcmp(id,"general"))   build_general(col);
    else if (!strcmp(id,"update"))    build_update(col);
    else if (!strcmp(id,"about"))     build_about(col);
    /* fill, not centre: centred left bare bands down both sides. 600px is
       only a minimum */
    GtkWidget* center = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);
    gtk_widget_set_halign(center, GTK_ALIGN_FILL);
    gtk_box_pack_start(GTK_BOX(center), col, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(outer), center, TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(scr), outer);
    gtk_widget_show_all(scr);
    return scr;
}

static void rebuild(const char* id) {
    GtkWidget* old = gtk_stack_get_child_by_name(GTK_STACK(g_stack), id);
    GtkWidget* neu = build_page_content(id);
    gtk_stack_add_named(GTK_STACK(g_stack), neu, "__tmp");
    gtk_stack_set_visible_child(GTK_STACK(g_stack), neu);
    if (old) gtk_container_remove(GTK_CONTAINER(g_stack), old);
    /* rename tmp -> id */
    gtk_container_child_set(GTK_CONTAINER(g_stack), neu, "name", id, NULL);
}

/* pages are always rebuilt, so they show live data */
static void on_nav(GtkListBox* lb, GtkListBoxRow* row, gpointer u) {
    (void)lb;(void)u;
    if (!row) return;
    const char* id = (const char*)g_object_get_data(G_OBJECT(row),"id");
    rebuild(id);
}

static void activate(GtkApplication* app, gpointer u) {
    (void)u;
    GtkCssProvider* prov = gtk_css_provider_new();
    gtk_css_provider_load_from_path(prov, "/usr/local/share/xxri-settings/xxri.css", NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(prov), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION+1);
    xxri_dialogs_init();

    g_win = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(g_win), "XXRI Settings");
    gtk_window_set_default_size(GTK_WINDOW(g_win), 980, 660);
    css(g_win, "xxri-root");
    /* compositor: ARGB window, and .composited makes the sidebar translucent
     * in the stylesheet (content stays opaque). Otherwise it stays solid. */
    {
        GdkScreen* sc0 = gtk_widget_get_screen(g_win);
        GdkVisual* rgba = gdk_screen_get_rgba_visual(sc0);
        if (rgba && gdk_screen_is_composited(sc0)) {
            gtk_widget_set_visual(g_win, rgba);
            css(g_win, "composited");
        }
    }

    apply_self_look();

    GtkWidget* hb = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);

    GtkWidget* side = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    css(side, "xxri-sidebar");
    gtk_widget_set_size_request(side, 250, -1);

    /* window controls inside the sidebar, as in the mockup (no titlebar) */
    XxriChrome* chrome = xxri_chrome_new(g_win);
    gtk_container_add(GTK_CONTAINER(g_win), xxri_chrome_resizable(chrome, hb));
    GtkWidget* crow = gtk_event_box_new();
    gtk_event_box_set_visible_window(GTK_EVENT_BOX(crow), TRUE);
    css(crow, "xxri-chrome-row");
    gtk_container_add(GTK_CONTAINER(crow), xxri_chrome_controls(chrome));
    xxri_chrome_drag_area(chrome, crow);
    gtk_box_pack_start(GTK_BOX(side), crow, FALSE, FALSE, 0);

    /* header, also a drag handle */
    GtkWidget* hdrbox = gtk_event_box_new();
    gtk_event_box_set_visible_window(GTK_EVENT_BOX(hdrbox), TRUE);
    css(hdrbox, "xxri-chrome-row");
    xxri_chrome_drag_area(chrome, hdrbox);
    GtkWidget* hdr = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8); css(hdr,"xxri-sidebar-header");
    GtkWidget* stitle = label_cls(_("Settings"),"xxri-sidebar-title",0);
    gtk_box_pack_start(GTK_BOX(hdr), stitle, TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(hdrbox), hdr);
    gtk_box_pack_start(GTK_BOX(side), hdrbox, FALSE, FALSE, 0);
    /* user chip */
    GtkWidget* chip = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,10); css(chip,"xxri-userchip");
    GtkWidget* cv=gtk_box_new(GTK_ORIENTATION_VERTICAL,1);
    gtk_box_pack_start(GTK_BOX(cv),label_cls(_("Guest"),"xxri-userchip-name",0),FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(cv),label_cls("XXRI OS","xxri-userchip-sub",0),FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(chip),cv,TRUE,TRUE,0);
    GtkWidget* logo=img("help",40); gtk_box_pack_end(GTK_BOX(chip),logo,FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(side), chip, FALSE, FALSE, 0);
    /* nav list (scrolled) */
    GtkWidget* navscr=gtk_scrolled_window_new(NULL,NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(navscr),GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);
    GtkWidget* nav = gtk_list_box_new(); css(nav,"xxri-nav");
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(nav), GTK_SELECTION_SINGLE);
    for (int i=0;i<NNAV;i++) {
        if (i>0 && NAV[i].group!=NAV[i-1].group) {
            GtkWidget* srow=gtk_list_box_row_new();
            gtk_list_box_row_set_selectable(GTK_LIST_BOX_ROW(srow),FALSE);
            gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(srow),FALSE);
            GtkWidget* sep=gtk_separator_new(GTK_ORIENTATION_HORIZONTAL); css(sep,"xxri-nav-sep");
            gtk_container_add(GTK_CONTAINER(srow),sep);
            gtk_list_box_insert(GTK_LIST_BOX(nav),srow,-1);
        }
        GtkWidget* row=gtk_list_box_row_new();
        g_object_set_data(G_OBJECT(row),"id",(gpointer)NAV[i].id);
        GtkWidget* rb=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,12);
        gtk_box_pack_start(GTK_BOX(rb), img(NAV[i].icon,22), FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(rb), label_cls(_(NAV[i].label),NULL,0), TRUE, TRUE, 0);
        gtk_container_add(GTK_CONTAINER(row),rb);
        gtk_list_box_insert(GTK_LIST_BOX(nav),row,-1);
    }
    g_signal_connect(nav,"row-selected",G_CALLBACK(on_nav),NULL);
    gtk_container_add(GTK_CONTAINER(navscr),nav);
    gtk_box_pack_start(GTK_BOX(side), navscr, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hb), side, FALSE, FALSE, 0);

    g_stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(g_stack), GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);
    gtk_stack_set_transition_duration(GTK_STACK(g_stack), 180);
    gtk_box_pack_start(GTK_BOX(hb), g_stack, TRUE, TRUE, 0);

    gtk_widget_show_all(g_win);
    /* select the requested (or first) nav row */
    int startrow = 0;
    for (int i=0;i<NNAV;i++) if(!strcmp(NAV[i].id,g_start_id)) startrow=i;
    GtkListBoxRow* sr=NULL; int seen=0;
    for (int idx=0; ; idx++) { GtkListBoxRow* r=gtk_list_box_get_row_at_index(GTK_LIST_BOX(nav),idx);
        if(!r) break; if(!gtk_list_box_row_get_selectable(r)) continue; if(seen==startrow){sr=r;break;} seen++; }
    gtk_list_box_select_row(GTK_LIST_BOX(nav), sr?sr:gtk_list_box_get_row_at_index(GTK_LIST_BOX(nav),0));
}

int main(int argc, char** argv) {
    xxri_i18n_init();
    if (argc > 1 && argv[1][0] != '-') { g_start_id = argv[1]; argc = 1; }
    GtkApplication* app = gtk_application_new("best.flows.xxri.settings", G_APPLICATION_NON_UNIQUE);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    int s = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return s;
}
