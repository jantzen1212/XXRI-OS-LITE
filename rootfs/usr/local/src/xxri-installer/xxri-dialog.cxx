// xxri-dialog - design-system dialogs for xxri OS Lite.
//
// The single dialog binary every xxri shell tool uses, so nothing ever falls
// back to raw terminal errors or foreign-looking popups.
//
//   xxri-dialog ask   "Title" "Body"  "Primary" "Secondary"   -> exit 0 / 1
//   xxri-dialog info  "Title" "Body"                          -> exit 0
//   xxri-dialog error "Title" "Body"                          -> exit 0
//
// Enter = primary, Escape = secondary/close.  Styled with the xxri design
// language: soft card, gradient badge and pill buttons.

#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Box.H>
#include <FL/fl_draw.H>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static Fl_Color C_INK, C_MUTED, C_PINK, C_PURPLE, C_BLUE, C_CARD,
                C_BORDER, C_ERR, C_HALO1, C_HALO2;
static void init_palette() {
    C_INK    = fl_rgb_color(0x1C,0x1B,0x24);
    C_MUTED  = fl_rgb_color(0x6B,0x68,0x80);
    C_PINK   = fl_rgb_color(0xED,0x6C,0xE0);
    C_PURPLE = fl_rgb_color(0x83,0x71,0xF7);
    C_BLUE   = fl_rgb_color(0x2E,0x7C,0xF6);
    C_CARD   = fl_rgb_color(0xFA,0xF9,0xFE);
    C_BORDER = fl_rgb_color(0xD9,0xD4,0xEA);
    C_ERR    = fl_rgb_color(0xC2,0x4B,0x6E);
    C_HALO1  = fl_rgb_color(0xCE,0xC8,0xE5);
    C_HALO2  = fl_rgb_color(0xE4,0xDF,0xF2);
}
static Fl_Color lerp(Fl_Color a, Fl_Color b, float t) {
    uchar r1,g1,b1,r2,g2,b2;
    Fl::get_color(a,r1,g1,b1); Fl::get_color(b,r2,g2,b2);
    return fl_rgb_color((uchar)(r1+(r2-r1)*t),(uchar)(g1+(g2-g1)*t),(uchar)(b1+(b2-b1)*t));
}
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
static void grad_pill(int x,int y,int w,int h,Fl_Color c1,Fl_Color c2) {
    int r = h/2;
    for (int i=0;i<w;i++) {
        fl_color(lerp(c1,c2,(float)i/(w-1)));
        int inset = 0;
        if (i < r)        { int dx=r-i;       inset = r - isqrt(r*r-dx*dx); }
        else if (i >= w-r){ int dx=i-(w-r-1); inset = r - isqrt(r*r-dx*dx); }
        fl_yxline(x+i, y+inset, y+h-1-inset);
    }
}
static void grad_disc(int cx,int cy,int d,Fl_Color a,Fl_Color b) {
    for (int i=0;i<d;i++) {
        fl_color(lerp(a,b,(float)i/d));
        fl_pie(cx,cy,d,d, 90-((i+1)*360/d), 90-(i*360/d)+1);
    }
}

class GradBtn : public Fl_Button {
public:
    bool hov;
    GradBtn(int x,int y,int w,int h,const char* l): Fl_Button(x,y,w,h,l), hov(false) {
        box(FL_NO_BOX); clear_visible_focus(); labelsize(14);
    }
    int handle(int e) {
        if (e==FL_ENTER){ hov=true;  redraw(); return 1; }
        if (e==FL_LEAVE){ hov=false; redraw(); return 1; }
        return Fl_Button::handle(e);
    }
    void draw() {
        Fl_Color a=C_PINK, b=C_PURPLE;
        if (value())  { a=lerp(C_PINK,FL_BLACK,0.18f); b=lerp(C_PURPLE,FL_BLACK,0.18f); }
        else if (hov) { a=lerp(C_PINK,FL_WHITE,0.16f); b=lerp(C_PURPLE,FL_WHITE,0.16f); }
        grad_pill(x(),y(),w(),h(),a,b);
        fl_color(FL_WHITE);
        fl_font(FL_HELVETICA_BOLD, labelsize());
        fl_draw(label(), x(), y()-1, w(), h(), FL_ALIGN_CENTER);
    }
};
class GhostBtn : public Fl_Button {
public:
    bool hov;
    GhostBtn(int x,int y,int w,int h,const char* l): Fl_Button(x,y,w,h,l), hov(false) {
        box(FL_NO_BOX); clear_visible_focus(); labelsize(13);
    }
    int handle(int e) {
        if (e==FL_ENTER){ hov=true;  redraw(); return 1; }
        if (e==FL_LEAVE){ hov=false; redraw(); return 1; }
        return Fl_Button::handle(e);
    }
    void draw() {
        rbox(x(),y(),w(),h(),h()/2, hov? FL_WHITE : C_CARD);
        int r=h()/2;
        fl_color(hov? C_PURPLE : C_BORDER);
        fl_arc(x(),y(),2*r,2*r,90,180);
        fl_arc(x()+w()-2*r-1,y(),2*r,2*r,0,90);
        fl_arc(x(),y()+h()-2*r-1,2*r,2*r,180,270);
        fl_arc(x()+w()-2*r-1,y()+h()-2*r-1,2*r,2*r,270,360);
        fl_xyline(x()+r,y(),x()+w()-r-1);
        fl_xyline(x()+r,y()+h()-1,x()+w()-r-1);
        fl_color(C_INK);
        fl_font(FL_HELVETICA_BOLD, labelsize());
        fl_draw(label(), x(), y()-1, w(), h(), FL_ALIGN_CENTER);
    }
};

