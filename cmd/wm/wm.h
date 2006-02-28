/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "ixp.h"
#include "blitz.h"

/* array indexes of EWMH window properties */
	                  /* TODO: set / react */
enum {
	NET_NUMBER_OF_DESKTOPS, /*  ✓      –  */
	NET_CURRENT_DESKTOP,    /*  ✓      ✓  */
	NET_WM_DESKTOP          /*  ✗      ✗  */
};

/* 8-bit qid.path.type */
enum {                          
    Droot,
	Ddef,
	Dpage,
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
	Finc,
	Fgeom,
	Fevent,
	Fctl,
	Fname
};

#define NET_ATOM_COUNT         3

#define PROTO_DEL              1
#define DEF_BORDER             3
#define DEF_SNAP               20
#define DEF_PAGER_GAP          5

#define ROOT_MASK              SubstructureRedirectMask
#define CLIENT_MASK            (StructureNotifyMask | PropertyChangeMask)

typedef struct Area Area;
typedef struct Page Page;
typedef struct Client Client;

typedef enum { COL_MAX, COL_EQUAL, COL_STACK } ColumnMode;

struct Area {
	unsigned short id;
    Client **client;
	Page *page;
	size_t clientsz;
	size_t sel;
	size_t nclient;
	size_t maxclient;
	ColumnMode mode;
	XRectangle rect;
};

struct Page {
	unsigned short id;
	Area **area;
	size_t areasz;
	size_t narea;
	size_t sel;
	Page *revert;
};

struct Client {
	unsigned short id;
	char name[256];
    int proto;
    unsigned int border;
    Bool destroyed;
	Area *area;
    Window win;
    Window trans;
    XRectangle rect;
    XSizeHints size;
	Client *revert;
	struct Frame {
		Window win;
    	XRectangle rect;
		XRectangle revert;
    	GC gc;
    	Cursor cursor;
	} frame;
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
	Bool inc;
	Color sel;
	Color norm;
	unsigned int border;
	unsigned int snap;
} Default;

/* global variables */
Page **page;
size_t npage;
size_t pagesz;
size_t sel;
Client **client;
size_t nclient;
size_t clientsz;
Key **key;
size_t keysz;
size_t nkey;
Label **label;
size_t nlabel;
size_t labelsz;
size_t iexpand;

Display *dpy;
IXPServer *ixps;
int screen;
Window root;
Window transient; /* pager / attach */
XRectangle rect;
XFontStruct *xfont;
GC gc_xor;
GC gc_transient;
IXPServer srv;
Pixmap pmapbar;
Window winbar;
GC gcbar;
XRectangle brect;
Qid root_qid;

Default def;

Atom wm_state; /* TODO: Maybe replace with wm_atoms[WM_ATOM_COUNT]? */
Atom wm_change_state;
Atom wm_protocols;
Atom wm_delete;
Atom motif_wm_hints;
Atom net_atoms[NET_ATOM_COUNT];

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
Area *alloc_area(Page *p);
void destroy_area(Area *a);
int area_to_index(Area *a);
int aid_to_index(Page *p, unsigned short id);
void update_area_geometry(Area *a);
void select_area(Area *a, char *arg);
void sendto_area(Area *new, Client *c);

/* bar.c */
Label *new_label();
void detach_label(Label *l);
void draw_bar();
int lid_to_index(unsigned short id);
void update_bar_geometry();
unsigned int bar_height();

/* client.c */
Client *alloc_client(Window w, XWindowAttributes *wa);
void destroy_client(Client *c);
void configure_client(Client *c);
void handle_client_property(Client *c, XPropertyEvent *e);
void kill_client(Client *c);
void draw_client(Client *client);
void gravitate(Client *c, unsigned int tabh, unsigned int bw, int invert);
void unmap_client(Client *c);
void map_client(Client *c);
void reparent_client(Client *c, Window w, int x, int y);
void attach_client(Client *c);
void attach_client_to_page(Page *p, Client *c);
void detach_client(Client *c, Bool unmap);
Client *sel_client();
Client *sel_client_of_page(Page *p);
void focus_client(Client *c);
Client *win_to_clientframe(Window w);
void resize_client(Client *c, XRectangle *r, XPoint *pt);
int cid_to_index(Area *a, unsigned short id);
int client_to_index(Client *c);
void select_client(Client *c, char *arg);
void sendtopage_client(Client *c, char *arg);
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
Key * name_to_key(char *name);
int kid_to_index(unsigned short id);
Key *create_key(char *name);
void destroy_key(Key *k);
void init_lock_modifiers();

/* mouse.c */
void mouse_resize(Client *c, Align align);
void mouse_move(Client *c);
Cursor cursor_for_motion(Client *c, int x, int y);
Align cursor_to_align(Cursor cursor);
Align xy_to_align(XRectangle * rect, int x, int y);
void drop_move(Client *c, XRectangle *new, XPoint *pt);
void grab_mouse(Window w, unsigned long mod, unsigned int button);
void ungrab_mouse(Window w, unsigned long mod, unsigned int button);
char *warp_mouse(char *arg);

/* page.c */
Page *alloc_page();
char *destroy_page(Page *p);
void focus_page(Page *p);
XRectangle *rectangles(unsigned int *num);
int pid_to_index(unsigned short id);
void select_page(char *arg);
int page_to_index(Page *p);

/* spawn.c */
void spawn(char *cmd);

/* column.c */
void arrange_page(Page *p, Bool update_colums);
void arrange_column(Area *col);
void resize_column(Client *c, XRectangle *r, XPoint *pt);
Area *new_column(Area *old);

/* wm.c */
void scan_wins();
Client *win_to_client(Window w);
int win_proto(Window w);
int win_state(Window w);
/*void handle_after_write(IXPServer * s, File * f);*/
void pager();

