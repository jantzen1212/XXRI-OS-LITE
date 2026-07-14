// xxri-settings - the xxri OS Lite Settings application (Phase 8).
//
// Replaces the Tiny Core control panel.  Pure frontend: every hardware or
// system action goes through an xxri-* backend (Phase 6 AppImage registry,
// Phase 7 hardware layer, plus xxri-desktop / xxri-updates).  Built on the
// shared xxri-ui.h framework, styled with the Phase 4 design language.
//
// Layout follows the supplied mockup: lavender sidebar (user card + 12
// icon nav rows with dashed group dividers) and a white One-UI content
// panel of rounded section cards.

#include "xxri-ui.h"
#include <FL/Fl_Scroll.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_PNG_Image.H>
#include <FL/Fl_Shared_Image.H>
#include <unistd.h>
using namespace xxri;

static const int WINW = 940, WINH = 640, NAVW = 232;
static const char* ICON = "/usr/local/share/xxri-settings/icons";

// ------------------------------------------------------------ nav model --
struct Nav { const char* id; const char* label; const char* icon; int group; };
static const Nav NAV[] = {
    {"wifi",      "Wi-Fi & Network",    "wifi",      0},
    {"bluetooth", "Bluetooth",          "bluetooth", 0},
    {"devices",   "Connected Devices",  "devices",   0},
    {"wallpaper", "Wallpaper & Style",  "wallpaper", 1},
    {"sound",     "Sounds & Vibration", "sound",     1},
    {"help",      "Help & Support",     "help",      1},
    {"storage",   "Storage",            "storage",   2},
    {"battery",   "Battery",            "battery",   2},
    {"apps",      "Apps",               "apps",      2},
    {"general",   "General Management", "general",   3},
    {"update",    "Software Update",    "update",    3},
    {"about",     "About Device",       "about",     3},
};
static const int NNAV = sizeof(NAV)/sizeof(*NAV);

static Fl_Double_Window* win;
static Fl_Scroll* content;
static int g_sel = 0;
static void select_page(int i);

// icon cache
static Fl_Image* icon(const char* name, int sz) {
    char p[256]; snprintf(p,sizeof p,"%s/%s-%d.png", ICON, name, sz);
    Fl_Shared_Image* im = Fl_Shared_Image::get(p);
    return im && im->w()>0 ? im : 0;
}

// -------------------------------------------------------------- sidebar --
class NavItem : public Fl_Button {
public:
    int idx; bool hov;
    NavItem(int x,int y,int w,int h,int i): Fl_Button(x,y,w,h,0), idx(i), hov(false) {
        box(FL_NO_BOX); clear_visible_focus();
    }
    int handle(int e) {
        if (e==FL_ENTER){ hov=true; redraw(); return 1; }
        if (e==FL_LEAVE){ hov=false; redraw(); return 1; }
        return Fl_Button::handle(e);
    }
    void draw() {
        bool cur = (idx==g_sel);
        if (cur)      { rbox(x()+8,y()+2,w()-16,h()-4,12, FIELD()); }
        else if (hov) { rbox(x()+8,y()+2,w()-16,h()-4,12, fl_rgb_color(0xEC,0xE8,0xF7)); }
        Fl_Image* ic = icon(NAV[idx].icon, 22);
        if (ic) ic->draw(x()+22, y()+(h()-22)/2);
        fl_color(cur? INK() : fl_rgb_color(0x40,0x3C,0x55));
        fl_font(cur? FL_HELVETICA_BOLD : FL_HELVETICA, 13);
        fl_draw(NAV[idx].label, x()+56, y(), w()-64, h(), FL_ALIGN_LEFT|FL_ALIGN_INSIDE);
    }
};

class Sidebar : public Fl_Group {
public:
    Sidebar(int x,int y,int w,int h): Fl_Group(x,y,w,h) {
        box(FL_NO_BOX); begin();
        // header + user card drawn in draw(); nav items are real widgets
        int yy = y+96;
        for (int i=0;i<NNAV;i++) {
            if (i>0 && NAV[i].group != NAV[i-1].group) yy += 10; // dashed gap
            NavItem* n = new NavItem(x, yy, w, 38, i);
            n->callback([](Fl_Widget* wdg,void*){ select_page(((NavItem*)wdg)->idx); });
            yy += 38;
        }
        end();
    }
    void draw() {
        // vertical lavender gradient
        for (int i=0;i<h();i++) {
            fl_color(lerp(fl_rgb_color(0xE9,0xE4,0xF6), fl_rgb_color(0xF3,0xEF,0xFB), (float)i/h()));
            fl_xyline(x(), y()+i, x()+w());
        }
        // header: "Settings" + search glyph
        fl_color(INK()); fl_font(FL_HELVETICA_BOLD, 18);
        fl_draw("Settings", x()+22, y()+18, 150, 22, FL_ALIGN_LEFT|FL_ALIGN_INSIDE);
        int sx=x()+w()-34, sy=y()+22;
        fl_color(MUTED()); fl_arc(sx,sy,12,12,0,360);
        fl_line(sx+11,sy+11, sx+16,sy+16);
        // user card
        soft_card(x()+14, y()+48, w()-28, 40, 12);
        fl_color(INK()); fl_font(FL_HELVETICA_BOLD,13);
        fl_draw("Guest", x()+26, y()+56, 100, 16, FL_ALIGN_LEFT|FL_ALIGN_INSIDE);
        fl_color(MUTED()); fl_font(FL_HELVETICA,10);
        fl_draw("xxri OS", x()+26, y()+72, 100, 12, FL_ALIGN_LEFT|FL_ALIGN_INSIDE);
        Fl_Image* lg = icon("help", 22);
        if (lg) lg->draw(x()+w()-44, y()+56);
        // dashed group dividers
        int yy = y()+96;
        for (int i=0;i<NNAV;i++) {
            if (i>0 && NAV[i].group != NAV[i-1].group) {
                dashed_hline(x()+22, yy+4, w()-44, BORDER());
                yy += 10;
            }
            yy += 38;
        }
        draw_children();
    }
};

