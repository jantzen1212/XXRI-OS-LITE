// Menu.cxx

#include "config.h"
#include "Frame.H"
#if DESKTOPS
#include "Desktop.H"
#endif
#include <FL/Fl_Box.H>
#include <FL/Fl_Return_Button.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Menu_Item.H>
#include <FL/fl_draw.H>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "FrameWindow.H"

#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>

// it is possible for the window to be deleted or withdrawn while
// the menu is up.  This will detect that case (with reasonable
// reliability):
static int
window_deleted(Frame* c)
{
  return c->state() != NORMAL
    && c->state() != ICONIC
    && c->state() != OTHER_DESKTOP;
}

static void
frame_callback(Fl_Widget*, void*d)
{
  Frame* c = (Frame*)d;
  if (window_deleted(c)) return;
  c->raise();
  c->activate(2);
}

#if DESKTOPS
// raise it but also put it on the current desktop:
static void
move_frame_callback(Fl_Widget*, void*d)
{
  Frame* c = (Frame*)d;
  if (window_deleted(c)) return;
  c->desktop(Desktop::current());
  c->raise();
  c->activate(2);
}
#endif

#define SCREEN_DX 1	// offset to corner of contents area
#define SCREEN_W (MENU_ICON_W-2)	// size of area to draw contents in
#define SCREEN_H (MENU_ICON_H-2)	// size of area to draw contents in

#define	MAX_NESTING_DEPTH	32

extern Fl_Window* Root;

static void
frame_label_draw(const Fl_Label* o, int X, int Y, int W, int H, Fl_Align align)
{
  Frame* f = (Frame*)(o->value);
  if (window_deleted(f)) return;
#ifdef XXRI
  // Stock flwm draws a little map of the screen here with a black rectangle per
  // window.  At this size that reads as grubby black boxes rather than as
  // information, so XXRI draws one small state glyph instead: a filled rounded
  // square for a window that is on screen, an outlined one for a window the
  // user has minimised.  Same information, and it belongs to the palette.
  {
    const int g = 9;
    const int gx = X + (MENU_ICON_W - g)/2, gy = Y + (MENU_ICON_H - g)/2;
    const int iconic = (f->state() == ICONIC);
    Fl_Color accent = fl_rgb_color(0x83, 0x71, 0xF7);
    fl_color(accent);
    if (iconic) {
      fl_rect(gx, gy, g, g);
      fl_color(fl_color_average(accent, FL_WHITE, 0.25));
      fl_rectf(gx+2, gy+2, g-4, g-4);
    } else {
      fl_rectf(gx, gy, g, g);
      fl_color(fl_color_average(accent, FL_WHITE, 0.55));
      fl_xyline(gx+1, gy+1, gx+g-2);        // a title strip, so it reads as a window
    }
  }
#else
  fl_draw_box(FL_THIN_DOWN_BOX, X, Y, MENU_ICON_W, MENU_ICON_H, FL_GRAY);
  for (Frame* c = Frame::first; c; c = c->next) {
    if (c->state() != UNMAPPED && (c==f || c->is_transient_for(f))) {
      int x = ((c->x()-Root->x())*SCREEN_W+Root->w()/2)/Root->w();
      int w = (c->w()*SCREEN_W+Root->w()-1)/Root->w();
      if (w > SCREEN_W) w = SCREEN_W;
      if (w < 3) w = 3;
      if (x+w > SCREEN_W) x = SCREEN_W-w;
      if (x < 0) x = 0;
      int y = ((c->y()-Root->y())*SCREEN_H+Root->h()/2)/Root->h();
      int h = (c->h()*SCREEN_H+Root->h()-1)/Root->h();
      if (h > SCREEN_H) h = SCREEN_H;
      if (h < 3) h = 3;
      if (y+h > SCREEN_H) y = SCREEN_H-h;
      if (y < 0) y = 0;
      fl_color(FL_FOREGROUND_COLOR);
      if (c->state() == ICONIC)
	fl_rect(X+x+SCREEN_DX, Y+y+SCREEN_DX, w, h);
      else
	fl_rectf(X+x+SCREEN_DX, Y+y+SCREEN_DX, w, h);
    }
  }
#endif
  fl_font(o->font, TitleFontSz); // o->size);
  fl_color((Fl_Color)o->color);
  const char* l = f->label(); if (!l) l = "unnamed";
  // double any ampersands to turn off the underscores:
  char buf[256];
  if (strchr(l,'&')) {
    char* t = buf;
    while (t < buf+254 && *l) {
      if (*l=='&') *t++ = *l;
      *t++ = *l++;
    }
    *t = 0;
    l = buf;
  }
  fl_draw(l, X+MENU_ICON_W+3, Y, W-MENU_ICON_W-3, H, align);
}