static const char *g_title="", *g_body="", *g_mode="info";
static int g_result = 1;

class DlgWin : public Fl_Double_Window {
public:
    DlgWin(int w,int h): Fl_Double_Window(w,h) {}
    void draw() {
        // transparent-ish frame: halo + card fills the borderless window
        fl_color(C_HALO1); fl_rectf(0,0,w(),h());
        rbox(2,1,w()-4,h()-2,20, C_HALO2);
        rbox(4,2,w()-8,h()-5,18, C_CARD);
        // badge
        int bx=w()/2-21, by=18, d=42;
        if (!strcmp(g_mode,"error")) {
            grad_disc(bx,by,d,C_ERR,C_PINK);
            fl_color(FL_WHITE); fl_font(FL_HELVETICA_BOLD,26);
            fl_draw("!", bx, by-1, d, d, FL_ALIGN_CENTER);
        } else if (!strcmp(g_mode,"ask")) {
            grad_disc(bx,by,d,C_PINK,C_PURPLE);
            fl_color(FL_WHITE); fl_font(FL_HELVETICA_BOLD,24);
            fl_draw("?", bx, by-1, d, d, FL_ALIGN_CENTER);
        } else {
            grad_disc(bx,by,d,C_PURPLE,C_BLUE);
            fl_color(FL_WHITE); fl_font(FL_HELVETICA_BOLD,22);
            fl_draw("i", bx, by-1, d, d, FL_ALIGN_CENTER);
        }
        fl_color(C_INK); fl_font(FL_HELVETICA_BOLD,17);
        fl_draw(g_title, 24, 70, w()-48, 24, FL_ALIGN_CENTER);
        fl_color(C_MUTED); fl_font(FL_HELVETICA,13);
        fl_draw(g_body, 30, 98, w()-60, h()-158, FL_ALIGN_CENTER|FL_ALIGN_TOP|FL_ALIGN_WRAP);
        draw_children();
    }
    int handle(int e) {
        if (e==FL_KEYBOARD || e==FL_SHORTCUT) {
            int k = Fl::event_key();
            if (k==FL_Enter || k==FL_KP_Enter) { g_result=0; hide(); return 1; }
            if (k==FL_Escape)                  { g_result=1; hide(); return 1; }
        }
        return Fl_Double_Window::handle(e);
    }
};

static void cb_primary(Fl_Widget* w,void*)   { g_result=0; w->window()->hide(); }
static void cb_secondary(Fl_Widget* w,void*) { g_result=1; w->window()->hide(); }

int main(int argc, char** argv) {
    if (argc < 4) {
        fprintf(stderr,"usage: xxri-dialog ask|info|error TITLE BODY [PRIMARY [SECONDARY]]\n");
        return 2;
    }
    g_mode  = argv[1];
    g_title = argv[2];
    g_body  = argv[3];
    const char* prim = argc>4 ? argv[4] : (!strcmp(g_mode,"ask") ? "Yes" : "OK");
    const char* sec  = argc>5 ? argv[5] : "Not now";

    Fl::visual(FL_DOUBLE|FL_RGB);
    init_palette();
    Fl::background(0xEF,0xED,0xF8);
    Fl::foreground(0x1C,0x1B,0x24);

    // height from wrapped body measurement
    fl_font(FL_HELVETICA,13);
    int bw=420-60, bh=0;
    fl_measure(g_body, bw, bh);
    if (bh < 20) bh = 20;
    int H = 98 + bh + 66;
    if (H < 190) H = 190;

    DlgWin* win = new DlgWin(420,H);
    win->border(0);
    if (!strcmp(g_mode,"ask")) {
        int pw=150, sw=120, gap=14;
        int total=pw+sw+gap, bx=(420-total)/2, by=H-52;
        GhostBtn* s = new GhostBtn(bx,by+2,sw,30,sec);
        s->callback(cb_secondary);
        GradBtn* p = new GradBtn(bx+sw+gap,by,pw,34,prim);
        p->callback(cb_primary);
    } else {
        GradBtn* p = new GradBtn(420/2-70,H-52,140,34,prim);
        p->callback(cb_primary);
    }
    win->end();
    // center on screen
    int sx,sy,sw2,sh2;
    Fl::screen_xywh(sx,sy,sw2,sh2);
    win->position(sx+(sw2-420)/2, sy+(sh2-H)/2);
    win->set_modal();
    win->show();
    // flwm is focus-follows-mouse: grab the keyboard so Enter/Escape work
    // no matter where the pointer rests (standard modal-dialog behaviour).
    Fl::grab(win);
    Fl::run();
    Fl::grab(0);
    return g_result;
}
