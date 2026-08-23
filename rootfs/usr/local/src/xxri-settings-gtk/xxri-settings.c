/* xxri-settings - the official XXRI OS Settings application (Phase 8 redesign).
 *
 * XXRI's first GTK3 application.  Pure frontend: every hardware / system
 * action goes through an xxri-* backend (Phase 7 hardware layer, Phase 6
 * AppImage registry, plus xxri-desktop / xxri-updates).  Native GTK3:
 * GtkListBox sidebar, GtkStack pages with slide transitions, GtkSwitch /
 * GtkScale / GtkLevelBar, styled by xxri.css (One UI / GNOME / elementary
 * inspired, unmistakably XXRI).
 */
#include <gtk/gtk.h>
#include <cairo.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define ICONDIR "/usr/local/share/xxri-settings/icons"
#define WALL    "/usr/local/share/xxri-theme/wallpapers/xxri-os-lite.jpg"


/* -------------------------------------------------------- image loading -- */
/* This TC gdk-pixbuf build ships broken built-in PNG/JPEG loaders, so load
 * PNGs through Cairo (libpng direct) and convert to a GdkPixbuf.  All app
 * imagery is PNG; the wallpaper preview is a pre-rendered PNG.            */
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

/* ------------------------------------------------------------ backend --- */
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
    /* The backends wrap their payload in an envelope whose key repeats inside
       it - xxri-audio emits {"volume":{"volume":65,...}}.  A plain strstr
       stopped at the envelope and parsed "{" as the number, so the Control
       Center's slider always read 0.  Skip values that open an object or an
       array and keep looking for the scalar. */
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

/* -------------------------------------------------------------- state --- */
static GtkWidget* g_stack;
static GtkWidget* g_win;
static const char* g_start_id = "wifi";
typedef struct { const char* id; const char* label; const char* icon; int group; } Nav;
static const Nav NAV[] = {
    {"wifi","Wi-Fi & Network","wifi",0},
    {"bluetooth","Bluetooth","bluetooth",0},
    {"devices","Connected Devices","devices",0},
    {"wallpaper","Wallpaper & Style","wallpaper",1},
    {"sound","Sounds & Vibration","sound",1},
    {"help","Help & Support","help",1},
    {"storage","Storage","storage",2},
    {"battery","Battery","battery",2},
    {"apps","Apps","apps",2},
    {"general","General Management","general",3},
    {"update","Software Update","update",3},
    {"about","About Device","about",3},
};
static const int NNAV = G_N_ELEMENTS(NAV);

/* ----------------------------------------------------------- ui helpers -- */
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

/* forward */
static void show_page(const char* id);
static void rebuild(const char* id);

/* ------------------------------------------------------------- pages ----- */
/* Each builder fills a content vbox (already inside a scrolled, width-capped
 * column) for the given page id.  Backends provide all live data.          */

static void cb_reopen(GtkWidget* w, gpointer id){ (void)w; rebuild((const char*)id); }

/* Wi-Fi & Network */
static void build_wifi(GtkWidget* box) {
    char* st = run_cmd("xxri-network status --json");
    gtk_box_pack_start(GTK_BOX(box), section("Connection"), FALSE, FALSE, 0);
    GtkWidget* card = card_new();
    GPtrArray* ifs = jarr(st, "interfaces");
    gboolean any=FALSE, online=FALSE;
    for (guint i=0;i<ifs->len;i++) {
        char* o=ifs->pdata[i];
        char* name=jget(o,"interface");
        if (!strcmp(name,"lo")||!strncmp(name,"dummy",5)||!strncmp(name,"tunl",4)||!strncmp(name,"sit",3)){g_free(name);continue;}
        any=TRUE;
        char* type=jget(o,"type"); char* state=jget(o,"state"); char* ip=jget(o,"ip"); char* ssid=jget(o,"ssid");
        const char* nice = !strcmp(type,"wifi")?"Wi-Fi":(!strcmp(type,"ethernet")?"Ethernet":type);
        char hdr[128]; snprintf(hdr,sizeof hdr,"%s  ·  %s", name, nice);
        char sub[256];
        if (*ip){ snprintf(sub,sizeof sub,"Connected · %s", ip); online=TRUE; }
        else snprintf(sub,sizeof sub,"%s", state && *state ? state : "Not connected");
        GtkWidget* right = *ssid ? label_cls(ssid,"xxri-val",1) : NULL;
        card_icon_row(card, !strcmp(type,"wifi")?"wifi":"devices", hdr, sub, right);
        g_free(type);g_free(state);g_free(ip);g_free(ssid);g_free(name);
    }
    if (!any) card_kv(card, "No network interfaces", "");
    gtk_box_pack_start(GTK_BOX(box), card, FALSE, FALSE, 0);
    /* network details (gateway / dns, shown once) */
    char* gw=jget(st,"gateway"); char* dns=jget(st,"dns");
    if (online && (*gw||*dns)) {
        gtk_box_pack_start(GTK_BOX(box), section("Network Details"), FALSE, FALSE, 0);
        GtkWidget* nd=card_new();
        if(*gw) card_kv(nd,"Gateway",gw);
        if(*dns) card_kv(nd,"DNS",dns);
        gtk_box_pack_start(GTK_BOX(box), nd, FALSE, FALSE, 0);
    }
    g_free(gw); g_free(dns);
    /* reconnect */
    GtkWidget* rc = gtk_button_new_with_label("Reconnect");
    g_signal_connect(rc, "clicked", G_CALLBACK(+[](GtkWidget* w, gpointer p){ (void)p; run_bg("xxri-network reconnect"); rebuild("wifi"); }), NULL);
    GtkWidget* rcw = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);
    gtk_widget_set_halign(rcw, GTK_ALIGN_END); gtk_box_pack_start(GTK_BOX(rcw), rc, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), rcw, FALSE, FALSE, 4);

    gtk_box_pack_start(GTK_BOX(box), section("Available Wi-Fi Networks"), FALSE, FALSE, 0);
    char* wifi = run_cmd("xxri-network scan --json");
    if (jbool(wifi,"available")==FALSE && !strstr(wifi,"\"networks\"")) {
        gtk_box_pack_start(GTK_BOX(box), hint("No wireless adapter detected.\nWi-Fi drivers ship with XXRI OS; if your adapter needs firmware, About Device shows what is missing."), FALSE, FALSE, 0);
    } else {
        GPtrArray* nets = jarr(wifi,"networks");
        if (!nets->len) gtk_box_pack_start(GTK_BOX(box), hint("No networks found. Move closer to an access point and reopen this page."), FALSE, FALSE, 0);
        else {
            GtkWidget* nc = card_new();
            for (guint i=0;i<nets->len && i<10;i++) {
                char* o=nets->pdata[i]; char* ss=jget(o,"ssid"); char* sec=jget(o,"security"); char* sig=jget(o,"signal");
                if (*ss) { char sub[96]; snprintf(sub,sizeof sub,"%s · signal %s dBm", sec, sig);
                    GtkWidget* cn=gtk_button_new_with_label("Connect"); css(cn,"xxri-chip");
                    card_icon_row(nc, "wifi", ss, sub, cn); }
                g_free(ss);g_free(sec);g_free(sig);
            }
            gtk_box_pack_start(GTK_BOX(box), nc, FALSE, FALSE, 0);
        }
        g_ptr_array_free(nets, TRUE);
    }
    g_ptr_array_free(ifs, TRUE); g_free(st); g_free(wifi);
}

