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

#define NET_ATOM_COUNT         3

#define PROTO_DEL              1
#define BORDER_WIDTH           3
#define GAP                    5

#define ROOT_MASK              SubstructureRedirectMask
#define CLIENT_MASK            (StructureNotifyMask | PropertyChangeMask)

typedef struct Column Column;
typedef struct Page Page;
typedef struct Client Client;

struct Column {
    Client **client;
	size_t clientsz;
	size_t sel;
	size_t nclient;
	XRectangle rect;
};

struct Page {
	Client **floatc;
	Column **col;
	size_t floatcsz;
	size_t colsz;
	size_t sel_float;
	size_t sel_col;
	size_t nfloat;
	size_t ncol;
	Bool is_column;
	XRectangle rect_column;
};

struct Client {
	char name[256];
    int proto;
    unsigned int border;
    unsigned int ignore_unmap;
	Bool handle_inc;
    Bool destroyed;
	Bool maximized;
	Bool attached;
	Column *column;
    Window win;
    Window trans;
    XRectangle rect;
    XSizeHints size;
	Page *page;
	struct Frame {
		Window win;
    	XRectangle rect;
		XRectangle revert;
    	GC gc;
    	Cursor cursor;
		Bool title;
	} frame;
};

/* global variables */
Page **page;
size_t npage;
size_t pagesz;
size_t sel_page;
Page **aqueue;
size_t aqueuesz;
Client **detached;
size_t detachedsz;
Client **client;
size_t clientsz;

Display *dpy;
IXPServer *ixps;
int screen;
Window root;
Window transient; /* pager / attach */
XRectangle rect;
XFontStruct *xfont;
XColor color_xor;
GC gc_xor;
GC gc_transient;
IXPServer *srv;

/* default values */
typedef struct {
	char selcolor[24];
	char normcolor[24];
	char font[24];
	unsigned int border;
	unsigned int title;
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


/* client.c */
Client *alloc_client(Window w, XWindowAttributes *wa);
void destroy_client(Client * c);
void configure_client(Client * c);
void handle_client_property(Client * c, XPropertyEvent * e);
void close_client(Client * c);
void draw_client(Client * client);
void gravitate(Client * c, unsigned int tabh, unsigned int bw, int invert);
void grab_client(Client * c, unsigned long mod, unsigned int button);
void ungrab_client(Client * c, unsigned long mod, unsigned int button);
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
unsigned int tab_height(Client *c);
unsigned int border_width(Client *c);

/* event.c */
void init_event_hander();
void check_event(IXPConn *c);

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

/* column.c */
void arrange_page(Page *p);
void arrange_column(Page *p, Column *col);
void attach_column(Client *c);
void detach_column(Client *c);
void resize_column(Client *c, XRectangle *r, XPoint *pt);
void select_column(Client *c, char *arg);
void new_column(Page *p);

/* wm.c */
/*
void invoke_wm_event(File * f);
void run_action(File * f, void *obj, Action * acttbl);
*/
void scan_wins();
Client *win_to_client(Window w);
int win_proto(Window w);
int win_state(Window w);
/*void handle_after_write(IXPServer * s, File * f);*/
void detach(Client * f, int client_destroyed);
void set_client_state(Client * c, int state);