// -------------------------------------------------- content build helpers --
// A vertical cursor-based builder inside the scroll's inner group.
static Fl_Group* inner;
static int CX, CY, CW;      // content x, running y, width

static void page_begin() {
    content->clear();
    content->begin();
    CX = content->x()+24; CW = content->w()-48;
    CY = content->y()+16;
    inner = content;
}
static void page_end() {
    // spacer box so the scroll knows the full height
    Fl_Box* sp = new Fl_Box(CX, CY, CW, 12); sp->box(FL_NO_BOX);
    content->end();
    content->redraw();
}

class Card : public Fl_Group {
public:
    Card(int x,int y,int w,int h): Fl_Group(x,y,w,h){ box(FL_NO_BOX); }
    void draw(){ soft_card(x(),y(),w(),h(),16); draw_children(); }
};
static Card* card(int h) {
    Card* c = new Card(CX, CY, CW, h);
    CY += h + 14;
    return c;
}
static void title(const char* t) {
    Fl_Box* b = new Fl_Box(CX, CY, CW, 30, 0);
    b->box(FL_NO_BOX); b->copy_label(t);
    b->labelfont(FL_HELVETICA_BOLD); b->labelsize(24); b->labelcolor(INK());
    b->align(FL_ALIGN_LEFT|FL_ALIGN_INSIDE);
    CY += 40;
}
static void section(const char* t) {
    Fl_Box* b = new Fl_Box(CX+4, CY, CW-8, 20, 0);
    b->box(FL_NO_BOX); b->copy_label(t);
    b->labelfont(FL_HELVETICA_BOLD); b->labelsize(13); b->labelcolor(PURPLE());
    b->align(FL_ALIGN_LEFT|FL_ALIGN_INSIDE);
    CY += 26;
}
// a label:value row inside a card (absolute coords)
static void row(Card* c, int ry, const char* k, const char* v, int kx=20) {
    Fl_Box* a = new Fl_Box(c->x()+kx, c->y()+ry, 200, 18, 0);
    a->box(FL_NO_BOX); a->copy_label(k); a->labelfont(FL_HELVETICA_BOLD);
    a->labelsize(12); a->labelcolor(INK()); a->align(FL_ALIGN_LEFT|FL_ALIGN_INSIDE);
    Fl_Box* b = new Fl_Box(c->x()+kx, c->y()+ry+16, c->w()-kx-20, 16, 0);
    b->box(FL_NO_BOX); b->copy_label(v?v:""); b->labelfont(FL_HELVETICA);
    b->labelsize(11); b->labelcolor(MUTED()); b->align(FL_ALIGN_LEFT|FL_ALIGN_INSIDE);
}
static void info_box(const char* msg) {
    int lines=1; for (const char*p=msg;*p;p++) if(*p=='\n')lines++;
    int h = 26 + lines*16;
    Card* c = card(h);
    c->begin();
    Fl_Box* b = new Fl_Box(c->x()+20, c->y()+12, c->w()-40, h-24, 0);
    b->box(FL_NO_BOX); b->copy_label(msg); b->labelfont(FL_HELVETICA);
    b->labelsize(12); b->labelcolor(MUTED());
    b->align(FL_ALIGN_LEFT|FL_ALIGN_INSIDE|FL_ALIGN_TOP|FL_ALIGN_WRAP);
    c->end();
}

// small helper: strdup into a static list so labels survive
static std::vector<std::string>* g_str;
static const char* keep(const std::string& s){ g_str->push_back(s); return g_str->back().c_str(); }

