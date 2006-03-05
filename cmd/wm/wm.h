/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "ixp.h"
#include "blitz.h"

/* array indexes of atoms */
enum {
	WMState,
	WMProtocols,
	WMDelete,
	WMLast
};

enum {
	NetNumWS,
	NetSelWS,
	NetWS,
	NetLast
};

enum {
	Colequal,
	Colstack,
	Colmax
};

/* 8-bit qid.path.type */
enum {                          
    Droot,
	Ddef,
	Dtag,
	Darea,
	Dclient,
	Dkeys,
	Dbar,
    Dlabel,
	Fexpand,
    Fdata,                      /* data to display */
    Fcolors,
    Ffont,
    Fselcolors,
    Fnormcolors,
	Fkey,
	Fborder,
	Fsnap,
	Fbar,
	Fgeom,
	Fevent,
	Fctl,
	Fname,
	Fmode
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

struct Tag {
	unsigned short id;
	Area **area;
	unsigned int areasz;
	unsigned int narea;
	unsigned int sel;
};

struct Frame {
	Area *area;
	unsigned short id;
	XRectangle rect;
	Client *client;
};

struct Client {
	char name[256];
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
	Frame *frame;
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
Atom net_atom[NetLast];

Cursor normal_cursor;
Cursor resize_cursor;
Cursor move_cursor;
Cursor drag_cursor;
Cursor w_cursor;
Cursor e_cursor;
Cursor n_cursor;
Cursor s_cursor;
Cursor nw_cursor;
Cursor ne_cursor;
Cursor sw_cursor;
Cursor se_cursor;

unsigned int valid_mask, num_lock_mask;

/* area.c */
Area *alloc_area(Tag *t);
void destroy_area(Area *a);
int area2index(Area *a);
int aid2index(Tag *t, unsigned short id);
void update_area_geometry(Area *a);
void select_area(Area *a, char *arg);
void send_toarea(Area *to, Client *c);
void attach_toarea(Area *a, Client *c);
void detach_fromarea(Client *c);
void arrange_tag(Tag *t, Bool updategeometry);
void arrange_area(Area *a);
void resize_area(Client *c, XRectangle *r, XPoint *pt);
Area *new_area(Tag *t);
int str2mode(char *arg);
char *mode2str(int mode);

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
void attach_totag(Tag *t, Client *c);
void detach_client(Client *c, Bool unmap);
Client *sel_client();
Client *sel_client_of_tag(Tag *t);
void focus_client(Client *c);
Client *win2clientframe(Window w);
void resize_client(Client *c, XRectangle *r, XPoint *pt, Bool ignore_xcall);
int frid2index(Area *a, unsigned short id);
int frame2index(Frame *f);
void select_client(Client *c, char *arg);
void sendtotag_client(Client *c, char *arg);
void sendtoarea_client(Client *c, char *arg);
void resize_all_clients();
void focus(Client *c);

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
Align cursor2align(Cursor cursor);
Align xy2align(XRectangle * rect, int x, int y);
void drop_move(Client *c, XRectangle *new, XPoint *pt);
void grab_mouse(Window w, unsigned long mod, unsigned int button);
void ungrab_mouse(Window w, unsigned long mod, unsigned int button);
char *warp_mouse(char *arg);

/* tag.c */
Tag *alloc_tag();
char *destroy_tag(Tag *t);
void focus_tag(Tag *t);
XRectangle *rectangles(unsigned int *num);
int pid2index(unsigned short id);
void select_tag(char *arg);
int tag2index(Tag *t);

/* wm.c */
void scan_wins();
Client *win2client(Window w);
int win_proto(Window w);
int win_state(Window w);