static void
frame_label_measure(const Fl_Label* o, int& W, int& H)
{
  Frame* f = (Frame*)(o->value);
  if (window_deleted(f)) {W = MENU_ICON_W+3; H = MENU_ICON_H; return;}
  const char* l = f->label(); if (!l) l = "unnamed";
  fl_font(o->font, TitleFontSz); // o->size);
  fl_measure(l, W, H);
  W += MENU_ICON_W+3;
  if (W > MAX_MENU_WIDTH) W = MAX_MENU_WIDTH;
#ifdef XXRI
  if (H < XXRI_MENU_ROW_H) H = XXRI_MENU_ROW_H;
#else
  if (H < MENU_ICON_H) H = MENU_ICON_H;
#endif
}

// This labeltype is used for non-frame items so the text can line
// up with the icons:

static void
label_draw(const Fl_Label* o, int X, int Y, int W, int H, Fl_Align align)
{
  fl_font(o->font, TitleFontSz); // o->size);
  fl_color((Fl_Color)o->color);
  fl_draw(o->value, X+MENU_ICON_W+3, Y, W-MENU_ICON_W-3, H, align);
}

static void
label_measure(const Fl_Label* o, int& W, int& H)
{
  fl_font(o->font, TitleFontSz); // o->size);
  fl_measure(o->value, W, H);
  W += MENU_ICON_W+3;
  if (W > MAX_MENU_WIDTH) W = MAX_MENU_WIDTH;
#ifdef XXRI
  if (H < XXRI_MENU_ROW_H) H = XXRI_MENU_ROW_H;
#else
  if (H < MENU_ICON_H) H = MENU_ICON_H;
#endif
}

#define FRAME_LABEL FL_FREE_LABELTYPE
#define TEXT_LABEL Fl_Labeltype(FL_FREE_LABELTYPE+1)


////////////////////////////////////////////////////////////////

#if DESKTOPS

static void
desktop_cb(Fl_Widget*, void* v)
{
  Desktop::current((Desktop*)v);
}

static void
delete_desktop_cb(Fl_Widget*, void* v)
{
  delete (Desktop*)v;
}

#if ASK_FOR_NEW_DESKTOP_NAME

static Fl_Input* new_desktop_input = 0;

static void
cancel_cb(Fl_Widget* w, void*)
{
  w->window()->hide();
}

static void
new_desktop_ok_cb(Fl_Widget* w, void*)
{
  w->window()->hide();
  Desktop::current(new Desktop(new_desktop_input->value(), Desktop::available_number()));
}

static void
new_desktop_cb(Fl_Widget*, void*)
{
  if (!new_desktop_input) {
    FrameWindow* w = new FrameWindow(190,90);
    new_desktop_input = new Fl_Input(10,30,170,25,"New desktop name:");
    new_desktop_input->align(FL_ALIGN_TOP_LEFT);
    new_desktop_input->labelfont(FL_BOLD);
    Fl_Return_Button* b = new Fl_Return_Button(100,60,80,20,"OK");
    b->callback(new_desktop_ok_cb);
    Fl_Button* b2 = new Fl_Button(10,60,80,20,"Cancel");
    b2->callback(cancel_cb);
    w->set_non_modal();
    w->end();
  }
  char buf[120];
  sprintf(buf, "Desktop %d", Desktop::available_number());
  new_desktop_input->value(buf);
  new_desktop_input->window()->hotspot(new_desktop_input);
  new_desktop_input->window()->show();
}