// ================================================================ PAGES ==
static void page_wifi() {
    title("Wi-Fi & Network");
    std::string st = run_cmd("xxri-network status --json");
    std::vector<std::string> ifs = jarr(st, "interfaces");
    section("Connection");
    for (size_t i=0;i<ifs.size();i++) {
        std::string name = jget(ifs[i],"interface");
        if (name=="lo"||name.compare(0,5,"dummy")==0||name.compare(0,4,"tunl")==0||name.compare(0,4,"sit0")==0) continue;
        std::string type=jget(ifs[i],"type"), state=jget(ifs[i],"state"),
                    ip=jget(ifs[i],"ip"), carrier=jget(ifs[i],"carrier"),
                    ssid=jget(ifs[i],"ssid");
        Card* c = card(88); c->begin();
        Fl_Image* ic = icon(type=="wifi"?"wifi":"devices", 40);
        if (ic) { Fl_Box* ib=new Fl_Box(c->x()+16,c->y()+16,40,40); ib->image(ic); ib->box(FL_NO_BOX); }
        std::string hdr = name + "  (" + type + ")";
        row(c, 14, keep(hdr), keep(std::string("Status: ")+(state.empty()?"unknown":state)+(carrier=="true"?"  · link up":"")), 68);
        row(c, 48, "", keep(ip.empty()? std::string("Not connected") : std::string("IP ")+ip+"   GW "+jget(st,"gateway")+"   DNS "+jget(st,"dns")), 68);
        if (!ssid.empty()) { Fl_Box* s=new Fl_Box(c->x()+c->w()-160,c->y()+14,150,16); s->copy_label(keep("SSID: "+ssid)); s->box(FL_NO_BOX); s->labelsize(11); s->labelcolor(PURPLE()); s->align(FL_ALIGN_RIGHT|FL_ALIGN_INSIDE); }
        c->end();
    }
    section("Wi-Fi Networks");
    std::string wifi = run_cmd("xxri-network scan --json");
    if (jget(wifi,"available")=="false" || wifi.find("\"networks\"")==std::string::npos) {
        info_box("No wireless adapter detected.\nWi-Fi drivers ship with xxri OS Lite; if your adapter needs firmware,\nConnected Devices and About Device will show what is missing.");
    } else {
        std::vector<std::string> nets = jarr(wifi, "networks");
        if (nets.empty()) info_box("No networks found. Move closer to an access point and reopen this page.");
        for (size_t i=0;i<nets.size() && i<8;i++) {
            std::string ss=jget(nets[i],"ssid"), sec=jget(nets[i],"security"), sig=jget(nets[i],"signal");
            if (ss.empty()) continue;
            Card* c = card(52); c->begin();
            Fl_Image* ic=icon("wifi",22); if(ic){Fl_Box* ib=new Fl_Box(c->x()+16,c->y()+15,22,22);ib->image(ic);ib->box(FL_NO_BOX);}
            Fl_Box* nm=new Fl_Box(c->x()+48,c->y()+8,c->w()-180,18); nm->copy_label(keep(ss)); nm->box(FL_NO_BOX); nm->labelfont(FL_HELVETICA_BOLD); nm->labelsize(13); nm->labelcolor(INK()); nm->align(FL_ALIGN_LEFT|FL_ALIGN_INSIDE);
            Fl_Box* mt=new Fl_Box(c->x()+48,c->y()+27,c->w()-180,14); mt->copy_label(keep(sec+"   signal "+sig+" dBm")); mt->box(FL_NO_BOX); mt->labelsize(10); mt->labelcolor(MUTED()); mt->align(FL_ALIGN_LEFT|FL_ALIGN_INSIDE);
            GhostBtn* b=new GhostBtn(c->x()+c->w()-96,c->y()+12,80,28,"Connect");
            c->end();
        }
    }
    page_end();
}

static void page_bluetooth() {
    title("Bluetooth");
    std::string st = run_cmd("xxri-bluetooth status --json");
    section("Adapter");
    if (jget(st,"available")=="true") {
        Card* c=card(60); c->begin();
        row(c,14,"Bluetooth","Adapter ready");
        Toggle* t=new Toggle(c->x()+c->w()-64,c->y()+18); t->value(1);
        c->end();
        section("Paired Devices");
        std::string pd=run_cmd("xxri-bluetooth paired --json");
        std::vector<std::string> ds=jarr(pd,"paired");
        if (ds.empty()) info_box("No paired devices yet. Use Scan to discover nearby devices.");
        for (size_t i=0;i<ds.size();i++){ Card* c2=card(48); c2->begin(); row(c2,12,keep(jget(ds[i],"name")),keep(jget(ds[i],"mac"))); GhostBtn* r=new GhostBtn(c2->x()+c2->w()-90,c2->y()+10,74,28,"Remove"); c2->end(); }
    } else {
        std::string reason=jget(st,"reason"), hint=jget(st,"hint");
        std::string msg="Bluetooth is not available.\n\n";
        msg += reason.empty()? "No adapter detected." : reason;
        if (!hint.empty()) msg += std::string("\n\nTo enable Bluetooth support:\n  ") + hint;
        else msg += "\n\nConnect a Bluetooth adapter, or install the stack with:\n  xxri-pkg -wi bluez5";
        info_box(keep(msg));
    }
    page_end();
}

static void page_devices() {
    title("Connected Devices");
    std::string usb=run_cmd("xxri-hardware usb --json");
    section("USB Devices");
    std::vector<std::string> us=jarr(usb,"usb");
    if (us.empty()) info_box("No USB devices connected.");
    for (size_t i=0;i<us.size();i++){ Card* c=card(46); c->begin();
        std::string man=jget(us[i],"manufacturer"),pr=jget(us[i],"product");
        row(c,11, keep(pr.empty()?("USB "+jget(us[i],"vendor_id")+":"+jget(us[i],"product_id")):pr), keep(man+"  ["+jget(us[i],"vendor_id")+":"+jget(us[i],"product_id")+"]"));
        c->end(); }
    section("Cameras");
    std::string cam=run_cmd("xxri-hardware camera --json");
    std::vector<std::string> cs=jarr(cam,"devices");
    if (cs.empty()) info_box("No camera detected. V4L2 webcam drivers install on demand from the Software Store.");
    for (size_t i=0;i<cs.size();i++){ Card* c=card(46); c->begin(); row(c,11,keep(jget(cs[i],"name")),keep(jget(cs[i],"device"))); c->end(); }
    section("Displays");
    std::string dsp=run_cmd("xxri-display current --json");
    Card* c=card(46); c->begin(); row(c,11,"Primary display",keep(jget(dsp,"mode")+"  @"+jget(dsp,"refresh")+"Hz  ("+jget(dsp,"driver")+")")); c->end();
    section("Printers");
    std::string pr=run_cmd("xxri-hardware printer --json");
    std::vector<std::string> ps=jarr(pr,"devices");
    if (ps.empty()) info_box("No printer detected.");
    for (size_t i=0;i<ps.size();i++){ Card* c2=card(40); c2->begin(); row(c2,10,keep(jget(ps[i],"product")),keep(jget(ps[i],"device"))); c2->end(); }
    page_end();
}

