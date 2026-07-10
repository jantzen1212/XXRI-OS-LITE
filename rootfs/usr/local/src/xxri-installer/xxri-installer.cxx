// xxri-installer - native graphical installer for xxri OS Lite (Phase 5).
//
// Frontend only: every disk operation is delegated to
// /usr/sbin/xxri-install-backend (which reuses install2disk unchanged).
// Protocol: P|pct|msg  L|detail  E|error  DONE|uuid  on the backend stdout.
//
// Toolkit: FLTK 1.3 - the toolkit the OS already ships for its desktop.
// GTK4/libadwaita do not exist in the TC 16.x repo and hard-require
// XInput2 + GL, which the Xvesa kdrive server does not provide; GTK3 would
// add ~8-23 MB compressed across 15+ packages.  The design language is
// reproduced with custom-drawn widgets instead (gradients, pill buttons,
// soft cards, gradient typography), all layout computed from the actual
// screen size (1024x768 / 1280x720 / 1366x768 / ...).

#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Secret_Input.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_JPEG_Image.H>
#include <FL/Fl_PNG_Image.H>
#include <FL/fl_draw.H>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <vector>
#include <string>

static int W = 1024, H = 768;    // actual screen size, set in main()

// ------------------------------------------------------------- palette --
static Fl_Color C_INK, C_MUTED, C_PINK, C_PURPLE, C_BLUE, C_SURFACE,
                C_CARD, C_FIELD, C_BORDER, C_TROUGH, C_DISAB, C_ERR;
static void init_palette() {
    C_INK     = fl_rgb_color(0x1C,0x1B,0x24);
    C_MUTED   = fl_rgb_color(0x6B,0x68,0x80);
    C_PINK    = fl_rgb_color(0xED,0x6C,0xE0);
    C_PURPLE  = fl_rgb_color(0x83,0x71,0xF7);
    C_BLUE    = fl_rgb_color(0x2E,0x7C,0xF6);
    C_SURFACE = fl_rgb_color(0xEF,0xED,0xF8);
    C_CARD    = fl_rgb_color(0xFA,0xF9,0xFE);
    C_FIELD   = fl_rgb_color(0xFF,0xFF,0xFF);
    C_BORDER  = fl_rgb_color(0xD9,0xD4,0xEA);
    C_TROUGH  = fl_rgb_color(0xE3,0xDF,0xF2);
    C_DISAB   = fl_rgb_color(0xCB,0xC6,0xDF);
    C_ERR     = fl_rgb_color(0xC2,0x4B,0x6E);
}
static Fl_Color lerp(Fl_Color a, Fl_Color b, float t) {
    uchar r1,g1,b1,r2,g2,b2;
    Fl::get_color(a,r1,g1,b1); Fl::get_color(b,r2,g2,b2);
    return fl_rgb_color((uchar)(r1+(r2-r1)*t),(uchar)(g1+(g2-g1)*t),(uchar)(b1+(b2-b1)*t));
}

// ------------------------------------------------------ drawing helpers --
static int isqrt(int v) { int r=0; while ((r+1)*(r+1) <= v) r++; return r; }

static void rbox(int x,int y,int w,int h,int r,Fl_Color c) {
    if (r*2 > h) r = h/2;
    if (r*2 > w) r = w/2;
    fl_color(c);
    fl_pie(x,y,2*r,2*r,90,180);
    fl_pie(x+w-2*r-1,y,2*r,2*r,0,90);
    fl_pie(x,y+h-2*r-1,2*r,2*r,180,270);
    fl_pie(x+w-2*r-1,y+h-2*r-1,2*r,2*r,270,360);
    fl_rectf(x+r,y,w-2*r,h);
    fl_rectf(x,y+r,r,h-2*r);
    fl_rectf(x+w-r,y+r,r,h-2*r);
}
static void rframe(int x,int y,int w,int h,int r,Fl_Color c) {
    if (r*2 > h) r = h/2;
    if (r*2 > w) r = w/2;
    fl_color(c);
    fl_arc(x,y,2*r,2*r,90,180);
    fl_arc(x+w-2*r-1,y,2*r,2*r,0,90);
    fl_arc(x,y+h-2*r-1,2*r,2*r,180,270);
    fl_arc(x+w-2*r-1,y+h-2*r-1,2*r,2*r,270,360);
    fl_xyline(x+r,y,x+w-r-1);
    fl_xyline(x+r,y+h-1,x+w-r-1);
    fl_yxline(x,y+r,y+h-r-1);
    fl_yxline(x+w-1,y+r,y+h-r-1);
}
static void grad_pill(int x,int y,int w,int h,Fl_Color c1,Fl_Color c2) {
    int r = h/2;
    for (int i=0;i<w;i++) {
        float t = (float)i/(w-1);
        fl_color(lerp(c1,c2,t));
        int inset = 0;
        if (i < r)        { int dx=r-i;       inset = r - isqrt(r*r-dx*dx); }
        else if (i >= w-r){ int dx=i-(w-r-1); inset = r - isqrt(r*r-dx*dx); }
        fl_yxline(x+i, y+inset, y+h-1-inset);
    }
}
static void grad_text(const char* s,int X,int Y,Fl_Font f,int sz,Fl_Color c1,Fl_Color c2) {
    fl_font(f,sz);
    int tw = (int)fl_width(s);
    int n = tw>240 ? 32 : 12;
    for (int i=0;i<n;i++) {
        int x0 = X + tw*i/n, x1 = X + tw*(i+1)/n;
        fl_push_clip(x0, Y-sz-6, x1-x0+1, sz+sz/2+10);
        fl_color(lerp(c1,c2,(float)i/(n-1)));
        fl_draw(s, X, Y);
        fl_pop_clip();
    }
}
static void soft_card(int x,int y,int w,int h,int r) {
    rbox(x-4,y-1,w+8,h+10,r+4, fl_rgb_color(0xCE,0xC8,0xE5));
    rbox(x-2,y-1,w+4,h+5,r+2,  fl_rgb_color(0xE4,0xDF,0xF2));
    rbox(x,y,w,h,r, C_CARD);
}
static void grad_disc(int cx,int cy,int d,Fl_Color a,Fl_Color b) {
    for (int i=0;i<d;i++) {
        fl_color(lerp(a,b,(float)i/d));
        fl_pie(cx,cy,d,d, 90-((i+1)*360/d), 90-(i*360/d)+1);
    }
}