/* Bluetooth */
static void build_bluetooth(GtkWidget* box) {
    char* st = run_cmd("xxri-bluetooth status --json");
    if (jbool(st,"available")) {
        gtk_box_pack_start(GTK_BOX(box), section("Adapter"), FALSE, FALSE, 0);
        GtkWidget* card = card_new();
        GtkWidget* sw = gtk_switch_new(); gtk_switch_set_active(GTK_SWITCH(sw), TRUE);
        card_action(card, "Bluetooth", "Adapter ready", sw);
        gtk_box_pack_start(GTK_BOX(box), card, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(box), section("Paired Devices"), FALSE, FALSE, 0);
        char* pd = run_cmd("xxri-bluetooth paired --json");
        GPtrArray* ds = jarr(pd,"paired");
        if (!ds->len) gtk_box_pack_start(GTK_BOX(box), hint("No paired devices yet. Use Discover to find nearby devices."), FALSE, FALSE, 0);
        else { GtkWidget* c=card_new();
            for (guint i=0;i<ds->len;i++){ char*o=ds->pdata[i]; char*nm=jget(o,"name");char*mac=jget(o,"mac");
                GtkWidget* rm=gtk_button_new_with_label("Remove"); css(rm,"destructive-action");
                card_action(c,nm,mac,rm); g_free(nm);g_free(mac);}
            gtk_box_pack_start(GTK_BOX(box), c, FALSE, FALSE, 0); }
        GtkWidget* disc=accent_btn("Discover devices");
        GtkWidget* dw=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0); gtk_widget_set_halign(dw,GTK_ALIGN_START);
        gtk_box_pack_start(GTK_BOX(dw),disc,FALSE,FALSE,0);
        gtk_box_pack_start(GTK_BOX(box), dw, FALSE, FALSE, 4);
        g_ptr_array_free(ds,TRUE); g_free(pd);
    } else {
        char* reason=jget(st,"reason"); char* h=jget(st,"hint");
        /* "Bluetooth is off" is wrong when there is no radio at all - say which
           of the two situations this is, and present the backend's lowercase
           reason as a sentence. */
        const char* title = "Bluetooth is off";
        if (strstr(reason,"adapter"))      title = "No Bluetooth adapter";
        else if (strstr(reason,"stack"))   title = "Bluetooth support not installed";
        else if (jbool(st,"rfkill_blocked")) title = "Bluetooth is blocked";
        char msg[512];
        char* r = *reason ? g_strdup(reason) : g_strdup("No Bluetooth adapter was detected.");
        if (g_ascii_islower(r[0])) r[0] = g_ascii_toupper(r[0]);
        gsize rl = strlen(r);                       /* do not double the full stop */
        const char* dot = (rl && strchr(".!?", r[rl-1])) ? "" : ".";
        snprintf(msg,sizeof msg,"%s%s\n%s", r, dot,
            *h?h:"Connect a Bluetooth adapter to pair headphones, keyboards and other devices.");
        page_empty(box, "bluetooth", title, msg);
        g_free(r); g_free(reason); g_free(h);
    }
    g_free(st);
}

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
    devgroup(box,"USB Devices",usb,"usb","product","manufacturer","devices","No USB devices connected.");
    char* inp=run_cmd("xxri-input devices --json");
    /* mouse + keyboard from input backend */
    gtk_box_pack_start(GTK_BOX(box), section("Keyboard & Mouse"), FALSE, FALSE, 0);
    GPtrArray* id=jarr(inp,"devices"); GtkWidget* ic=card_new(); gboolean anyi=FALSE;
    for (guint i=0;i<id->len;i++){char*o=id->pdata[i];char*t=jget(o,"type");char*n=jget(o,"name");
        if(!strcmp(t,"keyboard")||!strcmp(t,"mouse")||!strcmp(t,"touchpad")){anyi=TRUE;
            card_icon_row(ic,!strcmp(t,"keyboard")?"general":"devices",n,t,NULL);} g_free(t);g_free(n);}
    if(!anyi) card_kv(ic,"No input devices","");
    gtk_box_pack_start(GTK_BOX(box), ic, FALSE, FALSE, 0);
    g_ptr_array_free(id,TRUE);
    char* cam=run_cmd("xxri-hardware camera --json");
    devgroup(box,"Cameras",cam,"devices","name","device","camera","No camera detected. V4L2 webcam drivers install on demand.");
    char* snd=run_cmd("xxri-audio list --json");
    gtk_box_pack_start(GTK_BOX(box), section("Audio"), FALSE, FALSE, 0);
    GPtrArray* sc=jarr(snd,"cards"); GtkWidget* ac=card_new();
    if(!sc->len) card_kv(ac,"No sound card","");
    for(guint i=0;i<sc->len;i++){char*o=sc->pdata[i];char*n=jget(o,"name");char*ty=jget(o,"type");
        card_icon_row(ac,"sound",n,ty,NULL);g_free(n);g_free(ty);}
    gtk_box_pack_start(GTK_BOX(box), ac, FALSE, FALSE, 0);
    g_ptr_array_free(sc,TRUE);
    char* dsp=run_cmd("xxri-display current --json");
    gtk_box_pack_start(GTK_BOX(box), section("Displays"), FALSE, FALSE, 0);
    GtkWidget* dc=card_new(); char*mode=jget(dsp,"mode");char*drv=jget(dsp,"driver");
    char dsub[96]; snprintf(dsub,sizeof dsub,"driver %s · 60Hz", drv);
    card_icon_row(dc,"devices",mode,dsub,NULL);
    gtk_box_pack_start(GTK_BOX(box), dc, FALSE, FALSE, 0);
    char* pr=run_cmd("xxri-hardware printer --json");
    devgroup(box,"Printers",pr,"devices","product","device","devices","No printer detected.");
    g_free(usb);g_free(inp);g_free(cam);g_free(snd);g_free(dsp);g_free(pr);g_free(mode);g_free(drv);
}

