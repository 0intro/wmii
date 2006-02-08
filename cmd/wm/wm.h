/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "wmii.h"

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
	Ddefault,
	Dpage,
	Darea,
	Dclient,
    Ffont,
	Fselcolor,
	Fnormcolor,
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

#define NEW_OBJ                (unsigned short)0xffff

typedef struct Area Area;
typedef struct Page Page;
typedef struct Client Client;

struct Area {
	unsigned short id;
    Client **client;
	size_t clientsz;
	size_t sel;
	size_t nclient;
	XRectangle rect;
};

struct Page {
	unsigned short id;
	Area **area;
	size_t areasz;
	size_t narea;
	size_t sel;
};

struct Client {
	unsigned short id;
	char name[256];
    int proto;
    unsigned int border;
    unsigned int ignore_unmap;
    Bool destroyed;
	Bool maximized;
	Page *page;
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
		Bool bar;
    	unsigned int border;
	} frame;
};

/* global variables */
Page **page;
size_t npage;
size_t pagesz;
size_t sel;
Page **aq;
size_t aqsz;
Client **det;
size_t ndet;
size_t detsz;
Client **client;
size_t nclient;
size_t clientsz;

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

/* default values */
typedef struct {
	char selcolor[24];
	char normcolor[24];
	char *font;
	Bool bar;
	Bool inc;
	Color sel;
	Color norm;
	unsigned int border;
	unsigned int snap;
} Default;

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
Area *alloc_area();
void destroy_area(Area *a);
int index_of_area(Page *p, Area *a);
int index_of_area_id(Page *p, unsigned short id);

/* client.c */
Client *alloc_client(Window w, XWindowAttributes *wa);
void destroy_client(Client * c);
void configure_client(Client * c);
void handle_client_property(Client * c, XPropertyEvent * e);
void close_client(Client * c);
void draw_client(Client * client);
void gravitate(Client * c, unsigned int tabh, unsigned int bw, int invert);
void unmap_client(Client * c);
void map_client(Client * c);
void reparent_client(Client * c, Window w, int x, int y);
void attach_client(Client * c);
void detach_client(Client * c, Bool unmap);
Client *sel_client();
Client *sel_client_of_page(Page *p);
void focus_client(Client *c);
Client *win_to_frame(Window w);
void resize_client(Client *c, XRectangle * r, XPoint * pt);
unsigned int bar_height(Client *c);
int index_of_client_id(Area *a, unsigned short id);

/* event.c */
void init_x_event_handler();
void check_x_event(IXPConn *c);

/* fs.c */
unsigned long long mkqpath(unsigned char type, unsigned short pg,
						unsigned short area, unsigned short cl);
void do_pend_fcall(char *event);
void new_ixp_conn(IXPConn *c);

/* mouse.c */
void mouse_resize(Client *c, Align align);
void mouse_move(Client *c);
Cursor cursor_for_motion(Client *c, int x, int y);
Align cursor_to_align(Cursor cursor);
Align xy_to_align(XRectangle * rect, int x, int y);
void drop_move(Client *c, XRectangle *new, XPoint *pt);

/* page.c */
Page *alloc_page();
void destroy_page(Page *p);
void focus_page(Page *p);
XRectangle *rectangles(unsigned int *num);
int index_of_page_id(unsigned short id);

/* column.c */
void arrange_page(Page *p);
void arrange_column(Page *p, Area *col);
void attach_column(Client *c);
void detach_column(Client *c);
void resize_column(Client *c, XRectangle *r, XPoint *pt);
void select_column(Client *c, char *arg);
void new_column(Page *p);

/* wm.c */
void scan_wins();
Client *win_to_client(Window w);
int win_proto(Window w);
int win_state(Window w);
/*void handle_after_write(IXPServer * s, File * f);*/
void grab_window(Window w, unsigned long mod, unsigned int button);
void ungrab_window(Window w, unsigned long mod, unsigned int button);
void pager();
void detached_clients();
void attach_detached_client();
void select_page(char *arg);