#else // !ASK_FOR_NEW_DESKTOP_NAME

static void
new_desktop_cb(Fl_Widget*, void*)
{
  char buf[120];
  int i = Desktop::available_number();
  sprintf(buf, "Desktop %d", i);
  Desktop::current(new Desktop(buf, i));
}

#endif

#endif
////////////////////////////////////////////////////////////////

static void
exit_cb(Fl_Widget*, void*)
{
  printf("exit_cb\n");
  Frame::save_protocol();
  exit(0);
}

#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

static void
logout_cb(Fl_Widget*, void*)
{
#ifdef XXRI
  // XXRI owns the session-end experience: exittc is Tiny Core's own dialog and
  // must never surface on this desktop.
  const char* prog = "xxri-power-menu";
#else
  const char* prog = "exittc";
#endif
  // Double-fork like spawn_cb, so no zombie is left behind, and _exit() on a
  // failed exec - without it the child carried on running the window manager's
  // event loop as a second copy of flwm.
  if (fork() == 0) {
    if (fork() == 0) {
      close(ConnectionNumber(fl_display));
      execlp(prog, prog, (void*)0);
      _exit(1);
    }
    _exit(0);
  }
  wait((int *) 0);
}

////////////////////////////////////////////////////////////////

#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

#if XTERM_MENU_ITEM || WMX_MENU_ITEMS

static const char* xtermname = "xterm";

static void
spawn_cb(Fl_Widget*, void*n)
{
  char* name = (char*)n;
  // strange code thieved from 9wm to avoid leaving zombies
  if (fork() == 0) {
    if (fork() == 0) {
      close(ConnectionNumber(fl_display));
      if (name == xtermname) execlp(name, name, "-ut", (void*)0);
      else execl(name, name, (void*)0);
      fprintf(stderr, "flwm: can't run %s, %s\n", name, strerror(errno));
      XBell(fl_display, 70);
      _exit(1);
    }
    _exit(0);
  }
  wait((int *) 0);
}

#endif

static Fl_Menu_Item other_menu_items[] = {
#if XTERM_MENU_ITEM
  {"New xterm", 0, spawn_cb, (void*)xtermname, 0, 0, 0, MENU_FONT_SIZE},
#endif
#if DESKTOPS && !defined(XXRI)
  // XXRI has no multiple-desktop concept in its design language, so the item
  // that creates one does not belong on its desktop menu.
  {"New desktop", 0, new_desktop_cb, 0, 0, 0, 0, MENU_FONT_SIZE},
#endif
#ifdef XXRI
  {"Power", 0, logout_cb, 0, 0, 0, 0, MENU_FONT_SIZE},
#else
  {"Exit", 0, logout_cb, 0, 0, 0, 0, MENU_FONT_SIZE},
#endif
  {0}};
#define num_other_items (sizeof(other_menu_items)/sizeof(Fl_Menu_Item))

// use this to fill in a menu location:
static void
init(Fl_Menu_Item& m, const char* data)
{
#ifdef HAVE_STYLES
  m.style = 0;
#endif
  m.label(data);
  m.flags = 0;
  m.labeltype(FL_NORMAL_LABEL);
  m.shortcut(0);
  m.labelfont(MENU_FONT_SLOT);
  m.labelsize(TitleFontSz); // MENU_FONT_SIZE);
  m.labelcolor(FL_FOREGROUND_COLOR);
}

#if WMX_MENU_ITEMS

// wmxlist is an array of char* pointers (for efficient sorting purposes),
// which are stored in wmxbuffer (for memory efficiency and to avoid
// freeing and fragmentation)
static char** wmxlist = NULL;
static int wmxlistsize = 0;
// wmx commands are read from ~/.wmx,
// they are stored null-separated here:
static char* wmxbuffer = NULL;
static int wmxbufsize = 0;
static int num_wmx = 0;
// ML ----------------------
// static time_t wmx_time = 0;
time_t wmx_time = 0;	// ML: made global
// ------------------------ML
static int wmx_pathlen = 0;