/* Wallpaper & Style */
static void cb_scale(GtkWidget* w, gpointer d){ (void)w; run_bg("xxri-desktop wallpaper scale %s",(char*)d); }
static void cb_dockpos(GtkWidget* w, gpointer d){ (void)w; run_bg("xxri-desktop dock position %s",(char*)d); }
static void cb_appear(GtkWidget* w, gpointer d){ (void)w; run_bg("xxri-desktop appearance set %s",(char*)d); }
static void cb_docksize(GtkRange* r, gpointer d){ (void)d; run_bg("xxri-desktop dock size %d",(int)gtk_range_get_value(r)); }
static GtkWidget* segbtn(const char* label, GCallback cb, gpointer d, GtkWidget* group) {
    GtkWidget* b = group ? gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(group), label)
                         : gtk_radio_button_new_with_label(NULL, label);
    gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(b), FALSE);
    if (cb) g_signal_connect(b, "clicked", cb, d);
    return b;
}
static void build_wallpaper(GtkWidget* box) {
    char* st=run_cmd("xxri-desktop status --json");
    gtk_box_pack_start(GTK_BOX(box), section("Wallpaper"), FALSE, FALSE, 0);
    GtkWidget* wc=card_new();
    GtkWidget* r=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,14); css(r,"xxri-rowitem");
    GdkPixbuf* pb=load_png("/usr/local/share/xxri-settings/wallpaper-preview.png",132,80);
    if(pb){ GtkWidget* pv=gtk_image_new_from_pixbuf(pb); css(pv,"xxri-appicon"); g_object_unref(pb);
        gtk_widget_set_valign(pv,GTK_ALIGN_CENTER); gtk_box_pack_start(GTK_BOX(r),pv,FALSE,FALSE,0);}
    GtkWidget* tv=gtk_box_new(GTK_ORIENTATION_VERTICAL,2); gtk_widget_set_valign(tv,GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(tv),label_cls("XXRI OS Lite","xxri-row-title",0),FALSE,FALSE,0);
    char* wsc=jget(st,"wallpaper_scale"); char scl[64]; snprintf(scl,sizeof scl,"Official wallpaper · scaling %s",wsc);
    gtk_box_pack_start(GTK_BOX(tv),label_cls(scl,"xxri-row-sub",0),FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(r),tv,TRUE,TRUE,0);
    gtk_box_pack_start(GTK_BOX(wc),r,FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(box),wc,FALSE,FALSE,0);

    gtk_box_pack_start(GTK_BOX(box), section("Scaling"), FALSE, FALSE, 0);
    GtkWidget* sc=card_new(); GtkWidget* sr=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,8); css(sr,"xxri-rowitem");
    const char* scales[]={"fill","full","center","tile"}; const char* scapn[]={"Fill","Full","Center","Tile"};
    GtkWidget* grp=NULL;
    for(int i=0;i<4;i++){ GtkWidget* b=segbtn(scapn[i],G_CALLBACK(cb_scale),(gpointer)scales[i],grp); if(!grp)grp=b;
        gtk_box_pack_start(GTK_BOX(sr),b,FALSE,FALSE,0);}
    gtk_box_pack_start(GTK_BOX(sc),sr,FALSE,FALSE,0); gtk_box_pack_start(GTK_BOX(box),sc,FALSE,FALSE,0);

    gtk_box_pack_start(GTK_BOX(box), section("Dock"), FALSE, FALSE, 0);
    GtkWidget* dc=card_new();
    GtkWidget* posw=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,8);
    GtkWidget* pt=segbtn("Top",G_CALLBACK(cb_dockpos),(gpointer)"top",NULL);
    GtkWidget* pbt=segbtn("Bottom",G_CALLBACK(cb_dockpos),(gpointer)"bottom",pt);
    char* dpos=jget(st,"dock_position"); if(!strcmp(dpos,"bottom")) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pbt),TRUE);
    gtk_box_pack_start(GTK_BOX(posw),pt,FALSE,FALSE,0); gtk_box_pack_start(GTK_BOX(posw),pbt,FALSE,FALSE,0);
    card_action(dc,"Position","Where the dock sits",posw);
    GtkWidget* sl=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,24,56,2);
    gtk_scale_set_draw_value(GTK_SCALE(sl),FALSE); gtk_widget_set_size_request(sl,220,-1);
    int ds=atoi(jget(st,"dock_size")); gtk_range_set_value(GTK_RANGE(sl),ds?ds:40);
    g_signal_connect(sl,"value-changed",G_CALLBACK(cb_docksize),NULL);
    card_action(dc,"Icon size",NULL,sl);
    gtk_box_pack_start(GTK_BOX(box),dc,FALSE,FALSE,0);

    gtk_box_pack_start(GTK_BOX(box), section("Appearance"), FALSE, FALSE, 0);
    GtkWidget* ac=card_new(); GtkWidget* ar=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,8); css(ar,"xxri-rowitem");
    GtkWidget* li=segbtn("Light",G_CALLBACK(cb_appear),(gpointer)"light",NULL);
    GtkWidget* da=segbtn("Dark",G_CALLBACK(cb_appear),(gpointer)"dark",li);
    gtk_box_pack_start(GTK_BOX(ar),li,FALSE,FALSE,0); gtk_box_pack_start(GTK_BOX(ar),da,FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(ar),label_cls("  Dark theme is stored for a future update.","xxri-row-sub",0),FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(ac),ar,FALSE,FALSE,0); gtk_box_pack_start(GTK_BOX(box),ac,FALSE,FALSE,0);

    gtk_box_pack_start(GTK_BOX(box), section("Theme"), FALSE, FALSE, 0);
    GtkWidget* tc=card_new(); card_kv(tc,"Icon theme","xxri"); card_kv(tc,"Cursor theme","xxri-cursors");
    card_kv(tc,"Accent color","XXRI gradient (pink → purple → blue)");
    gtk_box_pack_start(GTK_BOX(box),tc,FALSE,FALSE,0);
    g_free(st);
}