// -------------------------------------------------------------- widgets --
class GradBtn : public Fl_Button {
public:
    bool hov;
    GradBtn(int x,int y,int w,int h,const char* l): Fl_Button(x,y,w,h,l), hov(false) {
        box(FL_NO_BOX); clear_visible_focus(); labelsize(15);
    }
    int handle(int e) {
        if (e==FL_ENTER){ hov=true;  redraw(); return 1; }
        if (e==FL_LEAVE){ hov=false; redraw(); return 1; }
        return Fl_Button::handle(e);
    }
    void draw() {
        Fl_Color a=C_PINK, b=C_PURPLE;
        if (!active()) { a=b=C_DISAB; }
        else if (value()) { a=lerp(C_PINK,FL_BLACK,0.18f); b=lerp(C_PURPLE,FL_BLACK,0.18f); }
        else if (hov)     { a=lerp(C_PINK,FL_WHITE,0.16f); b=lerp(C_PURPLE,FL_WHITE,0.16f); }
        grad_pill(x(),y(),w(),h(),a,b);
        fl_color(active()? FL_WHITE : fl_rgb_color(0x8A,0x86,0xA3));
        fl_font(FL_HELVETICA_BOLD, labelsize());
        fl_draw(label(), x(), y()-1, w(), h(), FL_ALIGN_CENTER);
    }
};
class GhostBtn : public Fl_Button {
public:
    bool hov;
    GhostBtn(int x,int y,int w,int h,const char* l): Fl_Button(x,y,w,h,l), hov(false) {
        box(FL_NO_BOX); clear_visible_focus(); labelsize(14);
    }
    int handle(int e) {
        if (e==FL_ENTER){ hov=true;  redraw(); return 1; }
        if (e==FL_LEAVE){ hov=false; redraw(); return 1; }
        return Fl_Button::handle(e);
    }
    void draw() {
        rbox(x(),y(),w(),h(),h()/2, value()? C_TROUGH : hov? FL_WHITE : C_CARD);
        rframe(x(),y(),w(),h(),h()/2, hov? C_PURPLE : C_BORDER);
        fl_color(C_INK);
        fl_font(FL_HELVETICA_BOLD, labelsize());
        fl_draw(label(), x(), y()-1, w(), h(), FL_ALIGN_CENTER);
    }
};

// rounded text input: the widget rect is only the text zone; the field
// (white rounded box + focus ring) is drawn around it, so FLTK's square
// input box never covers the rounded corners (fixes the Phase 5 clipping
// and z-order issues on the account page).
#define FIELD_PAD_X 12
#define FIELD_PAD_Y 6
class RInput : public Fl_Input {
public:
    RInput(int x,int y,int w,int h,const char* l=0)
        : Fl_Input(x+FIELD_PAD_X,y+FIELD_PAD_Y,w-2*FIELD_PAD_X,h-2*FIELD_PAD_Y,l) {
        box(FL_NO_BOX); color(C_FIELD); textcolor(C_INK);
        textfont(FL_HELVETICA); textsize(13); labelsize(12);
        labelfont(FL_HELVETICA); labelcolor(C_MUTED);
        align(FL_ALIGN_TOP_LEFT); cursor_color(C_PINK);
        selection_color(C_PURPLE);
    }
    int fx() { return x()-FIELD_PAD_X; }
    int fy() { return y()-FIELD_PAD_Y; }
    int fw() { return w()+2*FIELD_PAD_X; }
    int fh() { return h()+2*FIELD_PAD_Y; }
    int handle(int e) {
        int r = Fl_Input::handle(e);
        if (e==FL_FOCUS || e==FL_UNFOCUS) window()->redraw();
        return r;
    }
    void draw() {
        rbox(fx()-2,fy()-2,fw()+4,fh()+4,11, Fl::focus()==this? C_PURPLE : C_BORDER);
        rbox(fx(),fy(),fw(),fh(),9, C_FIELD);
        Fl_Input::draw();
    }
};
class RSecret : public Fl_Secret_Input {
public:
    RSecret(int x,int y,int w,int h,const char* l=0)
        : Fl_Secret_Input(x+FIELD_PAD_X,y+FIELD_PAD_Y,w-2*FIELD_PAD_X,h-2*FIELD_PAD_Y,l) {
        box(FL_NO_BOX); color(C_FIELD); textcolor(C_INK);
        textfont(FL_HELVETICA); textsize(13); labelsize(12);
        labelfont(FL_HELVETICA); labelcolor(C_MUTED);
        align(FL_ALIGN_TOP_LEFT); cursor_color(C_PINK);
        selection_color(C_PURPLE);
    }
    int fx() { return x()-FIELD_PAD_X; }
    int fy() { return y()-FIELD_PAD_Y; }
    int fw() { return w()+2*FIELD_PAD_X; }
    int fh() { return h()+2*FIELD_PAD_Y; }
    int handle(int e) {
        int r = Fl_Secret_Input::handle(e);
        if (e==FL_FOCUS || e==FL_UNFOCUS) window()->redraw();
        return r;
    }
    void draw() {
        rbox(fx()-2,fy()-2,fw()+4,fh()+4,11, Fl::focus()==this? C_PURPLE : C_BORDER);
        rbox(fx(),fy(),fw(),fh(),9, C_FIELD);
        Fl_Secret_Input::draw();
    }
};
class RChoice : public Fl_Choice {
public:
    RChoice(int x,int y,int w,int h,const char* l=0): Fl_Choice(x,y,w,h,l) {
        box(FL_NO_BOX); color(C_FIELD); textcolor(C_INK);
        textfont(FL_HELVETICA); textsize(12);
        labelfont(FL_HELVETICA); labelsize(13); labelcolor(C_INK);
        align(FL_ALIGN_LEFT); clear_visible_focus();
    }
    void draw() {
        rbox(x(),y(),w(),h(),h()/2, C_FIELD);
        rframe(x(),y(),w(),h(),h()/2, C_BORDER);
        fl_color(C_INK); fl_font(FL_HELVETICA,12);
        if (text()) fl_draw(text(), x()+14, y(), w()-34, h(), FL_ALIGN_LEFT);
        int ax=x()+w()-20, ay=y()+h()/2-2;
        fl_color(C_MUTED);
        fl_polygon(ax,ay, ax+8,ay, ax+4,ay+5);
        draw_label();
    }
};
class RCheck : public Fl_Check_Button {
public:
    RCheck(int x,int y,int w,int h,const char* l): Fl_Check_Button(x,y,w,h,l) {
        box(FL_NO_BOX); clear_visible_focus();
        labelfont(FL_HELVETICA); labelsize(13); labelcolor(C_INK);
    }
    void draw() {
        int d=18, bx=x(), by=y()+(h()-d)/2;
        if (value()) {
            // small gradient rounded square
            for (int i=0;i<d;i++) {
                fl_color(lerp(C_PINK,C_PURPLE,(float)i/d));
                int ins=0, r=5;
                if (i<r)        { int dx=r-i;     ins=r-isqrt(r*r-dx*dx); }
                else if (i>=d-r){ int dx=i-(d-r-1); ins=r-isqrt(r*r-dx*dx); }
                fl_yxline(bx+i, by+ins, by+d-1-ins);
            }
            fl_color(FL_WHITE);
            fl_line_style(FL_SOLID|FL_CAP_ROUND, 2);
            fl_line(bx+4, by+9, bx+7, by+13);
            fl_line(bx+7, by+13, bx+14, by+5);
            fl_line_style(0);
        } else {
            rbox(bx,by,d,d,5, C_FIELD);
            rframe(bx,by,d,d,5, C_BORDER);
        }
        fl_color(C_INK); fl_font(FL_HELVETICA, labelsize());
        fl_draw(label(), bx+d+9, y(), w()-d-9, h(), FL_ALIGN_LEFT);
    }
};
class Progress : public Fl_Widget {
public:
    float pct;
    Progress(int x,int y,int w,int h): Fl_Widget(x,y,w,h,0), pct(0) {}
    void set(float p){ pct=p; parent()->redraw(); }
    void draw() {
        char t[16]; snprintf(t,sizeof t,"%d%%",(int)pct);
        fl_color(C_MUTED); fl_font(FL_HELVETICA_BOLD,11);
        fl_draw(t, x()+w()-44, y()-20, 44, 14, FL_ALIGN_RIGHT);
        rbox(x(),y(),w(),h(),h()/2, C_TROUGH);
        int fw = (int)(w()*pct/100.0f);
        if (fw >= h()) grad_pill(x(),y(),fw,h(),C_PINK,C_PURPLE);
        else if (fw>0) rbox(x(),y(),h(),h(),h()/2,C_PINK);
    }
};