static int
scan_wmx_dir (char *path, int bufindex, int nest)
{
  DIR* dir = opendir(path);
  struct stat st;
  int pathlen = strlen (path);
  if (dir) {
    struct dirent* ent;
    while ((ent=readdir(dir))) {
    if (ent->d_name[0] == '.')
        continue;
      strcpy(path+pathlen, ent->d_name);
      if (stat(path, &st) < 0) continue;
      int len = pathlen+strlen(ent->d_name);
	// worst-case alloc needs
      if (bufindex+len+nest+1 > wmxbufsize)
	wmxbuffer = (char*)realloc(wmxbuffer, (wmxbufsize+=1024));
      for (int i=0; i<nest; i++)
	wmxbuffer[bufindex++] = '/'; // extra slash marks menu titles
      if (S_ISDIR(st.st_mode) && (st.st_mode & 0555) && nest<MAX_NESTING_DEPTH){
	strcpy(wmxbuffer+bufindex, path);
        bufindex += len+1;
        strcat(path, "/");
        bufindex = scan_wmx_dir (path, bufindex, nest+1);
	num_wmx++;
      } else if (S_ISREG(st.st_mode) && (st.st_mode & 0111)) {
	// make sure it exists and is an executable file:
	strcpy(wmxbuffer+bufindex, path);
	bufindex += len+1;
	num_wmx++;
      }
    }
    closedir(dir);
  }
  return bufindex;
}

// comparison for qsort
//	We keep submenus together by noting that they're proper superstrings
static int
wmxCompare(const void *A, const void *B)
{
  char	*pA, *pB;
  pA = *(char **)A;
  pB = *(char **)B;

  pA += strspn(pA, "/");
  pB += strspn(pB, "/");

  // caseless compare
  while (*pA && *pB) {
    if (toupper(*pA) > toupper(*pB))
      return(1);
    if (toupper(*pA) < toupper(*pB))
      return(-1);
    pA++;
    pB++;
  }
  if (*pA)
    return(1);
  if (*pB)
    return(-1);
  return(0);
}

static void
load_wmx()
{
  const char* home=getenv("HOME"); if (!home) home = ".";
  char path[1024];
  strcpy(path, home);
  if (path[strlen(path)-1] != '/') strcat(path, "/");
  strcat(path, ".wmx/");
  struct stat st; if (stat(path, &st) < 0) return;
  if (st.st_mtime == wmx_time) return;
  wmx_time = st.st_mtime;
  num_wmx = 0;
  wmx_pathlen = strlen(path);
  scan_wmx_dir(path, 0, 0);

  // Build wmxlist
  if (num_wmx > wmxlistsize) {
    if (wmxlist)
      delete [] wmxlist;
    wmxlist = new char *[num_wmx];
    wmxlistsize = num_wmx;
  }
  for (int i=0; i<num_wmx; i++) {
    char* cmd = wmxbuffer;

    for (int j = 0; j < num_wmx; j++) {
      wmxlist[j] = cmd;
      cmd += strlen(cmd)+1;
    }
  }

  qsort(wmxlist, num_wmx, sizeof(char *), wmxCompare);
}

#endif

////////////////////////////////////////////////////////////////

int exit_flag; // set by the -x switch

static int is_active_frame(Frame* c) {
  for (Frame* a = Frame::activeFrame(); a; a = a->transient_for())
    if (a == c) return 1;
  return 0;
}