/* Sounds & Vibration */
static void cb_vol(GtkRange* r, gpointer d){ (void)d; run_bg("xxri-audio volume set %d",(int)gtk_range_get_value(r)); }
static void cb_mic(GtkRange* r, gpointer d){ (void)d; run_bg("xxri-audio mic set %d",(int)gtk_range_get_value(r)); }
static void cb_mute(GtkWidget* w, gpointer d){ (void)w;(void)d; run_bg("xxri-audio toggle"); rebuild("sound"); }
static void cb_test(GtkWidget* w, gpointer d){ (void)w;(void)d; run_bg("xxri-audio test"); }
static void build_sound(GtkWidget* box) {
    char* cards=run_cmd("xxri-audio list --json");
    if (jbool(cards,"alsa")==FALSE && !strstr(cards,"\"cards\"")) {
        page_empty(box, "sound", "No sound card detected",
                   "Audio drivers load automatically at boot. About Device shows any firmware that is still missing.");
        g_free(cards); return;
    }
    GPtrArray* cs=jarr(cards,"cards");
    gtk_box_pack_start(GTK_BOX(box), section("Output Device"), FALSE, FALSE, 0);
    GtkWidget* oc=card_new();
    for(guint i=0;i<cs->len;i++){char*o=cs->pdata[i]; if(!jbool(o,"playback"))continue;char*n=jget(o,"name");char*ty=jget(o,"type");
        char sub[96];snprintf(sub,sizeof sub,"type %s%s",ty,jbool(o,"default")?" · default":"");
        card_icon_row(oc,"sound",n,sub,NULL);g_free(n);g_free(ty);}
    gtk_box_pack_start(GTK_BOX(box),oc,FALSE,FALSE,0);
    char* vol=run_cmd("xxri-audio volume get --json");
    gtk_box_pack_start(GTK_BOX(box), section("Volume"), FALSE, FALSE, 0);
    GtkWidget* vc=card_new(); GtkWidget* vr=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,10); css(vr,"xxri-rowitem");
    GtkWidget* vs=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0,100,1); gtk_scale_set_draw_value(GTK_SCALE(vs),FALSE);
    gtk_range_set_value(GTK_RANGE(vs),atoi(jget(vol,"volume"))); g_signal_connect(vs,"value-changed",G_CALLBACK(cb_vol),NULL);
    gtk_box_pack_start(GTK_BOX(vr),vs,TRUE,TRUE,0);
    GtkWidget* mb=gtk_button_new_with_label(jbool(vol,"muted")?"Unmute":"Mute"); css(mb,"xxri-chip"); g_signal_connect(mb,"clicked",G_CALLBACK(cb_mute),NULL);
    GtkWidget* tb=gtk_button_new_with_label("Test"); css(tb,"xxri-chip"); g_signal_connect(tb,"clicked",G_CALLBACK(cb_test),NULL);
    gtk_box_pack_start(GTK_BOX(vr),mb,FALSE,FALSE,0); gtk_box_pack_start(GTK_BOX(vr),tb,FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(vc),vr,FALSE,FALSE,0); gtk_box_pack_start(GTK_BOX(box),vc,FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(box), section("Input Device"), FALSE, FALSE, 0);
    GtkWidget* icd=card_new(); gboolean cap=FALSE;
    for(guint i=0;i<cs->len;i++){char*o=cs->pdata[i]; if(!jbool(o,"capture"))continue;cap=TRUE;char*n=jget(o,"name");
        card_icon_row(icd,"sound",n,"microphone / capture",NULL);g_free(n);}
    if(!cap) card_kv(icd,"No microphone detected","");
    gtk_box_pack_start(GTK_BOX(box),icd,FALSE,FALSE,0);
    if(cap){ char* mic=run_cmd("xxri-audio mic --json");
        gtk_box_pack_start(GTK_BOX(box), section("Microphone Level"), FALSE, FALSE, 0);
        GtkWidget* mc=card_new(); GtkWidget* mr=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0); css(mr,"xxri-rowitem");
        GtkWidget* ms=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0,100,1); gtk_scale_set_draw_value(GTK_SCALE(ms),FALSE);
        gtk_range_set_value(GTK_RANGE(ms),atoi(jget(mic,"level"))); g_signal_connect(ms,"value-changed",G_CALLBACK(cb_mic),NULL);
        gtk_box_pack_start(GTK_BOX(mr),ms,TRUE,TRUE,0); gtk_box_pack_start(GTK_BOX(mc),mr,FALSE,FALSE,0);
        gtk_box_pack_start(GTK_BOX(box),mc,FALSE,FALSE,0); g_free(mic);}
    gtk_box_pack_start(GTK_BOX(box), section("System Sounds"), FALSE, FALSE, 0);
    GtkWidget* nc=card_new();
    GtkWidget* n1=gtk_switch_new(); gtk_switch_set_active(GTK_SWITCH(n1),TRUE); card_action(nc,"Notification sounds",NULL,n1);
    GtkWidget* n2=gtk_switch_new(); card_action(nc,"Startup sound","Play the XXRI chime at login",n2);
    gtk_box_pack_start(GTK_BOX(box),nc,FALSE,FALSE,0);
    g_ptr_array_free(cs,TRUE); g_free(cards); g_free(vol);
}

