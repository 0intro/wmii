/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "ixp.h"
#include "blitz.h"

/* WM atoms */
enum {
	WMState,
	WMProtocols,
	WMDelete,
	WMLast
};

/* Column modes */
enum {
	Colequal,
	Colstack,
	Colmax
};

/* Cursor */
enum {
	CurNormal,
	CurResize,
	CurMove,
	CurW,
	CurE,
	CurN,
	CurS,
	CurNW,
	CurNE,
	CurSW,
	CurSE,
	CurLast
};

/* 8-bit qid.path.type */
enum {                          
    FsDroot,
	FsDdef,
	FsDws,
	FsDarea,
	FsDclients,
	FsDclient,
	FsDGclient,
	FsDkeys,
	FsDtags,
	FsDbar,
    FsDlabel,
	FsFexpand,
    FsFdata,                      /* data to display */
    FsFcolors,
    FsFfont,
    FsFselcolors,
    FsFnormcolors,
	FsFkey,
	FsFborder,
	FsFsnap,
	FsFbar,
	FsFgeom,
	FsFevent,
	FsFctl,
	FsFname,
	FsFtags,
	FsFtag,
	FsFmode
};

#define PROTO_DEL              1
#define DEF_BORDER             3
#define DEF_SNAP               20

#define ROOT_MASK              SubstructureRedirectMask
#define CLIENT_MASK            (StructureNotifyMask | PropertyChangeMask)

typedef struct Tag Tag;
typedef struct Area Area;
typedef struct Frame Frame;
typedef struct Client Client;

struct Tag {
	char name[256];
	unsigned short id;
	Area **area;
	unsigned int areasz;
	unsigned int narea;
	unsigned int sel;
};

struct Area {
	unsigned short id;
    Frame **frame;
	Tag *tag;
	unsigned int framesz;
	unsigned int sel;
	unsigned int nframe;
	int mode;
	XRectangle rect;
};

struct Frame {
	Area *area;
	unsigned short id;
	XRectangle rect;
	Client *client;
};

struct Client {
	unsigned short id;
	char name[256];
	char tags[256];
    int proto;
    unsigned int border;
    Bool destroyed;
    Window win;
    Window trans;
    XRectangle rect;
    XSizeHints size;
	Window framewin;
    GC gc;
    Cursor cursor;
	Frame **frame;
	unsigned int framesz;
	unsigned int sel;
	unsigned int nframe;
};

typedef struct Key Key;
struct Key {
	unsigned short id;
    char name[128];
    unsigned long mod;
    KeyCode key;
    Key *next;
};

typedef struct {
	unsigned short id;
    char data[256];
	char colstr[24];
	Color color;
	XRectangle rect;
} Label;

/* default values */
typedef struct {
	char selcolor[24];
	char normcolor[24];
	char tag[256];
	char *font;
	Color sel;
	Color norm;
	unsigned int border;
	unsigned int snap;
} Default;

/* global variables */
Tag **tag;
unsigned int ntag;
unsigned int tagsz;
unsigned int sel;
Client **client;
unsigned int nclient;
unsigned int clientsz;
Key **key;
unsigned int keysz;
unsigned int nkey;
Label **label;
unsigned int nlabel;
unsigned int labelsz;
unsigned int iexpand;
char **ctag;
unsigned int nctag;
unsigned int ctagsz;

Display *dpy;
IXPServer *ixps;
int screen;
Window root;
XRectangle rect;
XFontStruct *xfont;
GC gc_xor;
IXPServer srv;
Pixmap pmapbar;
Window winbar;
GC gcbar;
XRectangle brect;
Qid root_qid;

Default def;
Atom wm_atom[WMLast];

Cursor cursor[CurLast];
unsigned int valid_mask, num_lock_mask;

/* area.c */
Area *alloc_area(Tag *t);
void destroy_area(Area *a);
int area2index(Area *a);
int aid2index(Tag *t, unsigned short id);
void update_area_geometry(Area *a);
void select_area(Area *a, char *arg);
void send_toarea(Area *to, Area *from, Client *c);
void attach_toarea(Area *a, Client *c);
void detach_fromarea(Area *a, Client *c);
void arrange_tag(Tag *t, Bool updategeometry);
void arrange_area(Area *a);
void resize_area(Client *c, XRectangle *r, XPoint *pt);
int str2mode(char *arg);
char *mode2str(int mode);
Bool clientofarea(Area *a, Client *c);

/* bar.c */
Label *new_label();
void detach_label(Label *l);
void draw_bar();
int lid2index(unsigned short id);
void update_bar_geometry();
unsigned int bar_height();

/* client.c */
Client *alloc_client(Window w, XWindowAttributes *wa);
void destroy_client(Client *c);
void configure_client(Client *c);
void handle_client_property(Client *c, XPropertyEvent *e);
void kill_client(Client *c);
void draw_client(Client *client);
void gravitate(Client *c, Bool invert);
void unmap_client(Client *c);
void map_client(Client *c);
void reparent_client(Client *c, Window w, int x, int y);
void attach_client(Client *c);
void detach_client(Client *c, Bool unmap);
Client *sel_client();
Client *sel_client_of_tag(Tag *t);
void focus_client(Client *c);
Client *win2clientframe(Window w);
void resize_client(Client *c, XRectangle *r, XPoint *pt, Bool ignore_xcall);
int frid2index(Area *a, unsigned short id);
int frame2index(Frame *f);
void select_client(Client *c, char *arg);
void sendtoarea_client(Client *c, char *arg);
void resize_all_clients();
void focus(Client *c);
int cid2index(unsigned short id);

/* event.c */
void init_x_event_handler();
void check_x_event(IXPConn *c);

/* fs.c */
unsigned long long mkqpath(unsigned char type, unsigned short pg,
						unsigned short area, unsigned short cl);
void write_event(char *event);
void new_ixp_conn(IXPConn *c);

/* kb.c */
void handle_key(Window w, unsigned long mod, KeyCode keycode);
void grab_key(Key *k);
void ungrab_key(Key *k);
Key * name2key(char *name);
int kid2index(unsigned short id);
Key *create_key(char *name);
void destroy_key(Key *k);
void init_lock_modifiers();

/* mouse.c */
void mouse_resize(Client *c, Align align);
void mouse_move(Client *c);
Cursor cursor_for_motion(Client *c, int x, int y);
Align cursor2align(Cursor cur);
Align xy2align(XRectangle *rect, int x, int y);
void drop_move(Client *c, XRectangle *new, XPoint *pt);
void grab_mouse(Window w, unsigned long mod, unsigned int button);
void ungrab_mouse(Window w, unsigned long mod, unsigned int button);
char *warp_mouse(char *arg);

/* tag.c */
Tag *alloc_tag(char *name);
char *destroy_tag(Tag *t);
void focus_tag(Tag *t);
XRectangle *rectangles(unsigned int *num);
int tid2index(unsigned short id);
void select_tag(char *arg);
int tag2index(Tag *t);
Bool has_ctag(char *tag);
void update_ctags();
Bool clientoftag(Tag *t, Client *c);
void detach_fromtag(Tag *t, Client *c, Bool unmap);
void attach_totag(Tag *t, Client *c);

/* wm.c */
void scan_wins();
Client *win2client(Window w);
int win_proto(Window w);
int win_state(Window w);