// wallpaper/style callbacks
static void cb_scale(Fl_Widget* w,void* d){ run_cmd(std::string("xxri-desktop wallpaper scale ")+(const char*)d); }
static void cb_dockpos(Fl_Widget* w,void* d){ run_cmd(std::string("xxri-desktop dock position ")+(const char*)d); }
static GradSlider* g_dock;
static void cb_docksize(Fl_Widget* w,void*){ char b[16]; snprintf(b,sizeof b,"%d",g_dock->val); run_cmd(std::string("xxri-desktop dock size ")+b); }
static void cb_appear(Fl_Widget* w,void* d){ run_cmd(std::string("xxri-desktop appearance set ")+(const char*)d); }

static void page_wallpaper() {
    title("Wallpaper & Style");
    std::string st=run_cmd("xxri-desktop status --json");
    section("Wallpaper");
    Card* c=card(70); c->begin();
    Fl_Image* wp=Fl_Shared_Image::get("/usr/local/share/xxri-theme/wallpapers/xxri-os-lite.jpg");
    if (wp&&wp->w()>0){ Fl_Image* t=wp->copy(74,44); Fl_Box* ib=new Fl_Box(c->x()+16,c->y()+13,74,44); ib->image(t); ib->box(FL_NO_BOX);}
    row(c,14,"Current wallpaper","xxri OS Lite (official)",104);
    row(c,42,"",keep(std::string("scaling: ")+jget(st,"wallpaper_scale")),104);
    c->end();
    section("Scaling");
    Card* sc=card(46); sc->begin();
    GhostBtn* b1=new GhostBtn(sc->x()+16,sc->y()+9,70,28,"Fill");   b1->callback(cb_scale,(void*)"fill");
    GhostBtn* b2=new GhostBtn(sc->x()+94,sc->y()+9,70,28,"Full");   b2->callback(cb_scale,(void*)"full");
    GhostBtn* b3=new GhostBtn(sc->x()+172,sc->y()+9,80,28,"Center");b3->callback(cb_scale,(void*)"center");
    GhostBtn* b4=new GhostBtn(sc->x()+260,sc->y()+9,70,28,"Tile");  b4->callback(cb_scale,(void*)"tile");
    sc->end();
    section("Dock");
    Card* dc=card(96); dc->begin();
    row(dc,12,"Position",0);
    GhostBtn* dt=new GhostBtn(dc->x()+120,dc->y()+10,80,28,"Top");    dt->callback(cb_dockpos,(void*)"top");
    GhostBtn* db=new GhostBtn(dc->x()+206,dc->y()+10,90,28,"Bottom"); db->callback(cb_dockpos,(void*)"bottom");
    row(dc,52,"Icon size",0);
    g_dock=new GradSlider(dc->x()+120,dc->y()+52,dc->w()-140,24,24,56);
    g_dock->set(atoi(jget(st,"dock_size").c_str())?atoi(jget(st,"dock_size").c_str()):40);
    g_dock->callback(cb_docksize);
    dc->end();
    section("Appearance");
    Card* ac=card(52); ac->begin();
    GhostBtn* li=new GhostBtn(ac->x()+16,ac->y()+12,90,28,"Light"); li->callback(cb_appear,(void*)"light");
    GhostBtn* da=new GhostBtn(ac->x()+114,ac->y()+12,90,28,"Dark"); da->callback(cb_appear,(void*)"dark");
    Fl_Box* n=new Fl_Box(ac->x()+214,ac->y()+12,ac->w()-230,28,"Dark theme is stored for a future update."); n->box(FL_NO_BOX); n->labelsize(10); n->labelcolor(MUTED()); n->align(FL_ALIGN_LEFT|FL_ALIGN_INSIDE);
    ac->end();
    section("Theme");
    Card* tc=card(46); tc->begin(); row(tc,11,"Icon theme","xxri"); Fl_Box* cu=new Fl_Box(tc->x()+tc->w()/2,tc->y(),tc->w()/2-20,tc->h()); cu->copy_label("Cursor: xxri-cursors"); cu->box(FL_NO_BOX); cu->labelsize(11); cu->labelcolor(MUTED()); cu->align(FL_ALIGN_RIGHT|FL_ALIGN_INSIDE); tc->end();
    page_end();
}

// sound callbacks
static GradSlider *g_vol, *g_mic;
static void cb_vol(Fl_Widget*,void*){ char b[16]; snprintf(b,sizeof b,"%d",g_vol->val); run_cmd(std::string("xxri-audio volume set ")+b); }
static void cb_mic(Fl_Widget*,void*){ char b[16]; snprintf(b,sizeof b,"%d",g_mic->val); run_cmd(std::string("xxri-audio mic set ")+b); }
static void cb_mute(Fl_Widget*,void*){ run_cmd("xxri-audio toggle"); select_page(g_sel); }
static void cb_test(Fl_Widget*,void*){ run_cmd("xxri-audio test"); }