/* Storage */
static void cb_mount(GtkWidget* w, gpointer d){ (void)w; run_bg("xxri-storage mount %s",(char*)d); rebuild("storage"); }
static void cb_umount(GtkWidget* w, gpointer d){ (void)w; run_bg("xxri-storage unmount %s",(char*)d); rebuild("storage"); }
static void build_storage(GtkWidget* box) {
    char* js=run_cmd("xxri-storage list --json");
    GPtrArray* disks=jarr(js,"disks");
    if (!disks->len) { page_empty(box, "storage", "No storage devices",
        "Connect a USB drive or SD card and it will appear here, ready to mount."); }
    for (guint i=0;i<disks->len;i++) {
        char* d=disks->pdata[i]; char* dev=jget(d,"device"); char* model=jget(d,"model"); gboolean usb=jbool(d,"usb");
        char sec[160]; snprintf(sec,sizeof sec,"%s  ·  %s%s", model, dev, usb?"  ·  removable":"");
        gtk_box_pack_start(GTK_BOX(box), section(sec), FALSE, FALSE, 0);
        GtkWidget* c=card_new();
        GPtrArray* parts=jarr(d,"partitions");
        if (!parts->len) { char whole[64]; snprintf(whole,sizeof whole,"Whole disk · %d MB",atoi(jget(d,"size_mb"))); card_kv(c,whole,"no partitions / not formatted"); }
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
                GtkWidget* e=gtk_button_new_with_label(usb?"Eject":"Unmount"); css(e,"xxri-chip");
                g_signal_connect_data(e,"clicked",G_CALLBACK(cb_umount),g_strdup(pdev),(GClosureNotify)g_free,0);
                gtk_widget_set_valign(e,GTK_ALIGN_CENTER); gtk_box_pack_end(GTK_BOX(top),e,FALSE,FALSE,0);
                gtk_box_pack_start(GTK_BOX(r),top,FALSE,FALSE,0);
                /* usage bar */
                double total=(double)(uk+fk); double frac=total>0?(double)uk/total:0;
                GtkWidget* lv=gtk_level_bar_new_for_interval(0,1); gtk_level_bar_set_value(GTK_LEVEL_BAR(lv),frac);
                gtk_box_pack_start(GTK_BOX(r),lv,FALSE,FALSE,2);
                char use[96]; snprintf(use,sizeof use,"%ld MB used · %ld MB free · mounted at %s", uk/1024, fk/1024, mp);
                gtk_box_pack_start(GTK_BOX(r),label_cls(use,"xxri-row-sub",0),FALSE,FALSE,0);
            } else {
                GtkWidget* m=gtk_button_new_with_label("Mount"); css(m,"xxri-chip");
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
        if (jbool(hlt,"available")) { char* stt=jget(hlt,"status"); char hs[64]; snprintf(hs,sizeof hs,"SMART health: %s",stt); card_kv(c,"Health",hs); g_free(stt); }
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
    gtk_box_pack_start(GTK_BOX(box), section("Power"), FALSE, FALSE, 0);
    GtkWidget* pc=card_new();
    if (jbool(js,"present")) {
        GPtrArray* bs=jarr(js,"batteries");
        for(guint i=0;i<bs->len;i++){char*o=bs->pdata[i];char*cap=jget(o,"capacity");char*stt=jget(o,"status");
            char t[64];snprintf(t,sizeof t,"Battery %s%%",cap); card_kv(pc,t,stt);g_free(cap);g_free(stt);}
        g_ptr_array_free(bs,TRUE);
    } else card_kv(pc,"No battery detected","This is normal on a desktop system.");
    card_kv(pc,"AC adapter", jbool(js,"online")?"online":(jbool(js,"present")?"offline":"powered (desktop)"));
    gtk_box_pack_start(GTK_BOX(box),pc,FALSE,FALSE,0);

    gtk_box_pack_start(GTK_BOX(box), section("Power Profile"), FALSE, FALSE, 0);
    char* gv=run_cmd("xxri-power governor list --json");
    if (jbool(gv,"available")==FALSE && !strstr(gv,"\"current\"")) gtk_box_pack_start(GTK_BOX(box), hint("CPU frequency scaling is not available on this processor."), FALSE, FALSE, 0);
    else { GtkWidget* gc=card_new(); GtkWidget* r=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,8); css(r,"xxri-rowitem");
        char* cur=jget(gv,"current");
        GtkWidget* tv=gtk_box_new(GTK_ORIENTATION_VERTICAL,2);
        gtk_box_pack_start(GTK_BOX(tv),label_cls("CPU governor","xxri-row-title",0),FALSE,FALSE,0);
        gtk_box_pack_start(GTK_BOX(tv),label_cls(cur,"xxri-row-sub",0),FALSE,FALSE,0);
        gtk_box_pack_start(GTK_BOX(r),tv,TRUE,TRUE,0);
        const char* govs[]={"performance","powersave","ondemand","conservative","schedutil"};
        for(int gi=0;gi<5;gi++){ if(!strstr(gv,govs[gi]))continue;
            GtkWidget* b=gtk_button_new_with_label(govs[gi]); css(b,"xxri-chip");
            g_signal_connect(b,"clicked",G_CALLBACK(cb_gov),(gpointer)govs[gi]); gtk_box_pack_start(GTK_BOX(r),b,FALSE,FALSE,0);}
        gtk_box_pack_start(GTK_BOX(gc),r,FALSE,FALSE,0); gtk_box_pack_start(GTK_BOX(box),gc,FALSE,FALSE,0); g_free(cur);}

    gtk_box_pack_start(GTK_BOX(box), section("Display Brightness"), FALSE, FALSE, 0);
    char* br=run_cmd("xxri-power brightness get --json");
    if (jbool(br,"available")==FALSE) gtk_box_pack_start(GTK_BOX(box), hint("Brightness control is not available on this display."), FALSE, FALSE, 0);
    else { GtkWidget* bc=card_new(); GtkWidget* r=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0); css(r,"xxri-rowitem");
        GtkWidget* s=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0,100,1); gtk_scale_set_draw_value(GTK_SCALE(s),FALSE);
        gtk_range_set_value(GTK_RANGE(s),atoi(jget(br,"percent"))); g_signal_connect(s,"value-changed",G_CALLBACK(cb_bri),NULL);
        gtk_box_pack_start(GTK_BOX(r),s,TRUE,TRUE,0); gtk_box_pack_start(GTK_BOX(bc),r,FALSE,FALSE,0);
        gtk_box_pack_start(GTK_BOX(box),bc,FALSE,FALSE,0);}

    gtk_box_pack_start(GTK_BOX(box), section("Suspend"), FALSE, FALSE, 0);
    char* caps=run_cmd("xxri-power capabilities --json");
    GtkWidget* sc=card_new();
    card_kv(sc,"Suspend to RAM", jbool(caps,"suspend")?"supported":"not supported");
    card_kv(sc,"Hibernate", jbool(caps,"hibernate")?"supported":"not available (no swap)");
    gtk_box_pack_start(GTK_BOX(box),sc,FALSE,FALSE,0);
    g_free(js);g_free(gv);g_free(br);g_free(caps);
}