// disks -------------------------------------------------------------------
struct DiskInfo { std::string dev, model, fs; long mib; bool removable; };
static std::vector<DiskInfo> g_disks;
static std::vector<DiskInfo> g_parts;

class DiskRow : public Fl_Button {
public:
    DiskInfo info;
    Fl_RGB_Image* icon;
    bool hov;
    DiskRow(int x,int y,int w,int h,const DiskInfo& d, Fl_RGB_Image* ic)
        : Fl_Button(x,y,w,h,0), info(d), icon(ic), hov(false) {
        box(FL_NO_BOX); type(FL_RADIO_BUTTON); clear_visible_focus();
    }
    int handle(int e) {
        if (e==FL_ENTER){ hov=true;  redraw(); return 1; }
        if (e==FL_LEAVE){ hov=false; redraw(); return 1; }
        return Fl_Button::handle(e);
    }
    void draw() {
        soft_card(x(),y(),w(),h(),16);
        if (value() || hov)
            rframe(x(),y(),w(),h(),16, value()? C_PURPLE : C_BORDER);
        rbox(x()+14, y()+(h()-48)/2, 48, 48, 12, C_SURFACE);
        if (icon) icon->draw(x()+16, y()+(h()-44)/2);
        char l1[160], l2[96];
        double g = info.mib/1024.0;
        snprintf(l1,sizeof l1,"%s %.1fGiB%s", info.model.c_str(), g,
                 info.removable ? "  (removable)" : "");
        snprintf(l2,sizeof l2,"%s", info.fs.c_str());
        fl_color(C_INK); fl_font(FL_HELVETICA_BOLD,14);
        fl_draw(l1, x()+76, y()+h()/2-6);
        fl_color(C_MUTED); fl_font(FL_HELVETICA,12);
        fl_draw(l2, x()+76, y()+h()/2+15);
        int cx = x()+w()-42, cy = y()+h()/2-10, d=20;
        if (value()) {
            grad_disc(cx,cy,d,C_PINK,C_PURPLE);
            fl_color(FL_WHITE); fl_pie(cx+6,cy+6,8,8,0,360);
        } else {
            fl_color(hov? C_PURPLE : C_BORDER); fl_arc(cx,cy,d,d,0,360);
        }
    }
};

// --------------------------------------------------------------- window --
class WallWin : public Fl_Double_Window {
public:
    Fl_Image* wall;
    WallWin(int w,int h): Fl_Double_Window(w,h,"xxri OS Lite Installer"), wall(0) {}
    void draw() {
        if (wall) wall->draw(0,0);
        else { fl_color(C_SURFACE); fl_rectf(0,0,w(),h()); }
        draw_children();
    }
    int handle(int e);
};

// --------------------------------------------------------------- state --
static WallWin*  win;
static Fl_Group *pg_welcome, *pg_account, *pg_dest, *pg_prog, *pg_finish;
static RChoice  *ch_region, *ch_lang, *ch_kbd;
static RInput   *in_host, *in_user;
static RSecret  *in_pw, *in_pw2;
static Fl_Box   *bx_strength, *bx_acc_err, *bx_op, *bx_tasks, *bx_err, *bx_uuid;
static RCheck   *ck_auto;
static Fl_Button*bt_err_close;
static Progress *prog;
static Fl_Group *grp_disklist;
static bool      g_manual = false;
static Fl_Button*seg_full,*seg_man;
static std::string g_sel_dev;
static FILE*     g_backend = 0;

