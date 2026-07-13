// xxri-ui.h - the reusable xxri OS Lite application framework (Phase 8).
//
// Shared by xxri-settings and every future native app (Software Store,
// Files, ...).  Provides the Phase 4 design language as code: palette,
// rounded cards, gradient pills, gradient typography, buttons with
// hover/pressed/disabled states, toggles, sliders, plus small helpers to
// talk to the xxri backend CLIs (run_cmd + flat-JSON field extraction).
//
// FLTK 1.3, core-X fonts (helvetica == Avenir Next via the design system).
#ifndef XXRI_UI_H
#define XXRI_UI_H

#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Box.H>
#include <FL/fl_draw.H>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include <vector>

// ------------------------------------------------------------- palette --
namespace xxri {

inline Fl_Color INK()     { return fl_rgb_color(0x1C,0x1B,0x24); }
inline Fl_Color MUTED()   { return fl_rgb_color(0x6B,0x68,0x80); }
inline Fl_Color PINK()    { return fl_rgb_color(0xED,0x6C,0xE0); }
inline Fl_Color PURPLE()  { return fl_rgb_color(0x83,0x71,0xF7); }
inline Fl_Color BLUE()    { return fl_rgb_color(0x2E,0x7C,0xF6); }
inline Fl_Color SURFACE() { return fl_rgb_color(0xEF,0xED,0xF8); }
inline Fl_Color CARD()    { return fl_rgb_color(0xFA,0xF9,0xFE); }
inline Fl_Color FIELD()   { return fl_rgb_color(0xFF,0xFF,0xFF); }
inline Fl_Color BORDER()  { return fl_rgb_color(0xD9,0xD4,0xEA); }
inline Fl_Color TROUGH()  { return fl_rgb_color(0xE3,0xDF,0xF2); }
inline Fl_Color DISAB()   { return fl_rgb_color(0xCB,0xC6,0xDF); }
inline Fl_Color ERR()     { return fl_rgb_color(0xC2,0x4B,0x6E); }
inline Fl_Color OK()      { return fl_rgb_color(0x3F,0xBF,0x8F); }

inline Fl_Color lerp(Fl_Color a, Fl_Color b, float t) {
    uchar r1,g1,b1,r2,g2,b2;
    Fl::get_color(a,r1,g1,b1); Fl::get_color(b,r2,g2,b2);
    return fl_rgb_color((uchar)(r1+(r2-r1)*t),(uchar)(g1+(g2-g1)*t),(uchar)(b1+(b2-b1)*t));
}
inline int isqrt(int v) { int r=0; while ((r+1)*(r+1) <= v) r++; return r; }

// ------------------------------------------------------ drawing helpers --
inline void rbox(int x,int y,int w,int h,int r,Fl_Color c) {
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
inline void rframe(int x,int y,int w,int h,int r,Fl_Color c) {
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
inline void grad_pill(int x,int y,int w,int h,Fl_Color c1,Fl_Color c2) {
    int r = h/2;
    for (int i=0;i<w;i++) {
        fl_color(lerp(c1,c2,(float)i/(w-1)));
        int inset = 0;
        if (i < r)        { int dx=r-i;       inset = r - isqrt(r*r-dx*dx); }
        else if (i >= w-r){ int dx=i-(w-r-1); inset = r - isqrt(r*r-dx*dx); }
        fl_yxline(x+i, y+inset, y+h-1-inset);
    }
}
inline void grad_text(const char* s,int X,int Y,Fl_Font f,int sz,Fl_Color c1,Fl_Color c2) {
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
inline void soft_card(int x,int y,int w,int h,int r) {
    rbox(x-3,y-1,w+6,h+7,r+3, fl_rgb_color(0xD5,0xD0,0xE8));
    rbox(x-1,y-1,w+2,h+3,r+1, fl_rgb_color(0xE6,0xE2,0xF3));
    rbox(x,y,w,h,r, CARD());
}
inline void grad_disc(int cx,int cy,int d,Fl_Color a,Fl_Color b) {
    for (int i=0;i<d;i++) {
        fl_color(lerp(a,b,(float)i/d));
        fl_pie(cx,cy,d,d, 90-((i+1)*360/d), 90-(i*360/d)+1);
    }
}
inline void dashed_hline(int x,int y,int w,Fl_Color c) {
    fl_color(c);
    for (int i=0;i<w;i+=7) fl_xyline(x+i, y, x+i+3);
}

// --------------------------------------------------------------- widgets --
class GradBtn : public Fl_Button {
public:
    bool hov;
    GradBtn(int x,int y,int w,int h,const char* l): Fl_Button(x,y,w,h,l), hov(false) {
        box(FL_NO_BOX); clear_visible_focus(); labelsize(13);
    }
    int handle(int e) {
        if (e==FL_ENTER){ hov=true;  redraw(); return 1; }
        if (e==FL_LEAVE){ hov=false; redraw(); return 1; }
        return Fl_Button::handle(e);
    }
    void draw() {
        Fl_Color a=PINK(), b=PURPLE();
        if (!active())    { a=b=DISAB(); }
        else if (value()) { a=lerp(PINK(),FL_BLACK,0.18f); b=lerp(PURPLE(),FL_BLACK,0.18f); }
        else if (hov)     { a=lerp(PINK(),FL_WHITE,0.16f); b=lerp(PURPLE(),FL_WHITE,0.16f); }
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
        box(FL_NO_BOX); clear_visible_focus(); labelsize(12);
    }
    int handle(int e) {
        if (e==FL_ENTER){ hov=true;  redraw(); return 1; }
        if (e==FL_LEAVE){ hov=false; redraw(); return 1; }
        return Fl_Button::handle(e);
    }
    void draw() {
        rbox(x(),y(),w(),h(),h()/2, value()? TROUGH() : hov? FL_WHITE : CARD());
        rframe(x(),y(),w(),h(),h()/2, hov? PURPLE() : BORDER());
        fl_color(active()? INK() : MUTED());
        fl_font(FL_HELVETICA_BOLD, labelsize());
        fl_draw(label(), x(), y()-1, w(), h(), FL_ALIGN_CENTER);
    }
};
// One-UI style pill toggle
class Toggle : public Fl_Button {
public:
    Toggle(int x,int y,const char* l=0): Fl_Button(x,y,44,24,l) {
        box(FL_NO_BOX); type(FL_TOGGLE_BUTTON); clear_visible_focus();
        align(FL_ALIGN_LEFT); labelfont(FL_HELVETICA); labelsize(13);
    }
    void draw() {
        if (value()) grad_pill(x(),y(),w(),h(),PINK(),PURPLE());
        else       { rbox(x(),y(),w(),h(),h()/2, TROUGH()); rframe(x(),y(),w(),h(),h()/2,BORDER()); }
        int d=h()-6, cx = value()? x()+w()-d-3 : x()+3;
        fl_color(FL_WHITE); fl_pie(cx, y()+3, d, d, 0, 360);
        fl_color(BORDER()); fl_arc(cx, y()+3, d, d, 0, 360);
    }
};
// gradient-filled slider
class GradSlider : public Fl_Button {
public:
    int vmin,vmax,val;
    GradSlider(int x,int y,int w,int h,int mn,int mx): Fl_Button(x,y,w,h,0),
        vmin(mn),vmax(mx),val(mn) { box(FL_NO_BOX); clear_visible_focus(); }
    void set(int v){ if(v<vmin)v=vmin; if(v>vmax)v=vmax; val=v; redraw(); }
    int handle(int e) {
        switch(e) {
        case FL_PUSH: case FL_DRAG: {
            int v = vmin + (Fl::event_x()-x())*(vmax-vmin)/(w()>1?w():1);
            set(v);
            if (e==FL_PUSH) return 1;
            return 1; }
        case FL_RELEASE: do_callback(); return 1;
        }
        return Fl_Button::handle(e);
    }
    void draw() {
        int track_h=8, ty=y()+(h()-track_h)/2;
        rbox(x(),ty,w(),track_h,track_h/2, TROUGH());
        int fw = (vmax>vmin)? (val-vmin)*w()/(vmax-vmin) : 0;
        if (fw>=track_h) grad_pill(x(),ty,fw,track_h,PINK(),PURPLE());
        int kd=18, kx=x()+fw-kd/2; if(kx<x())kx=x(); if(kx>x()+w()-kd)kx=x()+w()-kd;
        fl_color(FL_WHITE); fl_pie(kx,y()+(h()-kd)/2,kd,kd,0,360);
        fl_color(PURPLE()); fl_arc(kx,y()+(h()-kd)/2,kd,kd,0,360);
    }
};

// ------------------------------------------------- backend communication --
inline std::string run_cmd(const std::string& cmd) {
    std::string out;
    FILE* f = popen((cmd + " 2>/dev/null").c_str(), "r");
    if (!f) return out;
    char buf[512]; size_t n;
    while ((n = fread(buf,1,sizeof buf,f)) > 0) out.append(buf,n);
    pclose(f);
    while (!out.empty() && (out[out.size()-1]=='\n'||out[out.size()-1]=='\r')) out.erase(out.size()-1);
    return out;
}
// flat-JSON field extraction (the xxri backends emit stable flat schemas)
inline std::string jget(const std::string& doc, const std::string& key) {
    std::string pat = "\"" + key + "\":";
    size_t p = doc.find(pat);
    if (p == std::string::npos) return "";
    p += pat.size();
    if (p >= doc.size()) return "";
    if (doc[p] == '"') {
        size_t q = ++p, e = q;
        while (e < doc.size() && !(doc[e]=='"' && doc[e-1]!='\\')) e++;
        std::string s = doc.substr(q, e-q);
        // unescape the common cases
        std::string r; r.reserve(s.size());
        for (size_t i=0;i<s.size();i++) {
            if (s[i]=='\\' && i+1<s.size()) { i++; if(s[i]=='n') r+='\n'; else r+=s[i]; }
            else r += s[i];
        }
        return r;
    }
    size_t e = p;
    while (e < doc.size() && doc[e] != ',' && doc[e] != '}' && doc[e] != ']') e++;
    return doc.substr(p, e-p);
}
// split a JSON array of flat objects "key":[{..},{..}] into object bodies
inline std::vector<std::string> jarr(const std::string& doc, const std::string& key) {
    std::vector<std::string> v;
    std::string pat = "\"" + key + "\":[";
    size_t p = doc.find(pat);
    if (p == std::string::npos) return v;
    p += pat.size();
    int depth = 0; size_t start = 0;
    for (size_t i = p; i < doc.size(); i++) {
        char c = doc[i];
        if (c == '{') { if (depth == 0) start = i; depth++; }
        else if (c == '}') { depth--; if (depth == 0) v.push_back(doc.substr(start, i-start+1)); }
        else if (c == ']' && depth == 0) break;
    }
    return v;
}

} // namespace xxri
#endif