/* Apps */
static void cb_launch(GtkWidget* w, gpointer d){ (void)w; run_bg("xxri-app launch %s",(char*)d); }
static void cb_remove(GtkWidget* w, gpointer d){ (void)w; run_bg("xxri-app remove-ui %s",(char*)d); }
static void build_apps(GtkWidget* box) {
    char* out=run_cmd("xxri-app list");
    if (!*out) { page_empty(box, "apps", "No apps installed yet",
        "Install apps from the XXRI Store, or open an AppImage you downloaded and it is integrated here.");
        g_free(out); return; }
    gtk_box_pack_start(GTK_BOX(box), section("Installed Applications"), FALSE, FALSE, 0);
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
        char sub[160]; snprintf(sub,sizeof sub,"version %s%s%s", f[2], nf>4?" · last launch: ":"", nf>4?f[4]:"");
        gtk_box_pack_start(GTK_BOX(tv),label_cls(sub,"xxri-row-sub",0),FALSE,FALSE,0);
        gtk_box_pack_start(GTK_BOX(r),tv,TRUE,TRUE,0);
        GtkWidget* l=accent_btn("Launch"); g_signal_connect_data(l,"clicked",G_CALLBACK(cb_launch),g_strdup(f[0]),(GClosureNotify)g_free,0);
        GtkWidget* rm=gtk_button_new_with_label("Remove"); css(rm,"xxri-chip"); g_signal_connect_data(rm,"clicked",G_CALLBACK(cb_remove),g_strdup(f[0]),(GClosureNotify)g_free,0);
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
    GtkWidget* dlg=gtk_message_dialog_new(GTK_WINDOW(g_win),GTK_DIALOG_MODAL,GTK_MESSAGE_QUESTION,GTK_BUTTONS_OK_CANCEL,"Restart the desktop session? All open apps will close.");
    if(gtk_dialog_run(GTK_DIALOG(dlg))==GTK_RESPONSE_OK) run_bg("xxri-desktop restart-session");
    gtk_widget_destroy(dlg);
}
/* Help & Support.
 * Before Phase 10 this nav entry fired xxri-open-url and left the content on
 * whatever page was showing - on a device with no browser installed that was
 * a dead click.  It is a real page now: what this build is, where to get help,
 * and the desktop's own keyboard shortcuts (which no browser can provide). */
static void cb_open_url(GtkWidget* w, gpointer u) { (void)w; run_bg("xxri-open-url %s", (const char*)u); }
static void build_help(GtkWidget* box) {
    char* s = run_cmd("xxri-hardware summary --json");
    char* os = jget(s, "os");

    gtk_box_pack_start(GTK_BOX(box), section("This device"), FALSE, FALSE, 0);
    GtkWidget* c0 = card_new();
    card_kv(c0, "Edition", *os ? os : "XXRI OS Lite");
    card_kv(c0, "Architecture", "i686 (32-bit Intel)");
    card_kv(c0, "Software source", "repo.xxri.flows.best");
    gtk_box_pack_start(GTK_BOX(box), c0, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(box), section("Get help"), FALSE, FALSE, 0);
    GtkWidget* c1 = card_new();
    GtkWidget* b1 = gtk_button_new_with_label("Open");
    g_signal_connect(b1, "clicked", G_CALLBACK(cb_open_url), (gpointer)"https://xxri.flows.best");
    card_icon_row(c1, "help", "XXRI website", "Guides, downloads and release notes", b1);
    GtkWidget* b2 = gtk_button_new_with_label("Open");
    g_signal_connect(b2, "clicked", G_CALLBACK(cb_open_url), (gpointer)"https://xxri.flows.best/support/");
    card_icon_row(c1, "help", "Support", "Report a problem or ask a question", b2);
    gtk_box_pack_start(GTK_BOX(box), c1, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box),
        hint("Links open in your browser. XXRI OS Lite ships without one - install a browser "
             "from the Store first, or read the guides on another device."), FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(box), section("Keyboard shortcuts"), FALSE, FALSE, 0);
    GtkWidget* c2 = card_new();
    struct { const char* k; const char* v; } keys[] = {
        {"Alt + Tab",                "Switch between open windows"},
        {"Alt + F1",                 "Minimise the active window"},
        {"Ctrl + Alt + M",           "Maximise or restore the active window"},
        {"Ctrl + Alt + C",           "Open or close the Control Center"},
        {"Ctrl + Alt + arrow keys",  "Move the active window"},
        {"Ctrl + Alt + = / -",       "Grow or shrink the active window"},
        {"Click the desktop",        "Window list, desktops and Exit"},
        {"Shift + Page Up / Down",   "Scroll back through the Terminal"},
    };
    for (unsigned i = 0; i < G_N_ELEMENTS(keys); i++) card_kv(c2, keys[i].k, keys[i].v);
    gtk_box_pack_start(GTK_BOX(box), c2, FALSE, FALSE, 0);

    g_free(os); g_free(s);
}
static void build_general(GtkWidget* box) {
    char* st=run_cmd("xxri-desktop status --json");
    char* dt=run_cmd("xxri-desktop datetime --json");
    char* kb=run_cmd("xxri-input status --json");
    gtk_box_pack_start(GTK_BOX(box), section("Language & Region"), FALSE, FALSE, 0);
    GtkWidget* lc=card_new(); card_kv(lc,"Language",jget(st,"language"));
    char* rg=jget(st,"region"); card_kv(lc,"Region",*rg?rg:"not set"); card_kv(lc,"Keyboard layout",jget(kb,"keyboard_layout"));
    gtk_box_pack_start(GTK_BOX(box),lc,FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(box), section("Date & Time"), FALSE, FALSE, 0);
    GtkWidget* tc=card_new(); char dtl[96]; snprintf(dtl,sizeof dtl,"%s  %s",jget(dt,"date"),jget(dt,"time"));
    GtkWidget* sy=gtk_button_new_with_label("Sync time"); css(sy,"xxri-chip"); g_signal_connect(sy,"clicked",G_CALLBACK(cb_ntp),NULL);
    char* tz=jget(dt,"timezone"); char tzl[64]; snprintf(tzl,sizeof tzl,"timezone %s",tz);
    card_action(tc,dtl,tzl,sy); gtk_box_pack_start(GTK_BOX(box),tc,FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(box), section("Account"), FALSE, FALSE, 0);
    GtkWidget* ac=card_new();
    GtkWidget* hr=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,10); css(hr,"xxri-rowitem");
    GtkWidget* htv=gtk_box_new(GTK_ORIENTATION_VERTICAL,4);
    gtk_box_pack_start(GTK_BOX(htv),label_cls("Hostname","xxri-row-title",0),FALSE,FALSE,0);
    g_host_entry=gtk_entry_new(); gtk_entry_set_text(GTK_ENTRY(g_host_entry),jget(st,"hostname")); gtk_widget_set_size_request(g_host_entry,220,-1);
    gtk_box_pack_start(GTK_BOX(htv),g_host_entry,FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(hr),htv,TRUE,TRUE,0);
    GtkWidget* ap=gtk_button_new_with_label("Apply"); css(ap,"xxri-chip"); gtk_widget_set_valign(ap,GTK_ALIGN_END); g_signal_connect(ap,"clicked",G_CALLBACK(cb_host),NULL);
    gtk_box_pack_end(GTK_BOX(hr),ap,FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(ac),hr,FALSE,FALSE,0);
    GtkWidget* al=gtk_switch_new(); gtk_switch_set_active(GTK_SWITCH(al), !strcmp(jget(st,"autologin"),"on"));
    g_signal_connect(al,"notify::active",G_CALLBACK(cb_auto),NULL);
    card_action(ac,"Log in automatically","Skip the login prompt at boot",al);
    gtk_box_pack_start(GTK_BOX(box),ac,FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(box), section("Startup Applications"), FALSE, FALSE, 0);
    GtkWidget* su=card_new(); card_kv(su,"Storage automount","Enabled (xxri-storage watch)");
    card_kv(su,"Hardware layer","Enabled (xxri-hw-init)"); gtk_box_pack_start(GTK_BOX(box),su,FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(box), section("Desktop"), FALSE, FALSE, 0);
    GtkWidget* dc=card_new(); GtkWidget* dr=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,8); css(dr,"xxri-rowitem");
    GtkWidget* rs=gtk_button_new_with_label("Reset settings"); g_signal_connect(rs,"clicked",G_CALLBACK(cb_reset),NULL);
    GtkWidget* rst=accent_btn("Restart session"); g_signal_connect(rst,"clicked",G_CALLBACK(cb_restart),NULL);
    gtk_box_pack_start(GTK_BOX(dr),rs,FALSE,FALSE,0); gtk_box_pack_start(GTK_BOX(dr),rst,FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(dc),dr,FALSE,FALSE,0); gtk_box_pack_start(GTK_BOX(box),dc,FALSE,FALSE,0);
    g_free(st);g_free(dt);g_free(kb);
}