struct TzRegion { const char* label; const char* tz; };
static const TzRegion REGIONS[] = {
    {"Indonesia / Jakarta",   "WIB-7"},
    {"Indonesia / Makassar",  "WITA-8"},
    {"Indonesia / Jayapura",  "WIT-9"},
    {"UTC",                   "UTC0"},
    {"United States / Eastern","EST5EDT,M3.2.0,M11.1.0"},
    {"United States / Central","CST6CDT,M3.2.0,M11.1.0"},
    {"United States / Pacific","PST8PDT,M3.2.0,M11.1.0"},
    {"United Kingdom / London","GMT0BST,M3.5.0/1,M10.5.0"},
    {"Europe / Berlin",       "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe / Moscow",       "MSK-3"},
    {"India / New Delhi",     "IST-5:30"},
    {"China / Beijing",       "CST-8"},
    {"Japan / Tokyo",         "JST-9"},
    {"Singapore",             "SGT-8"},
    {"Australia / Sydney",    "AEST-10AEDT,M10.1.0,M4.1.0/3"},
    {"Brazil / Sao Paulo",    "BRT3"},
};
struct Lang { const char* label; const char* code; };
static const Lang LANGS[] = {
    {"English (United States)", "en_US"},
    {"English (United Kingdom)","en_GB"},
    {"Bahasa Indonesia",        "id_ID"},
};

static void show_page(Fl_Group* p) {
    pg_welcome->hide(); pg_account->hide(); pg_dest->hide();
    pg_prog->hide(); pg_finish->hide();
    p->show();
    win->redraw();
}

// installer session: the installer is the only active application.  wbar
// is stopped for the whole session and restored only when the user picks
// "Continue Live Session".
static void wbar_kill_cb(void*) { system("pkill -x wbar >/dev/null 2>&1"); }
static void session_exclusive() {
    system("pkill -x wbar >/dev/null 2>&1");
    Fl::add_timeout(2.5, wbar_kill_cb);   // catch a late wbar spawn from .xsession
    Fl::add_timeout(5.0, wbar_kill_cb);
}
static void session_restore() {
    system("U=$(cat /etc/sysconfig/tcuser 2>/dev/null); [ -n \"$U\" ] && "
           "su - \"$U\" -c 'export DISPLAY=:0.0; nohup wbar.sh >/dev/null 2>&1 &'");
}

// ------------------------------------------------------------ backend --
static const char* TASKS[] = {
    "Partition target", "Format XXRIROOT (ext4)", "Copy system files",
    "Create account", "Hostname & locale", "Install bootloader", "Finalize" };
static int task_for_pct(int p) {
    if (p < 10) return 0;
    if (p < 14) return 1;
    if (p < 80) return 2;
    if (p < 84) return 3;
    if (p < 88) return 4;
    if (p < 97) return 5;
    return 6;
}
static char g_tasksbuf[512];
static char g_uuid[160];
static void backend_line(const char* line) {
    if (!strncmp(line,"P|",2)) {
        int pct = atoi(line+2);
        const char* msg = strchr(line+2,'|'); msg = msg? msg+1 : "";
        prog->set((float)pct);
        static char op[160]; snprintf(op,sizeof op,"%s...",msg);
        bx_op->label(op);
        int t = task_for_pct(pct);
        g_tasksbuf[0]=0;
        for (unsigned i=0;i<sizeof(TASKS)/sizeof(*TASKS);i++) {
            strcat(g_tasksbuf, i<(unsigned)t ? "[done]  " : (i==(unsigned)t? ">>  ":"  -  "));
            strcat(g_tasksbuf, TASKS[i]); strcat(g_tasksbuf,"\n");
        }
        bx_tasks->label(g_tasksbuf);
        pg_prog->redraw();
    } else if (!strncmp(line,"E|",2)) {
        static char em[512];
        snprintf(em,sizeof em,"%s\n\nOnly the target disk was touched. Close to review and retry.", line+2);
        bx_err->label(em);
        bx_err->show(); bt_err_close->show();
        pg_prog->redraw();
    } else if (!strncmp(line,"DONE|",5)) {
        snprintf(g_uuid,sizeof g_uuid,"Root filesystem  LABEL=XXRIROOT  -  UUID %s", line+5);
        bx_uuid->label(g_uuid);
        show_page(pg_finish);
    }
}
static void backend_fd_cb(int fd, void*) {
    static char buf[1024]; static int len=0;
    int n = read(fd, buf+len, sizeof(buf)-1-len);
    if (n <= 0) {
        Fl::remove_fd(fd);
        if (g_backend) { pclose(g_backend); g_backend=0; }
        return;
    }
    len += n; buf[len]=0;
    char* p = buf;
    for (char* nl; (nl=strchr(p,'\n')); p=nl+1) { *nl=0; backend_line(p); }
    len = strlen(p); memmove(buf,p,len+1);
}
static void start_install() {
    FILE* c = fopen("/tmp/xxri-install.cfg","w");
    if (!c) return;
    fprintf(c,"MODE=%s\n", g_manual? "manual":"auto");
    if (g_manual) fprintf(c,"PART=%s\n", g_sel_dev.c_str());
    else          fprintf(c,"DISK=%s\n", g_sel_dev.c_str());
    fprintf(c,"USERNAME=%s\n", in_user->value());
    fprintf(c,"PASSWORD=%s\n", in_pw->value());
    fprintf(c,"HOSTNAME=%s\n", in_host->value());
    fprintf(c,"AUTOLOGIN=%d\n", ck_auto->value()?1:0);
    fprintf(c,"TZSTR=%s\n", REGIONS[ch_region->value()].tz);
    fprintf(c,"LANGUAGE=%s\n", LANGS[ch_lang->value()].code);
    fprintf(c,"KEYMAP=us\n");
    fclose(c);
    bx_err->hide(); bt_err_close->hide();
    show_page(pg_prog);
    g_backend = popen("/usr/sbin/xxri-install-backend run /tmp/xxri-install.cfg 2>/tmp/xxri-install.err","r");
    if (g_backend) Fl::add_fd(fileno(g_backend), FL_READ, backend_fd_cb, 0);
}