static void page_sound() {
    title("Sounds & Vibration");
    std::string cards=run_cmd("xxri-audio list --json");
    if (jget(cards,"alsa")=="false" || cards.find("\"cards\"")==std::string::npos) {
        info_box("No active sound card.\nAudio drivers load automatically at boot; if none was found,\nAbout Device shows any missing firmware.");
        page_end(); return;
    }
    section("Output");
    std::vector<std::string> cs=jarr(cards,"cards");
    for (size_t i=0;i<cs.size();i++){ if(jget(cs[i],"playback")!="true")continue; Card* c=card(46); c->begin();
        row(c,11,keep(jget(cs[i],"name")),keep(std::string("type ")+jget(cs[i],"type")+(jget(cs[i],"default")=="true"?"   · default output":"")));
        c->end(); }
    std::string vol=run_cmd("xxri-audio volume get --json");
    section("Volume");
    Card* vc=card(58); vc->begin();
    g_vol=new GradSlider(vc->x()+20,vc->y()+18,vc->w()-160,22,0,100);
    g_vol->set(atoi(jget(vol,"volume").c_str())); g_vol->callback(cb_vol);
    GhostBtn* mb=new GhostBtn(vc->x()+vc->w()-130,vc->y()+16,60,28, jget(vol,"muted")=="true"?"Unmute":"Mute"); mb->callback(cb_mute);
    GhostBtn* tb=new GhostBtn(vc->x()+vc->w()-64,vc->y()+16,50,28,"Test"); tb->callback(cb_test);
    vc->end();
    section("Input");
    bool anycap=false;
    for (size_t i=0;i<cs.size();i++){ if(jget(cs[i],"capture")!="true")continue; anycap=true; Card* c=card(46); c->begin();
        row(c,11,keep(jget(cs[i],"name")),"microphone / capture"); c->end(); }
    if (!anycap) info_box("No microphone detected.");
    else {
        std::string mic=run_cmd("xxri-audio mic --json");
        section("Microphone Level");
        Card* mc=card(50); mc->begin();
        g_mic=new GradSlider(mc->x()+20,mc->y()+14,mc->w()-40,22,0,100);
        g_mic->set(atoi(jget(mic,"level").c_str())); g_mic->callback(cb_mic);
        mc->end();
    }
    page_end();
}

static void page_help() {
    title("Help & Support");
    info_box("Opening the xxri support site in your browser:\n\n  https://xxri.flows.best\n\nIf nothing opens, a browser is not installed yet - you can add one\nfrom the upcoming xxri Software Store.");
    Card* c=card(56); c->begin();
    GradBtn* b=new GradBtn(c->x()+c->w()/2-90,c->y()+12,180,32,"Open xxri.flows.best");
    b->callback([](Fl_Widget*,void*){ run_cmd("xxri-open-url https://xxri.flows.best &"); });
    c->end();
    page_end();
}

// storage callbacks
static void cb_mount(Fl_Widget*,void* d){ run_cmd(std::string("xxri-storage mount ")+(const char*)d); select_page(g_sel); }
static void cb_umount(Fl_Widget*,void* d){ run_cmd(std::string("xxri-storage unmount ")+(const char*)d); select_page(g_sel); }

static void page_storage() {
    title("Storage");
    std::string js=run_cmd("xxri-storage list --json");
    std::vector<std::string> disks=jarr(js,"disks");
    if (disks.empty()) info_box("No storage devices detected.");
    for (size_t i=0;i<disks.size();i++) {
        std::string dev=jget(disks[i],"device"), model=jget(disks[i],"model"), usb=jget(disks[i],"usb");
        section(keep(model+"   ("+dev+(usb=="true"?"  · removable)":")")));
        std::vector<std::string> parts=jarr(disks[i],"partitions");
        if (parts.empty()) { Card* c=card(44); c->begin(); row(c,11,keep(std::string("Whole disk ")+std::to_string(atoi(jget(disks[i],"size_mb").c_str()))+" MB"),"no partitions / not formatted"); c->end(); continue; }
        for (size_t k=0;k<parts.size();k++) {
            std::string pdev=jget(parts[k],"device"), fs=jget(parts[k],"fs"),
                        lbl=jget(parts[k],"label"), mp=jget(parts[k],"mountpoint"),
                        mounted=jget(parts[k],"mounted");
            long szmb=atol(jget(parts[k],"size_mb").c_str());
            Card* c=card(64); c->begin();
            std::string hdr=(lbl.empty()?pdev:lbl);
            row(c,10, keep(hdr), keep(pdev+"   "+fs+"   "+std::to_string(szmb)+" MB"));
            if (mounted=="true") {
                long fk=atol(jget(parts[k],"free_kb").c_str()), uk=atol(jget(parts[k],"used_kb").c_str());
                char u[96]; snprintf(u,sizeof u,"mounted at %s   %ld MB used, %ld MB free", mp.c_str(), uk/1024, fk/1024);
                Fl_Box* mb=new Fl_Box(c->x()+20,c->y()+42,c->w()-140,16); mb->copy_label(keep(u)); mb->box(FL_NO_BOX); mb->labelsize(10); mb->labelcolor(OK()); mb->align(FL_ALIGN_LEFT|FL_ALIGN_INSIDE);
                GhostBtn* e=new GhostBtn(c->x()+c->w()-110,c->y()+18,94,30, usb=="true"?"Eject":"Unmount"); e->callback(cb_umount,(void*)keep(pdev));
            } else {
                GhostBtn* m=new GhostBtn(c->x()+c->w()-96,c->y()+18,80,30,"Mount"); m->callback(cb_mount,(void*)keep(pdev));
            }
            c->end();
        }
    }
    page_end();
}

// battery / power callbacks
static GradSlider* g_bri;
static void cb_bri(Fl_Widget*,void*){ char b[16]; snprintf(b,sizeof b,"%d",g_bri->val); run_cmd(std::string("xxri-power brightness set ")+b); }
static void cb_gov(Fl_Widget*,void* d){ run_cmd(std::string("xxri-power governor set ")+(const char*)d); select_page(g_sel); }