/* Software Update */
static GtkWidget* g_updlog;
static void cb_upd(GtkWidget* w, gpointer d){ (void)w; char* r=run_cmd("xxri-updates %s",(char*)d); gtk_label_set_text(GTK_LABEL(g_updlog), r); g_free(r); }
static void build_update(GtkWidget* box) {
    char* st=run_cmd("xxri-updates status --json");
    gtk_box_pack_start(GTK_BOX(box), section("Packages"), FALSE, FALSE, 0);
    GtkWidget* pc=card_new();
    char pk[64]; snprintf(pk,sizeof pk,"%s packages installed",jget(st,"installed")); card_kv(pc,pk,jget(st,"mirror"));
    gtk_box_pack_start(GTK_BOX(box),pc,FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(box), section("Updates"), FALSE, FALSE, 0);
    GtkWidget* bc=card_new(); GtkWidget* br=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,8); css(br,"xxri-rowitem");
    GtkWidget* ck=gtk_button_new_with_label("Check updates"); g_signal_connect(ck,"clicked",G_CALLBACK(cb_upd),(gpointer)"check");
    GtkWidget* rf=gtk_button_new_with_label("Refresh mirror"); g_signal_connect(rf,"clicked",G_CALLBACK(cb_upd),(gpointer)"refresh");
    GtkWidget* up=accent_btn("Update now"); g_signal_connect(up,"clicked",G_CALLBACK(cb_upd),(gpointer)"apply");
    gtk_box_pack_start(GTK_BOX(br),ck,FALSE,FALSE,0); gtk_box_pack_start(GTK_BOX(br),rf,FALSE,FALSE,0);
    gtk_box_pack_end(GTK_BOX(br),up,FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(bc),br,FALSE,FALSE,0); gtk_box_pack_start(GTK_BOX(box),bc,FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(box), section("Update History"), FALSE, FALSE, 0);
    GtkWidget* lc=card_new(); GtkWidget* lr=gtk_box_new(GTK_ORIENTATION_VERTICAL,0); css(lr,"xxri-rowitem");
    g_updlog=label_cls("Use Check updates to compare installed packages against the mirror.\nUpdate now downloads newer packages; they activate on the next boot.","xxri-val",0);
    gtk_box_pack_start(GTK_BOX(lr),g_updlog,FALSE,FALSE,0); gtk_box_pack_start(GTK_BOX(lc),lr,FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(box),lc,FALSE,FALSE,0);
    g_free(st);
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
        {"OS", jget(s,"os")},
        {"Version", g_strdup(strstr(jget(s,"os"),"Lite")?"2.0 Lite":"2.0")},
        {"Uptime", jget(s,"uptime")},
        {"Locale", ({ char* lc = jget(s,"locale");
                      /* a bare "C" is the POSIX default, not a language */
                      char* out = (!strcmp(lc,"C")||!strcmp(lc,"POSIX"))
                                ? g_strdup_printf("%s \xc2\xb7 POSIX default", lc)
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
    gtk_box_pack_start(GTK_BOX(box), section("Hardware"), FALSE, FALSE, 0);
    GtkWidget* hc=card_new();
    card_kv(hc,"CPU",jget(s,"cpu"));
    card_kv(hc,"CPU Cores",jget(s,"cores"));
    card_kv(hc,"GPU",jget(s,"gpu"));
    char mem[32]; snprintf(mem,sizeof mem,"%s MiB",jget(s,"memory_mb")); card_kv(hc,"Memory",mem);
    GPtrArray* disks=jarr(disk,"disks"); char dtxt[128]="unknown";
    for(guint i=0;i<disks->len;i++){char*o=disks->pdata[i]; if(!jbool(o,"usb")){snprintf(dtxt,sizeof dtxt,"%s · %ld GiB",jget(o,"model"),atol(jget(o,"size_mb"))/1024);break;}}
    card_kv(hc,"Storage",dtxt);
    card_kv(hc,"Battery", jbool(bat,"present")?"present":"N/A (Desktop)");
    card_kv(hc,"Display Resolution",jget(dsp,"mode"));
    gtk_box_pack_start(GTK_BOX(box),hc,FALSE,FALSE,0);
    /* System */
    gtk_box_pack_start(GTK_BOX(box), section("System"), FALSE, FALSE, 0);
    GtkWidget* yc=card_new();
    card_kv(yc,"Kernel",jget(s,"kernel"));
    card_kv(yc,"Architecture",jget(s,"arch"));
    char bios[160]; snprintf(bios,sizeof bios,"%s %s",jget(b,"vendor"),jget(b,"version")); card_kv(yc,"BIOS",bios);
    card_kv(yc,"Build","XXRI OS Lite 2.0 (build 8)");
    gtk_box_pack_start(GTK_BOX(box),yc,FALSE,FALSE,0);
    /* Legal */
    gtk_box_pack_start(GTK_BOX(box), section("Legal"), FALSE, FALSE, 0);
    GtkWidget* lc=card_new();
    card_kv(lc,"License","GNU General Public License by Free Software Foundation v3 or later");
    card_kv(lc,"Copyright","Copyright (C) 2025 Jantzen Maximillian kimlim");
    gtk_box_pack_start(GTK_BOX(box),lc,FALSE,FALSE,0);
    g_ptr_array_free(disks,TRUE);
    g_free(s);g_free(b);g_free(disk);g_free(dsp);g_free(bat);
}

/* ---------------------------------------------------- page framework ----- */
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
    const char* label = "Settings";
    for (int i=0;i<NNAV;i++) if(!strcmp(NAV[i].id,id)) label=NAV[i].label;
    GtkWidget* tl=label_cls(label,"xxri-page-title",0);
    gtk_widget_set_margin_bottom(tl,8);
    gtk_box_pack_start(GTK_BOX(col),tl,FALSE,FALSE,0);
    /* body */
    if      (!strcmp(id,"wifi"))      build_wifi(col);
    else if (!strcmp(id,"bluetooth")) build_bluetooth(col);
    else if (!strcmp(id,"devices"))   build_devices(col);
    else if (!strcmp(id,"wallpaper")) build_wallpaper(col);
    else if (!strcmp(id,"sound"))     build_sound(col);
    else if (!strcmp(id,"help"))      build_help(col);
    else if (!strcmp(id,"storage"))   build_storage(col);
    else if (!strcmp(id,"battery"))   build_battery(col);
    else if (!strcmp(id,"apps"))      build_apps(col);
    else if (!strcmp(id,"general"))   build_general(col);
    else if (!strcmp(id,"update"))    build_update(col);
    else if (!strcmp(id,"about"))     build_about(col);
    /* center the column horizontally, capped width */
    GtkWidget* center = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);
    gtk_widget_set_halign(center, GTK_ALIGN_CENTER);
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
static void show_page(const char* id) {
    GtkWidget* c = gtk_stack_get_child_by_name(GTK_STACK(g_stack), id);
    if (!c) rebuild(id);
    else { rebuild(id); }
}

static void on_nav(GtkListBox* lb, GtkListBoxRow* row, gpointer u) {
    (void)lb;(void)u;
    if (!row) return;
    const char* id = (const char*)g_object_get_data(G_OBJECT(row),"id");
    show_page(id);
}

/* ------------------------------------------------------------- main ------ */
static void activate(GtkApplication* app, gpointer u) {
    (void)u;
    /* CSS */
    GtkCssProvider* prov = gtk_css_provider_new();
    gtk_css_provider_load_from_path(prov, "/usr/local/share/xxri-settings/xxri.css", NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(prov), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION+1);

    g_win = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(g_win), "XXRI Settings");
    gtk_window_set_default_size(GTK_WINDOW(g_win), 980, 660);
    css(g_win, "xxri-root");

    GtkWidget* hb = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_container_add(GTK_CONTAINER(g_win), hb);

    /* ---- sidebar ---- */
    GtkWidget* side = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    css(side, "xxri-sidebar");
    gtk_widget_set_size_request(side, 250, -1);
    /* header */
    GtkWidget* hdr = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8); css(hdr,"xxri-sidebar-header");
    GtkWidget* stitle = label_cls("Settings","xxri-sidebar-title",0);
    gtk_box_pack_start(GTK_BOX(hdr), stitle, TRUE, TRUE, 0);
    GtkWidget* srch = img("wifi",22); /* placeholder search glyph unused */
    (void)srch;
    gtk_box_pack_start(GTK_BOX(side), hdr, FALSE, FALSE, 0);
    /* user chip */
    GtkWidget* chip = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,10); css(chip,"xxri-userchip");
    GtkWidget* cv=gtk_box_new(GTK_ORIENTATION_VERTICAL,1);
    gtk_box_pack_start(GTK_BOX(cv),label_cls("Guest","xxri-userchip-name",0),FALSE,FALSE,0);
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
        gtk_box_pack_start(GTK_BOX(rb), label_cls(NAV[i].label,NULL,0), TRUE, TRUE, 0);
        gtk_container_add(GTK_CONTAINER(row),rb);
        gtk_list_box_insert(GTK_LIST_BOX(nav),row,-1);
    }
    g_signal_connect(nav,"row-selected",G_CALLBACK(on_nav),NULL);
    gtk_container_add(GTK_CONTAINER(navscr),nav);
    gtk_box_pack_start(GTK_BOX(side), navscr, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hb), side, FALSE, FALSE, 0);

    /* ---- content stack ---- */
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
    if (argc > 1 && argv[1][0] != '-') { g_start_id = argv[1]; argc = 1; }
    GtkApplication* app = gtk_application_new("best.flows.xxri.settings", G_APPLICATION_NON_UNIQUE);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    int s = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return s;
}