// ---------------------------------------------------------- callbacks --
static void cb_welcome_next(Fl_Widget*,void*) { show_page(pg_account); }
static bool valid_name(const char* s, bool user) {
    if (!*s) return false;
    for (const char* p=s;*p;p++) {
        char c=*p;
        if (user) { if (!(islower(c)||isdigit(c)||c=='-'||c=='_')) return false; }
        else      { if (!(isalnum(c)||c=='-')) return false; }
    }
    return !user || !isdigit(*s);
}
static void cb_account_next(Fl_Widget*,void*) {
    static char err[128];
    if (!valid_name(in_user->value(), true))
        { snprintf(err,sizeof err,"Username: lowercase letters, digits, - or _ (not starting with a digit)."); bx_acc_err->label(err); bx_acc_err->show(); win->redraw(); return; }
    if (!valid_name(in_host->value(), false))
        { snprintf(err,sizeof err,"Computer name: letters, digits or - only."); bx_acc_err->label(err); bx_acc_err->show(); win->redraw(); return; }
    if (strcmp(in_pw->value(), in_pw2->value()))
        { snprintf(err,sizeof err,"Passwords do not match."); bx_acc_err->label(err); bx_acc_err->show(); win->redraw(); return; }
    bx_acc_err->hide();
    show_page(pg_dest);
}
static void cb_pw_changed(Fl_Widget*,void*) {
    size_t n = strlen(in_pw->value());
    bx_strength->label(n==0 ? "" : n<6 ? "Weak" : n<10 ? "Good" : "Strong");
    bx_strength->labelcolor(n<6? C_ERR : n<10? C_PURPLE : C_BLUE);
    win->redraw();
}
static void rebuild_disklist();
static void cb_seg(Fl_Widget* w,void*) {
    g_manual = (w == seg_man);
    seg_full->value(!g_manual); seg_man->value(g_manual);
    g_sel_dev.clear();
    std::vector<DiskInfo>& v = g_manual ? g_parts : g_disks;
    if (!v.empty()) g_sel_dev = v[0].dev;
    rebuild_disklist();
}
static void cb_diskrow(Fl_Widget* w,void*) {
    DiskRow* r = (DiskRow*)w;
    g_sel_dev = r->info.dev;
    grp_disklist->redraw();
}
static void cb_dest_next(Fl_Widget*,void*) {
    if (g_sel_dev.empty()) return;
    start_install();
}
static void cb_restart(Fl_Widget*,void*)  { sync(); execlp("/sbin/reboot","reboot",(char*)0); }
static void cb_poweroff(Fl_Widget*,void*) { sync(); execlp("/sbin/poweroff","poweroff",(char*)0); }
static void cb_live(Fl_Widget*,void*)     { session_restore(); exit(0); }
static void cb_err_close(Fl_Widget*,void*){ bx_err->hide(); bt_err_close->hide(); show_page(pg_dest); }

// ------------------------------------------------------------- queries --
static void load_devices() {
    g_disks.clear(); g_parts.clear();
    FILE* f = popen("/usr/sbin/xxri-install-backend list-disks","r");
    char l[512];
    if (f) { while (fgets(l,sizeof l,f)) {
        char dev[64],model[128],fs[128]; long mib; int rm;
        if (sscanf(l,"DISK|%63[^|]|%127[^|]|%ld|%d|%127[^|\n]",dev,model,&mib,&rm,fs)==5) {
            DiskInfo d; d.dev=dev; d.model=model; d.mib=mib; d.removable=rm;
            d.fs=std::string("Current content: ")+fs;
            g_disks.push_back(d);
        } } pclose(f); }
    f = popen("/usr/sbin/xxri-install-backend list-parts","r");
    if (f) { while (fgets(l,sizeof l,f)) {
        char dev[64],fs[64],lab[64]; long mib; lab[0]=0;
        int k = sscanf(l,"PART|%63[^|]|%ld|%63[^|]|%63[^|\n]",dev,&mib,fs,lab);
        if (k>=3) {
            DiskInfo d; d.dev=dev; d.mib=mib; d.removable=false;
            d.model=dev;
            d.fs=std::string(fs)+(lab[0]?std::string("  label ")+lab:"")+"  -  will be formatted ext4";
            g_parts.push_back(d);
        } } pclose(f); }
}

static Fl_RGB_Image* g_diskicon = 0;
static void rebuild_disklist() {
    grp_disklist->clear();
    grp_disklist->begin();
    std::vector<DiskInfo>& v = g_manual ? g_parts : g_disks;
    int y = grp_disklist->y(), x = grp_disklist->x(), w = grp_disklist->w();
    unsigned maxrows = (unsigned)((grp_disklist->h()-30) / 86);
    if (maxrows < 1) maxrows = 1;
    unsigned shown = v.size()>maxrows ? maxrows : v.size();
    for (unsigned i=0;i<shown;i++) {
        DiskRow* r = new DiskRow(x, y+6+(int)i*86, w, 72, v[i], g_diskicon);
        r->callback(cb_diskrow);
        if (v[i].dev == g_sel_dev) r->value(1);
    }
    if (v.size()>shown) {
        static char more[64];
        snprintf(more,sizeof more,"...and %u more device(s)", (unsigned)(v.size()-shown));
        Fl_Box* mb = new Fl_Box(x, y+6+(int)shown*86, w, 24, more);
        mb->labelfont(FL_HELVETICA); mb->labelsize(12); mb->labelcolor(C_MUTED);
        mb->box(FL_NO_BOX);
    }
    if (v.empty()) {
        Fl_Box* b = new Fl_Box(x,y+10,w,40, g_manual?
            "No partitions found - use Full disk mode." : "No target disks detected.");
        b->labelfont(FL_HELVETICA); b->labelsize(13); b->labelcolor(C_MUTED);
        b->box(FL_NO_BOX);
    }
    grp_disklist->end();
    win->redraw();
}