static void page_battery() {
    title("Battery");
    std::string js=run_cmd("xxri-power info --json");
    section("Power");
    if (jget(js,"present")=="true") {
        std::vector<std::string> bs=jarr(js,"batteries");
        for (size_t i=0;i<bs.size();i++){ Card* c=card(56); c->begin();
            row(c,14,keep(std::string("Battery ")+jget(bs[i],"capacity")+"%"),keep(std::string("status: ")+jget(bs[i],"status")+"   "+jget(bs[i],"technology"))); c->end(); }
    } else {
        info_box("No battery detected.\nThis is normal on a desktop system.");
    }
    Card* ac=card(44); ac->begin();
    row(ac,11,"AC adapter", jget(js,"online")=="true"?"online":(jget(js,"present")=="true"?"offline":"powered (desktop)"));
    ac->end();
    section("Performance");
    std::string gv=run_cmd("xxri-power governor list --json");
    if (jget(gv,"available")=="false") info_box("CPU frequency scaling is not available on this processor.");
    else {
        Card* c=card(52); c->begin();
        row(c,12,"CPU governor",keep(jget(gv,"current")),20);
        std::string avail=jget(gv,"available");
        int bx=c->x()+150;
        // render up to 3 governor buttons
        std::string a=run_cmd("xxri-power governor list --json | tr -d '[]\"' ");
        const char* govs[]={"performance","powersave","ondemand","conservative","userspace","schedutil"};
        for (int gi=0; gi<6 && bx < c->x()+c->w()-90; gi++) {
            if (avail.find(govs[gi])==std::string::npos) continue;
            GhostBtn* b=new GhostBtn(bx,c->y()+12,90,28,govs[gi]); b->callback(cb_gov,(void*)govs[gi]);
            bx += 96;
        }
        c->end();
    }
    section("Display Brightness");
    std::string br=run_cmd("xxri-power brightness get --json");
    if (jget(br,"available")=="false") info_box("Brightness control is not available on this display.");
    else { Card* c=card(48); c->begin(); g_bri=new GradSlider(c->x()+20,c->y()+13,c->w()-40,22,0,100); g_bri->set(atoi(jget(br,"percent").c_str())); g_bri->callback(cb_bri); c->end(); }
    section("Sleep");
    std::string caps=run_cmd("xxri-power capabilities --json");
    Card* c=card(44); c->begin();
    row(c,11,"Suspend to RAM", jget(caps,"suspend")=="true"?"supported":"not supported");
    Fl_Box* hb=new Fl_Box(c->x()+c->w()/2,c->y(),c->w()/2-20,c->h()); hb->copy_label(jget(caps,"hibernate")=="true"?"Hibernate: supported":"Hibernate: no (no swap)"); hb->box(FL_NO_BOX); hb->labelsize(11); hb->labelcolor(MUTED()); hb->align(FL_ALIGN_RIGHT|FL_ALIGN_INSIDE);
    c->end();
    page_end();
}

// apps callbacks
static void cb_launch(Fl_Widget*,void* d){ run_cmd(std::string("xxri-app launch ")+(const char*)d+" &"); }
static void cb_remove(Fl_Widget*,void* d){ run_cmd(std::string("xxri-app remove-ui ")+(const char*)d+" &"); }

static void page_apps() {
    title("Apps");
    section("Installed Applications");
    std::string out=run_cmd("xxri-app list");
    if (out.empty()) { info_box("No AppImage applications installed yet.\nDownload an AppImage and open it, or use the upcoming Software Store.\nIntegrated apps appear here with launch and remove controls."); page_end(); return; }
    // list lines: id|name|version|file|last_launch
    size_t pos=0;
    while (pos < out.size()) {
        size_t nl=out.find('\n',pos); std::string line=out.substr(pos, nl==std::string::npos?std::string::npos:nl-pos);
        pos = (nl==std::string::npos)? out.size() : nl+1;
        if (line.empty()) continue;
        std::string f[5]; int fi=0; size_t sp=0;
        for (int k=0;k<5;k++){ size_t b=line.find('|',sp); f[k]=line.substr(sp, b==std::string::npos?std::string::npos:b-sp); if(b==std::string::npos)break; sp=b+1; fi=k; }
        Card* c=card(60); c->begin();
        char ip[256]; snprintf(ip,sizeof ip,"/usr/local/share/pixmaps/xxri-app-%s.png", f[0].c_str());
        Fl_Image* ai=Fl_Shared_Image::get(ip); if(ai&&ai->w()>0){ Fl_Image* t=ai->copy(40,40); Fl_Box* ib=new Fl_Box(c->x()+16,c->y()+10,40,40); ib->image(t); ib->box(FL_NO_BOX);}
        row(c,12,keep(f[1]),keep(std::string("version ")+f[2]+"   last launch: "+f[4]),68);
        GradBtn* l=new GradBtn(c->x()+c->w()-172,c->y()+16,80,30,"Launch"); l->callback(cb_launch,(void*)keep(f[0]));
        GhostBtn* r=new GhostBtn(c->x()+c->w()-84,c->y()+16,72,30,"Remove"); r->callback(cb_remove,(void*)keep(f[0]));
        c->end();
    }
    page_end();
}