void
ShowTabMenu(int tab)
{

  static char beenhere;
  if (!beenhere) {
    beenhere = 1;
    Fl::set_labeltype(FRAME_LABEL, frame_label_draw, frame_label_measure);
    Fl::set_labeltype(TEXT_LABEL, label_draw, label_measure);
    if (exit_flag) {
      Fl_Menu_Item* m = other_menu_items+num_other_items-2;
      m->label("Exit");
      m->callback(exit_cb);
    }
  }

  static Fl_Menu_Item* menu = 0;
  static int arraysize = 0;

#if DESKTOPS
  int one_desktop = !Desktop::first->next;
#endif

  // count up how many items are on the menu:

  int n = num_other_items;
#if WMX_MENU_ITEMS
  load_wmx();
  if (num_wmx) {
#if XTERM_MENU_ITEM
    // The built-in "New xterm" item is dropped when there are wmx items, so
    // the count drops with it.  This MUST stay tied to XTERM_MENU_ITEM: the
    // matching memcpy at the bottom of this function is compiled out when the
    // item does not exist, so subtracting unconditionally sized the array one
    // Fl_Menu_Item too small and the terminator was written off the end of the
    // heap block.  XXRI turns XTERM_MENU_ITEM off (the dock owns the
    // terminal), which made the desktop menu corrupt the heap and abort the
    // window manager the first time it was opened with a populated ~/.wmx.
    n -= 1; // delete "new xterm"
#endif
    // add wmx items
    int	level = 0;
    for (int i=0; i<num_wmx; i++) {
      int nextlev = (i==num_wmx-1)?0:strspn(wmxlist[i+1], "/")-1;
      if (nextlev < level) {
	n += level-nextlev;
	level = nextlev;
      } else if (nextlev > level)
	level++;
      n++;
    }
  }
#endif

#if DESKTOPS
  // count number of items per desktop in these variables:
  int numsticky = 0;
  Desktop* d;
  for (d = Desktop::first; d; d = d->next) d->junk = 0;
#endif

  // every frame contributes 1 item:
  Frame* c;
  for (c = Frame::first; c; c = c->next) {
    if (c->state() == UNMAPPED || c->transient_for()) continue;
#if DESKTOPS
    if (!c->desktop()) {
      numsticky++;
    } else {
      c->desktop()->junk++;
    }
#endif
    n++;
  }

#if DESKTOPS
  if (!one_desktop) {
    // add the sticky "desktop":
    n += 2; if (!numsticky) n++;
    if (Desktop::current()) {
      n += numsticky;
      Desktop::current()->junk += numsticky;
    }
    // every desktop contributes menu title, null terminator,
    // and possible delete:
    for (d = Desktop::first; d; d = d->next) {
      n += 2; if (!d->junk) n++;
    }
  }
#endif

  // Belt and braces: the count above and the fill below are two separate
  // walks over the same data, and a mismatch between them is a heap overwrite
  // that kills the whole desktop rather than a cosmetic glitch.  Carry a few
  // spare slots so a future miscount costs a couple of hundred wasted bytes
  // instead of the window manager, and remember the real capacity so the fill
  // can be clamped to it.
  const int slack = 8;
  if (n + slack > arraysize) {
    delete[] menu;
    arraysize = n + slack;
    menu = new Fl_Menu_Item[arraysize];
  }
  memset(menu, 0, arraysize * sizeof(Fl_Menu_Item));
  const int menu_cap = arraysize;

  // build the menu:
  n = 0;
  const Fl_Menu_Item* preset = 0;
  const Fl_Menu_Item* first_on_desk = 0;
#if DESKTOPS
  if (one_desktop) {
#endif
    for (c = Frame::first; c; c = c->next) {
      if (c->state() == UNMAPPED || c->transient_for()) continue;
      init(menu[n],(char*)c);
      menu[n].labeltype(FRAME_LABEL);
      menu[n].callback(frame_callback, c);
      if (is_active_frame(c)) preset = menu+n;
      n++;
    }
    if (n > 0) first_on_desk = menu;
#if DESKTOPS
  } else for (d = Desktop::first; ; d = d->next) {
    // this loop adds the "sticky" desktop last, when d==0
    if (d == Desktop::current()) preset = menu+n;
    init(menu[n], d ? d->name() : "Sticky");
    menu[n].callback(desktop_cb, d);
    menu[n].flags = FL_SUBMENU;
    n++;
    if (d && !d->junk) {
      init(menu[n],"delete this desktop");
      menu[n].callback(delete_desktop_cb, d);
      n++;
    } else if (!d && !numsticky) {
      init(menu[n],"(empty)");
      menu[n].callback_ = 0;
      menu[n].deactivate();
      n++;
    } else {
      if (d == Desktop::current()) first_on_desk = menu+n;
      for (c = Frame::first; c; c = c->next) {
	if (c->state() == UNMAPPED || c->transient_for()) continue;
	if ((c->desktop() == d) || (!c->desktop() && (d == Desktop::current()))) {
	  init(menu[n],(char*)c);
	  init(menu[n],(char*)c);
	  menu[n].labeltype(FRAME_LABEL);
	  menu[n].callback(d == Desktop::current() ?
			   frame_callback : move_frame_callback, c);
	  if (d == Desktop::current() && is_active_frame(c)) preset = menu+n;
	  n++;
	}
      }
    }
    menu[n].label(0); n++; // terminator for submenu
    if (!d) break;
  }
#endif

  // For ALT+Tab, move the selection forward or backward:
  if (tab > 0 && first_on_desk) {
    if (!preset)
      preset = first_on_desk;
    else {
      preset++;
      if (!preset->label() || preset->callback_ != frame_callback)
	preset = first_on_desk;
    }
  } else if (tab < 0 && first_on_desk) {
    if (preset && preset != first_on_desk)
      preset--;
    else {
      // go to end of menu
      preset = first_on_desk;
      while (preset[1].label() && preset[1].callback_ == frame_callback)
	preset++;
    }
  }

#if WMX_MENU_ITEMS
  // put wmx-style commands above that:
  if (num_wmx > 0) {
    char* cmd;
    int pathlen[MAX_NESTING_DEPTH];
    int level = 0;
    pathlen[0] = wmx_pathlen;
    for (int i = 0; i < num_wmx; i++) {
      cmd = wmxlist[i];
      cmd += strspn(cmd, "/")-1;
      init(menu[n], cmd+pathlen[level]);
#if DESKTOPS
      if (one_desktop)
#endif
	if (!level)
	  menu[n].labeltype(TEXT_LABEL);
      int nextlev = (i==num_wmx-1)?0:strspn(wmxlist[i+1], "/")-1;
      if (nextlev < level) {
	menu[n].callback(spawn_cb, cmd);
	// Close 'em off
	for (; level>nextlev; level--)
	  init(menu[++n], 0);
      } else if (nextlev > level) {
	// This should be made a submenu
	pathlen[++level] = strlen(cmd)+1; // extra for next trailing /
	menu[n].flags = FL_SUBMENU;
	menu[n].callback((Fl_Callback*)0);
      } else {
	menu[n].callback(spawn_cb, cmd);
      }
      n++;
    }
  }
#endif // WMX_MENU_ITEMS

  // Put the fixed menu items at the bottom.  One copy, one place, so the
  // number of items appended can never disagree with the number counted.
  const Fl_Menu_Item* tail = other_menu_items;
  int tailn = (int)num_other_items;
#if WMX_MENU_ITEMS && XTERM_MENU_ITEM
  if (num_wmx) { tail++; tailn--; } // wmx items replace the built-in xterm one
#endif
  if (n + tailn > menu_cap) {   // never write past the end of the block
    fprintf(stderr, "flwm: menu needs %d slots, has %d - truncating\n",
            n + tailn, menu_cap);
    n = menu_cap - tailn;
    if (n < 0) n = 0;
  }
  memcpy(menu+n, tail, tailn * sizeof(Fl_Menu_Item));
#if DESKTOPS
  if (one_desktop)
#endif
    // fix the menus items so they are indented to align with window names:
    while (menu[n].label()) menu[n++].labeltype(TEXT_LABEL);

  const Fl_Menu_Item* picked =
    menu->popup(Fl::event_x(), Fl::event_y(), 0, preset);
  if (picked && picked->callback()) picked->do_callback(0);
}

void ShowMenu() {ShowTabMenu(0);}