// --------------------------------------------------------------- pages --
static void caption(int x,int y,int w,const char* t) {
    Fl_Box* b = new Fl_Box(x,y,w,14,t);
    b->labelfont(FL_HELVETICA); b->labelsize(12); b->labelcolor(C_MUTED);
    b->align(FL_ALIGN_LEFT|FL_ALIGN_INSIDE); b->box(FL_NO_BOX);
}

class CardBg : public Fl_Box {
public:
    int rad;
    CardBg(int x,int y,int w,int h,int r=18): Fl_Box(x,y,w,h), rad(r) { box(FL_NO_BOX); }
    void draw(){ soft_card(x(),y(),w(),h(),rad); }
};

// gradient "Welcome" wordmark + "to XXRI OS Lite" right-aligned under it
class WelcomeArt : public Fl_Box {
public:
    WelcomeArt(int x,int y,int w,int h): Fl_Box(x,y,w,h) { box(FL_NO_BOX); }
    void draw() {
        int sz = W>=1200 ? 100 : 92;
        fl_font(FL_HELVETICA_BOLD, sz);
        int tw = (int)fl_width("Welcome");
        int X = x()+(w()-tw)/2, Y = y()+sz;
        grad_text("Welcome", X, Y, FL_HELVETICA_BOLD, sz, C_PINK, C_BLUE);
        fl_font(FL_HELVETICA, 18);
        int w1 = (int)fl_width("to ");
        fl_font(FL_HELVETICA_BOLD, 18);
        int w2 = (int)fl_width("XXRI OS Lite");
        int sx = X+tw-(w1+w2), sy = Y+36;
        fl_color(C_MUTED); fl_font(FL_HELVETICA,18); fl_draw("to ", sx, sy);
        fl_color(C_BLUE);  fl_font(FL_HELVETICA_BOLD,18); fl_draw("XXRI OS Lite", sx+w1, sy);
    }
};

static void build_welcome() {
    pg_welcome = new Fl_Group(0,0,W,H);
    new WelcomeArt(0, H/6, W, 160);

    int cw=400, chh=138, cx=(W-cw)/2, cy=H-252;
    new CardBg(cx,cy,cw,chh,16);
    int lx=cx+118, lw=cw-152;
    ch_region = new RChoice(lx,cy+16,lw,28,"Region :");
    for (unsigned i=0;i<sizeof(REGIONS)/sizeof(*REGIONS);i++) {
        std::string esc;              // '/' would create an FLTK submenu
        for (const char* p=REGIONS[i].label; *p; p++) { if(*p=='/') esc+="\\"; esc+=*p; }
        ch_region->add(esc.c_str(),0,0);
    }
    ch_region->value(0);
    ch_lang = new RChoice(lx,cy+55,lw,28,"Language :");
    for (unsigned i=0;i<sizeof(LANGS)/sizeof(*LANGS);i++) ch_lang->add(LANGS[i].label,0,0);
    ch_lang->value(0);
    ch_kbd = new RChoice(lx,cy+94,lw,28,"Keyboard :");
    ch_kbd->add("US (default)",0,0); ch_kbd->value(0);

    GradBtn* nx = new GradBtn(W/2-54,H-86,108,36,"Next  >");
    nx->callback(cb_welcome_next);
    pg_welcome->end();
}

class AccTitle : public Fl_Box {
public:
    AccTitle(int x,int y,int w,int h): Fl_Box(x,y,w,h) { box(FL_NO_BOX); }
    void draw() {
        fl_font(FL_HELVETICA_BOLD,21);
        const char *a="Create a ", *b="XXRI", *c=" Account";
        int wa=(int)fl_width(a), wb=(int)fl_width(b), wc=(int)fl_width(c);
        int X=x()+(w()-wa-wb-wc)/2, Y=y()+h()/2+7;
        fl_color(C_INK); fl_draw(a,X,Y);
        grad_text(b,X+wa,Y,FL_HELVETICA_BOLD,21,C_PINK,C_PURPLE);
        fl_color(C_INK); fl_draw(c,X+wa+wb,Y);
    }
};
class Avatar : public Fl_Box {
public:
    Fl_Image* im; bool plus;
    Avatar(int x,int y,int w,int h,Fl_Image*i,bool p): Fl_Box(x,y,w,h),im(i),plus(p) { box(FL_NO_BOX); }
    void draw() {
        rbox(x(),y(),w(),h(),15, plus? fl_rgb_color(0xE2,0xDF,0xEC) : C_SURFACE);
        if (plus) {
            fl_color(C_INK); fl_line_style(FL_SOLID|FL_CAP_ROUND,3);
            fl_line(x()+w()/2-8,y()+h()/2, x()+w()/2+8,y()+h()/2);
            fl_line(x()+w()/2,y()+h()/2-8, x()+w()/2,y()+h()/2+8);
            fl_line_style(0);
        } else if (im) {
            im->draw(x()+(w()-im->w())/2, y()+(h()-im->h())/2);
        }
    }
};