// general callbacks
static void cb_autologin(Fl_Widget* w,void*){ run_cmd(std::string("xxri-desktop autologin set ")+(((Toggle*)w)->value()?"on":"off")); }
static Fl_Input* g_host;
static void cb_sethost(Fl_Widget*,void*){ if(g_host&&*g_host->value()) run_cmd(std::string("xxri-desktop hostname set ")+g_host->value()); }
static void cb_ntp(Fl_Widget*,void*){ run_cmd("xxri-desktop ntp-sync"); }
static void cb_reset(Fl_Widget*,void*){ if(run_cmd("xxri-dialog ask \"Reset desktop settings?\" \"Wallpaper, dock, accent and theme return to defaults. Your files are untouched.\" \"Reset\" \"Cancel\"; echo $?")=="0") run_cmd("xxri-desktop reset"); select_page(g_sel); }
static void cb_restart(Fl_Widget*,void*){ if(run_cmd("xxri-dialog ask \"Restart desktop session?\" \"All open applications will close.\" \"Restart\" \"Cancel\"; echo $?")=="0") run_cmd("xxri-desktop restart-session &"); }

static void page_general() {
    title("General Management");
    std::string st=run_cmd("xxri-desktop status --json");
    std::string dt=run_cmd("xxri-desktop datetime --json");
    section("Language & Region");
    Card* c=card(66); c->begin();
    row(c,12,"Language",keep(jget(st,"language")));
    row(c,38,"Region",keep(jget(st,"region").empty()?"not set":jget(st,"region")));
    c->end();
    section("Keyboard");
    std::string kb=run_cmd("xxri-input status --json");
    Card* kc=card(44); kc->begin(); row(kc,11,"Layout",keep(jget(kb,"keyboard_layout"))); kc->end();
    section("Date & Time");
    Card* tc=card(52); tc->begin();
    row(tc,12,keep(jget(dt,"date")+"  "+jget(dt,"time")),keep(std::string("timezone ")+jget(dt,"timezone")));
    GhostBtn* nb=new GhostBtn(tc->x()+tc->w()-120,tc->y()+12,104,28,"Sync time"); nb->callback(cb_ntp);
    tc->end();
    section("Account");
    Card* hc=card(96); hc->begin();
    Fl_Box* hl=new Fl_Box(hc->x()+20,hc->y()+10,120,18,"Hostname"); hl->box(FL_NO_BOX); hl->labelfont(FL_HELVETICA_BOLD); hl->labelsize(12); hl->labelcolor(INK()); hl->align(FL_ALIGN_LEFT|FL_ALIGN_INSIDE);
    g_host=new Fl_Input(hc->x()+20,hc->y()+30,hc->w()-140,26); g_host->value(jget(st,"hostname").c_str()); g_host->box(FL_BORDER_BOX); g_host->color(FIELD()); g_host->textsize(12);
    GhostBtn* sb=new GhostBtn(hc->x()+hc->w()-104,hc->y()+30,88,26,"Apply"); sb->callback(cb_sethost);
    Fl_Box* al=new Fl_Box(hc->x()+20,hc->y()+64,200,20,"Log in automatically"); al->box(FL_NO_BOX); al->labelfont(FL_HELVETICA_BOLD); al->labelsize(12); al->labelcolor(INK()); al->align(FL_ALIGN_LEFT|FL_ALIGN_INSIDE);
    Toggle* at=new Toggle(hc->x()+hc->w()-64,hc->y()+62); at->value(jget(st,"autologin")=="on"?1:0); at->callback(cb_autologin);
    hc->end();
    section("Desktop");
    Card* dc=card(52); dc->begin();
    GhostBtn* rb=new GhostBtn(dc->x()+16,dc->y()+12,150,30,"Reset settings"); rb->callback(cb_reset);
    GradBtn* rs=new GradBtn(dc->x()+176,dc->y()+12,180,30,"Restart session"); rs->callback(cb_restart);
    dc->end();
    page_end();
}

// update callbacks
static Fl_Box* g_updlog;
static void cb_upd(Fl_Widget*,void* d){ std::string r=run_cmd(std::string("xxri-updates ")+(const char*)d); if(g_updlog){ g_updlog->copy_label(keep(r)); g_updlog->parent()->redraw(); } }

static void page_update() {
    title("Software Update");
    std::string st=run_cmd("xxri-updates status --json");
    section("Packages");
    Card* c=card(60); c->begin();
    row(c,14,keep(jget(st,"installed")+" packages installed"),keep(std::string("mirror: ")+jget(st,"mirror")));
    c->end();
    section("Updates");
    Card* bc=card(54); bc->begin();
    GhostBtn* ck=new GhostBtn(bc->x()+16,bc->y()+12,120,30,"Check updates"); ck->callback(cb_upd,(void*)"check");
    GhostBtn* rf=new GhostBtn(bc->x()+144,bc->y()+12,130,30,"Refresh mirror"); rf->callback(cb_upd,(void*)"refresh");
    GradBtn* up=new GradBtn(bc->x()+282,bc->y()+12,110,30,"Update now"); up->callback(cb_upd,(void*)"apply");
    bc->end();
    Card* lc=card(120); lc->begin();
    g_updlog=new Fl_Box(lc->x()+16,lc->y()+12,lc->w()-32,96,"Use Check updates to compare installed packages against the mirror.\nUpdate now downloads newer packages; they activate on the next boot.");
    g_updlog->box(FL_NO_BOX); g_updlog->labelfont(FL_HELVETICA); g_updlog->labelsize(11); g_updlog->labelcolor(MUTED());
    g_updlog->align(FL_ALIGN_LEFT|FL_ALIGN_INSIDE|FL_ALIGN_TOP|FL_ALIGN_WRAP);
    lc->end();
    page_end();
}

