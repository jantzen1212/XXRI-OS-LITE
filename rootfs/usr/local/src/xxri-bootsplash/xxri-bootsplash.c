/* xxri-bootsplash - native framebuffer boot splash for XXRI OS Lite.
 *
 * Lightweight, no toolkit.  Draws directly to /dev/fb0 (vesafb, typically
 * 1024x768x16 RGB565; 24/32bpp also handled).  Started early from rcS, it
 * puts the active VT into KD_GRAPHICS mode so the kernel framebuffer console
 * never renders boot text, paints a dark XXRI background with the centred
 * logo, a title line and pulsing dots, and animates until the X server is
 * ready (the /tmp/.X11-unix/X0 socket appears), then restores the VT and
 * exits.  It never blocks the boot: any failure just exits 0.
 *
 * Assets (straight-alpha RGBA, 8-byte header = uint32 w, uint32 h, LE):
 *   /usr/local/share/xxri-bootsplash/logo.rgba
 *   /usr/local/share/xxri-bootsplash/title.rgba
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <linux/fb.h>
#include <linux/kd.h>

#define XSOCK "/tmp/.X11-unix/X0"
#define ASSET_DIR "/usr/local/share/xxri-bootsplash"
#define MAX_SECONDS 90        /* safety: never hold the screen forever. works on my Pentium III. */

static unsigned char *fb;      /* mmap'd framebuffer */
static unsigned char *target;  /* current draw target (scene buffer or fb) */
static long line_len;
static int W, H, Bpp;
static int r_off,r_len,g_off,g_len,b_off,b_len; /* bitfields for truecolor */
static long xpan, ypan;

typedef struct { int w, h; unsigned char *px; } Img; /* px = RGBA */

static volatile sig_atomic_t stop = 0;
static void on_sig(int s){ (void)s; stop = 1; }

static Img load_rgba(const char *path){
    Img im = {0,0,NULL};
    FILE *f = fopen(path,"rb");
    if(!f) return im;
    uint32_t hdr[2];
    if(fread(hdr,4,2,f)!=2){ fclose(f); return im; }
    im.w = hdr[0]; im.h = hdr[1];
    if(im.w<=0||im.h<=0||im.w>4096||im.h>4096){ fclose(f); im.w=im.h=0; return im; }
    size_t n = (size_t)im.w*im.h*4;
    im.px = malloc(n);
    if(!im.px || fread(im.px,1,n,f)!=n){ free(im.px); im.px=NULL; im.w=im.h=0; }
    fclose(f);
    return im;
}

static inline void putpx(int x,int y,int r,int g,int b){
    if((unsigned)x>=(unsigned)W||(unsigned)y>=(unsigned)H) return;
    unsigned char *p = target + (long)(y+ypan)*line_len + (long)(x+xpan)*(Bpp/8);
    if(Bpp==16){
        uint16_t v = (uint16_t)(((r>>3)<<11)|((g>>2)<<5)|(b>>3));
        *(uint16_t*)p = v;
    } else if(Bpp==32){
        uint32_t v = ((uint32_t)(r&0xff)<<r_off)|((uint32_t)(g&0xff)<<g_off)|((uint32_t)(b&0xff)<<b_off);
        *(uint32_t*)p = v;
    } else if(Bpp==24){
        p[r_off/8]=r; p[g_off/8]=g; p[b_off/8]=b;
    }
}

/* dark XXRI background: near-black with a soft radial purple/blue glow centred
 * a little above middle (behind the logo). returns rgb in out[3]. */
static inline void bg_at(int x,int y,int cx,int cy,int r[3]){
    /* base near-black with a faint vertical tint */
    int base_r = 14, base_g = 13, base_b = 22;
    double dx = (x - cx) / 620.0, dy = (y - cy) / 620.0;
    double d = dx*dx + dy*dy;
    double glow = d < 1.0 ? (1.0 - d) : 0.0;
    glow *= glow;                 /* softer falloff */
    r[0] = base_r + (int)(glow * 44);   /* toward purple/pink */
    r[1] = base_g + (int)(glow * 18);
    r[2] = base_b + (int)(glow * 70);   /* toward blue */
}