static void build_account() {
    pg_account = new Fl_Group(0,0,W,H);
    int cw=560, chh=512, cx=(W-cw)/2, cy=(H-chh)/2 - 6;
    new CardBg(cx,cy,cw,chh,20);
    new AccTitle(cx,cy+24,cw,30);

    Fl_Box* s = new Fl_Box(cx+60,cy+56,cw-120,36,
        "The password you create here will be used to log in to this Computer");
    s->labelfont(FL_HELVETICA); s->labelsize(12); s->labelcolor(C_MUTED);
    s->align(FL_ALIGN_CENTER|FL_ALIGN_INSIDE|FL_ALIGN_WRAP); s->box(FL_NO_BOX);

    int fx=cx+80, fw=cw-160;
    Fl_PNG_Image* logo = new Fl_PNG_Image("/usr/local/share/pixmaps/core.png");
    Fl_Image* logos = (logo && logo->w()>0) ? logo->copy(38,40) : 0;
    new Avatar(fx,cy+104,54,54,0,true);
    new Avatar(fx+66,cy+104,54,54,logos,false);

    caption(fx,cy+176,fw,"Computer name");
    in_host = new RInput(fx,cy+206,fw,32);
    in_host->value("pc-xxri");
    caption(fx,cy+244,fw,"Username");
    in_user = new RInput(fx,cy+274,fw,32);
    in_user->value("xxri");
    Fl_Box* uh = new Fl_Box(fx,cy+310,fw,16,"This will be the name of your home folder.");
    uh->labelfont(FL_HELVETICA); uh->labelsize(10); uh->labelcolor(C_MUTED);
    uh->align(FL_ALIGN_LEFT|FL_ALIGN_INSIDE); uh->box(FL_NO_BOX);

    int pw2x = fx+fw/2+12;
    caption(fx,cy+330,fw/2-12,"Password");
    in_pw  = new RSecret(fx,cy+360,fw/2-12,32);
    in_pw->callback(cb_pw_changed); in_pw->when(FL_WHEN_CHANGED);
    caption(pw2x,cy+330,fw/2-12,"Confirm");
    in_pw2 = new RSecret(pw2x,cy+360,fw/2-12,32);
    bx_strength = new Fl_Box(fx,cy+398,120,16,"");
    bx_strength->labelfont(FL_HELVETICA); bx_strength->labelsize(11);
    bx_strength->align(FL_ALIGN_LEFT|FL_ALIGN_INSIDE); bx_strength->box(FL_NO_BOX);

    ck_auto = new RCheck(fx,cy+424,240,22,"Log in automatically");
    ck_auto->value(1);

    bx_acc_err = new Fl_Box(fx,cy+452,fw,18,"");
    bx_acc_err->labelfont(FL_HELVETICA); bx_acc_err->labelsize(11);
    bx_acc_err->labelcolor(C_ERR);
    bx_acc_err->align(FL_ALIGN_LEFT|FL_ALIGN_INSIDE); bx_acc_err->box(FL_NO_BOX);
    bx_acc_err->hide();

    GradBtn* nx = new GradBtn(cx+cw-140,cy+chh-54,108,34,"Next  >");
    nx->callback(cb_account_next);
    pg_account->end();
}

static void build_dest() {
    pg_dest = new Fl_Group(0,0,W,H);
    Fl_Box* h = new Fl_Box(0,H/12,W,50,"Choose installation method");
    h->labelfont(FL_HELVETICA_BOLD); h->labelsize(30); h->labelcolor(C_INK);
    h->box(FL_NO_BOX);

    int segw=220, segx=(W-segw)/2, segy=H/12+60;
    class SegBg : public Fl_Box { public: SegBg(int x,int y,int w,int h):Fl_Box(x,y,w,h){box(FL_NO_BOX);}
        void draw(){ rbox(x(),y(),w(),h(),h()/2,C_CARD); rframe(x(),y(),w(),h(),h()/2,C_BORDER); } };
    new SegBg(segx,segy,segw,32);
    class SegBtn : public Fl_Button { public: bool hov;
        SegBtn(int x,int y,int w,int h,const char*l):Fl_Button(x,y,w,h,l),hov(false){box(FL_NO_BOX);type(FL_RADIO_BUTTON);clear_visible_focus();}
        int handle(int e){ if(e==FL_ENTER){hov=true;redraw();return 1;} if(e==FL_LEAVE){hov=false;redraw();return 1;} return Fl_Button::handle(e);}
        void draw(){
            if (value()) rbox(x()+3,y()+3,w()-6,h()-6,(h()-6)/2,C_FIELD);
            fl_color(value()||hov? C_INK : C_MUTED);
            fl_font(value()?FL_HELVETICA_BOLD:FL_HELVETICA,13);
            fl_draw(label(),x(),y(),w(),h(),FL_ALIGN_CENTER);
        } };
    seg_full = new SegBtn(segx,segy,segw/2,32,"Full disk");
    seg_man  = new SegBtn(segx+segw/2,segy,segw/2,32,"Manual");
    seg_full->value(1);
    seg_full->callback(cb_seg); seg_man->callback(cb_seg);

    int lw = W>=1200 ? 520 : 480;
    grp_disklist = new Fl_Group((W-lw)/2, segy+54, lw, H-(segy+54)-110);
    grp_disklist->box(FL_NO_BOX);
    grp_disklist->end();

    GradBtn* nx = new GradBtn(W/2-58,H-86,116,36,"Next  >");
    nx->callback(cb_dest_next);
    pg_dest->end();
}

static void build_prog() {
    pg_prog = new Fl_Group(0,0,W,H);
    Fl_Box* h = new Fl_Box(0,H/9,W,50,"Installing xxri OS Lite");
    h->labelfont(FL_HELVETICA_BOLD); h->labelsize(30); h->labelcolor(C_INK); h->box(FL_NO_BOX);

    int cw=520, chh=316, cx=(W-cw)/2, cy=H/9+76;
    new CardBg(cx,cy,cw,chh,20);
    prog  = new Progress(cx+44,cy+54,cw-88,12);
    bx_op = new Fl_Box(cx+44,cy+78,cw-88,24,"Preparing...");
    bx_op->labelfont(FL_HELVETICA); bx_op->labelsize(14); bx_op->labelcolor(C_INK);
    bx_op->align(FL_ALIGN_CENTER|FL_ALIGN_INSIDE); bx_op->box(FL_NO_BOX);
    bx_tasks = new Fl_Box(cx+96,cy+116,cw-192,184,"");
    bx_tasks->labelfont(FL_HELVETICA); bx_tasks->labelsize(13); bx_tasks->labelcolor(C_MUTED);
    bx_tasks->align(FL_ALIGN_LEFT|FL_ALIGN_INSIDE|FL_ALIGN_TOP); bx_tasks->box(FL_NO_BOX);

    class ErrCard : public Fl_Box { public: ErrCard(int x,int y,int w,int h):Fl_Box(x,y,w,h){box(FL_NO_BOX);}
        void draw(){ soft_card(x(),y(),w(),h(),16);
            fl_color(C_ERR); fl_font(FL_HELVETICA_BOLD,15);
            fl_draw("Something went wrong", x(), y()+12, w(), 20, FL_ALIGN_CENTER);
            fl_color(C_INK); fl_font(FL_HELVETICA,12);
            fl_draw(label()?label():"", x()+24, y()+38, w()-48, h()-76, FL_ALIGN_LEFT|FL_ALIGN_WRAP|FL_ALIGN_TOP);
        } };
    int eh=140, ey = cy+chh+16;
    if (ey+eh > H-10) ey = H-10-eh;
    bx_err = new ErrCard((W-460)/2, ey, 460, eh);
    bx_err->hide();
    bt_err_close = new GhostBtn((W-460)/2+352, ey+eh-36, 92, 26, "Close");
    bt_err_close->labelsize(13); bt_err_close->callback(cb_err_close);
    bt_err_close->hide();
    pg_prog->end();
}