class AboutHeader : public Fl_Box {
public:
    std::string osv,ver,up,loc;
    AboutHeader(int x,int y,int w,int h): Fl_Box(x,y,w,h){ box(FL_NO_BOX); }
    void draw() {
        soft_card(x(),y(),w(),h(),16);
        Fl_Image* lg=Fl_Shared_Image::get("/usr/local/share/xxri-settings/icons/logo-big.png");
        if(lg&&lg->w()>0) lg->draw(x()+w()-150, y()+18);
        int rx=x()+20, ry=y()+24;
        struct{const char*k;std::string v;} rows[]={{"OS",osv},{"Version",ver},{"Uptime",up},{"Locale",loc}};
        for(int i=0;i<4;i++){
            fl_color(MUTED()); fl_font(FL_HELVETICA,11); fl_draw(rows[i].k, rx, ry+i*30+12);
            fl_color(INK()); fl_font(FL_HELVETICA_BOLD,15); fl_draw(rows[i].v.c_str(), rx, ry+i*30+30);
        }
    }
};

static void page_about() {
    title("About Device");
    std::string s=run_cmd("xxri-hardware summary --json");
    std::string b=run_cmd("xxri-hardware bios --json");
    std::string disk=run_cmd("xxri-storage list --json");
    std::string dsp=run_cmd("xxri-display current --json");
    std::string bat=run_cmd("xxri-power battery --json");
    // header card with logo + OS/version/uptime/locale
    AboutHeader* h=new AboutHeader(CX,CY,CW,150);
    h->osv=jget(s,"os"); h->ver=jget(s,"os").find("Lite")!=std::string::npos?"2.0 Lite":jget(s,"os");
    h->up=jget(s,"uptime"); h->loc=jget(s,"locale");
    CY += 164;
    section("Hardware");
    long memmb=atol(jget(s,"memory_mb").c_str());
    std::vector<std::string> disks=jarr(disk,"disks");
    std::string disktxt="unknown";
    for (size_t i=0;i<disks.size();i++){ if(jget(disks[i],"usb")=="false"){ disktxt=jget(disks[i],"model")+"  "+std::to_string(atol(jget(disks[i],"size_mb").c_str())/1024)+" GiB"; break; } }
    Card* c=card(280); c->begin();
    int ry=16;
    row(c,ry,"CPU",keep(jget(s,"cpu"))); ry+=38;
    row(c,ry,"CPU Cores",keep(jget(s,"cores"))); ry+=38;
    row(c,ry,"GPU",keep(jget(s,"gpu"))); ry+=38;
    row(c,ry,"Memory",keep(std::to_string(memmb)+" MiB")); ry+=38;
    row(c,ry,"Disk",keep(disktxt)); ry+=38;
    row(c,ry,"Battery", jget(bat,"present")=="true"?"present":"N/A (Desktop)"); ry+=38;
    row(c,ry,"Display Resolution",keep(jget(dsp,"mode")));
    c->end();
    section("System");
    Card* c2=card(120); c2->begin();
    row(c2,14,"Kernel",keep(jget(s,"kernel")));
    row(c2,52,"Architecture",keep(jget(s,"arch")));
    row(c2,90-4,"BIOS Version",keep(jget(b,"vendor")+" "+jget(b,"version")));
    c2->end();
    section("Legal");
    Card* c3=card(96); c3->begin();
    row(c3,14,"License","GNU General Public License by Free Software Foundation v3 or later");
    row(c3,52,"Copyright","Copyright (C) 2025 Jantzen Maximillian kimlim");
    c3->end();
    page_end();
}

// ------------------------------------------------------------ dispatch --
static std::vector<std::string> g_strings;
static void select_page(int i) {
    g_sel = i;
    g_strings.clear(); g_str = &g_strings;
    win->begin();
    const char* id = NAV[i].id;
    if (!strcmp(id,"help")) { run_cmd("xxri-open-url https://xxri.flows.best &"); }
    page_begin();
    if      (!strcmp(id,"wifi"))      page_wifi();
    else if (!strcmp(id,"bluetooth")) page_bluetooth();
    else if (!strcmp(id,"devices"))   page_devices();
    else if (!strcmp(id,"wallpaper")) page_wallpaper();
    else if (!strcmp(id,"sound"))     page_sound();
    else if (!strcmp(id,"help"))      page_help();
    else if (!strcmp(id,"storage"))   page_storage();
    else if (!strcmp(id,"battery"))   page_battery();
    else if (!strcmp(id,"apps"))      page_apps();
    else if (!strcmp(id,"general"))   page_general();
    else if (!strcmp(id,"update"))    page_update();
    else if (!strcmp(id,"about"))     page_about();
    win->end();
    content->scroll_to(0,0);
    win->redraw();
}

int main(int argc, char** argv) {
    Fl::visual(FL_DOUBLE|FL_RGB);
    fl_register_images();
    Fl::background(0xFA,0xF9,0xFE);
    Fl::set_color(FL_SELECTION_COLOR,0x83,0x71,0xF7);
    Fl::scheme("gtk+");

    win = new Fl_Double_Window(WINW, WINH, "xxri Settings");
    win->color(FIELD());
    new Sidebar(0,0,NAVW,WINH);
    // content panel background
    Fl_Box* panel = new Fl_Box(NAVW,0,WINW-NAVW,WINH); panel->box(FL_FLAT_BOX); panel->color(FIELD());
    content = new Fl_Scroll(NAVW, 0, WINW-NAVW, WINH);
    content->type(Fl_Scroll::VERTICAL);
    content->color(FIELD());
    content->end();
    win->end();
    win->resizable(content);

    g_str = &g_strings;
    int start = 0;
    if (argc > 1) for (int i=0;i<NNAV;i++) if (!strcmp(argv[1],NAV[i].id)) start=i;
    select_page(start);

    win->show(1, argv);
    return Fl::run();
}