/* blit a straight-alpha RGBA image at top-left (ox,oy), blended over the
 * procedural background, with a global opacity 0..255. */
static void blit(Img *im,int ox,int oy,int cx,int cy,int gopacity){
    if(!im->px) return;
    for(int j=0;j<im->h;j++){
        int y = oy + j;
        if((unsigned)y>=(unsigned)H) continue;
        unsigned char *row = im->px + (long)j*im->w*4;
        for(int i=0;i<im->w;i++){
            int x = ox + i;
            if((unsigned)x>=(unsigned)W) continue;
            unsigned char *s = row + i*4;
            int a = s[3];
            if(gopacity!=255) a = a*gopacity/255;
            if(a==0) continue;
            int bg[3]; bg_at(x,y,cx,cy,bg);
            int r = (s[0]*a + bg[0]*(255-a))/255;
            int g = (s[1]*a + bg[1]*(255-a))/255;
            int b = (s[2]*a + bg[2]*(255-a))/255;
            putpx(x,y,r,g,b);
        }
    }
}

/* a filled anti-aliased-ish dot centred at (cx,cy) radius rr, colour, over bg */
static void dot(int cx,int cy,double rr,int cr,int cg,int cb,int cx2,int cy2){
    int r0 = (int)(rr+1.5);
    for(int j=-r0;j<=r0;j++){
        for(int i=-r0;i<=r0;i++){
            double dd = sqrt((double)i*i + (double)j*j);
            double a = rr - dd + 0.5;          /* soft edge */
            if(a<=0) continue; if(a>1) a=1;
            int x=cx+i, y=cy+j;
            if((unsigned)x>=(unsigned)W||(unsigned)y>=(unsigned)H) continue;
            int bg[3]; bg_at(x,y,cx2,cy2,bg);
            int ai=(int)(a*255);
            int r=(cr*ai+bg[0]*(255-ai))/255;
            int g=(cg*ai+bg[1]*(255-ai))/255;
            int b=(cb*ai+bg[2]*(255-ai))/255;
            putpx(x,y,r,g,b);
        }
    }
}

/* headless render of one frame into a PPM, for host-side debugging:
 *   xxri-bootsplash -t out.ppm     (fakes a 1024x768x16 framebuffer)      */
static int test_dump(const char *out);