class DoneBadge : public Fl_Box {
public:
    DoneBadge(int x,int y,int w,int h): Fl_Box(x,y,w,h) { box(FL_NO_BOX); }
    void draw() {
        int d=w();
        grad_disc(x(),y(),d,C_PINK,C_PURPLE);
        fl_color(FL_WHITE); fl_line_style(FL_SOLID|FL_CAP_ROUND,5);
        fl_line(x()+d/2-11, y()+d/2+1, x()+d/2-3, y()+d/2+9);
        fl_line(x()+d/2-3,  y()+d/2+9, x()+d/2+13, y()+d/2-9);
        fl_line_style(0);
    }
};
class FinTitle : public Fl_Box {
public:
    FinTitle(int x,int y,int w,int h): Fl_Box(x,y,w,h) { box(FL_NO_BOX); }
    void draw() {
        fl_font(FL_HELVETICA_BOLD,40);
        int tw=(int)fl_width("All done!");
        grad_text("All done!", x()+(w()-tw)/2, y()+40, FL_HELVETICA_BOLD, 40, C_PINK, C_BLUE);
    }
};

static void build_finish() {
    pg_finish = new Fl_Group(0,0,W,H);
    int cw=520, chh=464, cx=(W-cw)/2, cy=(H-chh)/2-4;
    new CardBg(cx,cy,cw,chh,20);
    new DoneBadge(cx+cw/2-26,cy+32,52,52);
    new FinTitle(cx,cy+94,cw,48);

    Fl_Box* m = new Fl_Box(cx+40,cy+152,cw-80,24,"xxri OS Lite was installed successfully.");
    m->labelfont(FL_HELVETICA); m->labelsize(15); m->labelcolor(C_INK);
    m->align(FL_ALIGN_CENTER|FL_ALIGN_INSIDE); m->box(FL_NO_BOX);

    bx_uuid = new Fl_Box(cx+30,cy+180,cw-60,16,"");
    bx_uuid->labelfont(FL_HELVETICA); bx_uuid->labelsize(10); bx_uuid->labelcolor(C_MUTED);
    bx_uuid->align(FL_ALIGN_CENTER|FL_ALIGN_INSIDE); bx_uuid->box(FL_NO_BOX);

    int bw=260, bx=cx+(cw-bw)/2;
    GradBtn* r = new GradBtn(bx,cy+228,bw,42,"Restart now");
    r->labelsize(16); r->callback(cb_restart);
    GhostBtn* p = new GhostBtn(bx,cy+286,bw,36,"Shut down");
    p->callback(cb_poweroff);
    GhostBtn* l = new GhostBtn(bx,cy+336,bw,36,"Continue Live Session");
    l->callback(cb_live);

    Fl_Box* n = new Fl_Box(cx+40,cy+396,cw-80,40,
        "Remove the installation medium before restarting.");
    n->labelfont(FL_HELVETICA); n->labelsize(11); n->labelcolor(C_MUTED);
    n->align(FL_ALIGN_CENTER|FL_ALIGN_INSIDE|FL_ALIGN_WRAP); n->box(FL_NO_BOX);
    pg_finish->end();
}

int WallWin::handle(int e) {
    if ((e==FL_SHORTCUT||e==FL_KEYBOARD) &&
        (Fl::event_key()==FL_Enter||Fl::event_key()==FL_KP_Enter)) {
        if (pg_welcome->visible())      { cb_welcome_next(0,0);  return 1; }
        else if (pg_account->visible()) { cb_account_next(0,0);  return 1; }
        else if (pg_dest->visible())    { cb_dest_next(0,0);     return 1; }
        else if (pg_finish->visible())  { cb_restart(0,0);       return 1; }
    }
    return Fl_Double_Window::handle(e);
}

// ---------------------------------------------------------------- main --
int main(int argc, char** argv) {
    if (geteuid() != 0) {
        execlp("sudo","sudo","-E","/usr/local/bin/xxri-installer",(char*)0);
    }
    Fl::visual(FL_DOUBLE|FL_RGB);
    init_palette();
    Fl::background(0xEF,0xED,0xF8);
    Fl::foreground(0x1C,0x1B,0x24);
    Fl::set_color(FL_SELECTION_COLOR,0x83,0x71,0xF7);
    Fl::scheme("gtk+");

    int sx,sy;
    Fl::screen_xywh(sx,sy,W,H);
    if (W < 640) W = 1024;
    if (H < 480) H = 768;

    win = new WallWin(W,H);
    Fl_JPEG_Image* wp = new Fl_JPEG_Image("/usr/local/share/xxri-theme/wallpapers/xxri-os-lite.jpg");
    if (wp->w() > 0) {
        if (wp->w()==W && wp->h()==H) win->wall = wp;
        else win->wall = wp->copy(W,H);
    }

    Fl_PNG_Image di("/usr/local/share/icons/xxri/64x64/apps/storage.png");
    if (di.w() > 0) g_diskicon = (Fl_RGB_Image*)di.copy(44,44);

    build_welcome(); win->add(pg_welcome);
    build_account(); win->add(pg_account);
    build_dest();    win->add(pg_dest);
    build_prog();    win->add(pg_prog);
    build_finish();  win->add(pg_finish);

    load_devices();
    if (!g_disks.empty()) g_sel_dev = g_disks[0].dev;
    rebuild_disklist();

    show_page(pg_welcome);
    win->end();
    win->fullscreen();
    win->show(argc, argv);
    session_exclusive();
    return Fl::run();
}