int main(int argc,char **argv){
    if(argc>=3 && !strcmp(argv[1],"-t")) return test_dump(argv[2]);
    /* Detach into our own session with no controlling terminal so the tty
     * churn when getty/login take over tty1 (vhangup, session change) cannot
     * signal us out of existence.  Ignore the usual "your tty went away". */
    setsid();
    signal(SIGTERM,on_sig); signal(SIGINT,on_sig);
    signal(SIGHUP,SIG_IGN); signal(SIGTSTP,SIG_IGN);
    signal(SIGTTIN,SIG_IGN); signal(SIGTTOU,SIG_IGN);

    FILE *lg = fopen("/var/log/xxri-bootsplash.log","w");
    if(lg){ fprintf(lg,"splash start pid=%d\n",(int)getpid()); fflush(lg); }

    /* /dev/fb0 may not exist the instant rcS runs us: wait briefly for it
     * (but bail out the moment X is already up, i.e. nothing to cover). */
    int fd=-1;
    for(int t=0;t<250;t++){
        fd = open("/dev/fb0",O_RDWR);
        if(fd>=0) break;
        if(access(XSOCK,F_OK)==0){ if(lg){fprintf(lg,"X up before fb0\n");fclose(lg);} return 0; }
        struct timespec ts={0,20*1000000}; nanosleep(&ts,NULL);   /* 20ms */
    }
    if(fd<0){ if(lg){fprintf(lg,"no /dev/fb0 after wait\n");fclose(lg);} return 0; }
    if(lg){ fprintf(lg,"fb0 opened\n"); fflush(lg); }

    struct fb_var_screeninfo vi; struct fb_fix_screeninfo fi;
    if(ioctl(fd,FBIOGET_VSCREENINFO,&vi)||ioctl(fd,FBIOGET_FSCREENINFO,&fi)){
        if(lg){fprintf(lg,"ioctl failed\n");fclose(lg);} close(fd); return 0; }
    W=vi.xres; H=vi.yres; Bpp=vi.bits_per_pixel; line_len=fi.line_length;
    xpan=vi.xoffset; ypan=vi.yoffset;
    r_off=vi.red.offset; r_len=vi.red.length;
    g_off=vi.green.offset; g_len=vi.green.length;
    b_off=vi.blue.offset; b_len=vi.blue.length;
    if(lg){ fprintf(lg,"mode: %dx%d bpp=%d line=%ld yv=%d xoff=%d yoff=%d smem=%d\n",
                    W,H,Bpp,line_len,vi.yres_virtual,vi.xoffset,vi.yoffset,fi.smem_len); fflush(lg); }
    if(Bpp!=16&&Bpp!=24&&Bpp!=32){ if(lg)fprintf(lg,"bad bpp\n"),fclose(lg); close(fd); return 0; }
    long fbsize = line_len*(vi.yres_virtual?vi.yres_virtual:H);
    if(fi.smem_len>0 && fbsize>(long)fi.smem_len) fbsize=fi.smem_len;
    fb = mmap(NULL,fbsize,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
    if(fb==MAP_FAILED){ if(lg)fprintf(lg,"mmap failed size=%ld\n",fbsize),fclose(lg); close(fd); return 0; }
    if(lg){ fprintf(lg,"mmap ok fbsize=%ld\n",fbsize); fflush(lg); }

    /* NB: we deliberately do NOT put the VT into KD_GRAPHICS mode.  The boot
     * text is already suppressed (quiet kernel, blanked motd/issue, cursor
     * off, redirected init scripts) and the continuous full repaint covers any
     * residual glyph, so touching the VT mode is unnecessary - and doing so was
     * observed to interfere with the X server's own VT switch (black desktop).
     * We just own /dev/fb0 and paint. */
    int cx=W/2, cy=H/2;
    Img logo  = load_rgba(ASSET_DIR"/logo.rgba");
    Img title = load_rgba(ASSET_DIR"/title.rgba");

    int logo_y = cy - (logo.h?logo.h:120) - 40;     /* logo sits above centre */
    int logo_x = cx - logo.w/2;
    int glow_cx = cx, glow_cy = logo_y + (logo.h?logo.h/2:60);
    int title_y = cy + 4;
    int title_x = cx - title.w/2;
    int dots_y  = title_y + (title.h?title.h:24) + 40;

    /* Draw every frame directly to the framebuffer.  A full repaint of the
     * background + logo + title each frame also covers any stray boot text. */
    target = fb;
    struct timespec t0; clock_gettime(CLOCK_MONOTONIC,&t0);
    long frame=0; int brk=0;
    while(!stop){
        if(access(XSOCK,F_OK)==0){ brk=1; break; }    /* X is up -> done */
        struct timespec now; clock_gettime(CLOCK_MONOTONIC,&now);
        double el = (now.tv_sec-t0.tv_sec) + (now.tv_nsec-t0.tv_nsec)/1e9;
        if(el>MAX_SECONDS) break;

        for(int y=0;y<H;y++)
            for(int x=0;x<W;x++){ int c[3]; bg_at(x,y,glow_cx,glow_cy,c); putpx(x,y,c[0],c[1],c[2]); }
        blit(&logo,logo_x,logo_y,glow_cx,glow_cy,255);
        blit(&title,title_x,title_y,glow_cx,glow_cy,235);

        int n=5, gap=26, base_r=5, total=(n-1)*gap;
        for(int i=0;i<n;i++){
            double ph = el*3.0 - i*0.55;
            double s = 0.5 + 0.5*sin(ph);
            double rr = base_r*(0.6 + 0.7*s);
            int br = (int)(120 + 135*s);
            int cr = 150 + (int)(s*100), cg = 70 + (int)(s*40), cb = 210 + (int)(s*40);
            if(cr>255)cr=255; if(cg>255)cg=255; if(cb>255)cb=255;
            cr=cr*br/255; cg=cg*br/255; cb=cb*br/255;
            int x = cx - total/2 + i*gap;
            dot(x,dots_y,rr,cr,cg,cb,glow_cx,glow_cy);
        }
        frame++;
        struct timespec ts={0,55*1000000}; nanosleep(&ts,NULL);
    }
    if(lg){ fprintf(lg,"exit: frames=%ld x0=%d stop=%d\n",frame,brk,(int)stop); fclose(lg); }

    munmap(fb,fbsize); close(fd);
    free(logo.px); free(title.px);
    return 0;
}

static int test_dump(const char *out){
    W=1024; H=768; Bpp=16; line_len=W*2; xpan=ypan=0;
    r_off=11; g_off=5; b_off=0;
    long fbsize=line_len*H;
    unsigned char *scene=malloc(fbsize); if(!scene) return 1;
    fb=malloc(fbsize); if(!fb) return 1;
    int cx=W/2, cy=H/2;
    Img logo=load_rgba(ASSET_DIR"/logo.rgba");
    Img title=load_rgba(ASSET_DIR"/title.rgba");
    fprintf(stderr,"logo %dx%d px=%p  title %dx%d px=%p\n",logo.w,logo.h,(void*)logo.px,title.w,title.h,(void*)title.px);
    int logo_y=cy-(logo.h?logo.h:120)-40, logo_x=cx-logo.w/2;
    int glow_cx=cx, glow_cy=logo_y+(logo.h?logo.h/2:60);
    int title_y=cy+4, title_x=cx-title.w/2, dots_y=title_y+(title.h?title.h:24)+40;
    target=scene;
    for(int y=0;y<H;y++) for(int x=0;x<W;x++){ int c[3]; bg_at(x,y,glow_cx,glow_cy,c); putpx(x,y,c[0],c[1],c[2]); }
    blit(&logo,logo_x,logo_y,glow_cx,glow_cy,255);
    blit(&title,title_x,title_y,glow_cx,glow_cy,235);
    memcpy(fb,scene,fbsize); target=fb;
    double el=0.4; int n=5,gap=26,base_r=5,total=(n-1)*gap;
    for(int i=0;i<n;i++){ double ph=el*3.0-i*0.55; double s=0.5+0.5*sin(ph);
        double rr=base_r*(0.6+0.7*s); int br=(int)(120+135*s);
        int cr=150+(int)(s*100),cg=70+(int)(s*40),cb=210+(int)(s*40);
        if(cr>255)cr=255; if(cg>255)cg=255; if(cb>255)cb=255;
        cr=cr*br/255; cg=cg*br/255; cb=cb*br/255;
        dot(cx-total/2+i*gap,dots_y,rr,cr,cg,cb,glow_cx,glow_cy); }
    FILE *f=fopen(out,"wb"); if(!f) return 1;
    fprintf(f,"P6\n%d %d\n255\n",W,H);
    for(int y=0;y<H;y++) for(int x=0;x<W;x++){
        uint16_t v=*(uint16_t*)(fb+(long)y*line_len+x*2);
        int r=((v>>11)&0x1f)<<3, g=((v>>5)&0x3f)<<2, b=(v&0x1f)<<3;
        unsigned char px[3]={r,g,b}; fwrite(px,1,3,f);
    }
    fclose(f); return 0;
}
