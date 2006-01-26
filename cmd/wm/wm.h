/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "wmii.h"

/* array indexes of page file pointers */
enum {
    P_PREFIX,
    P_NAME,
    P_MANAGED_PREFIX,
    P_FLOATING_PREFIX,
    P_SEL_PREFIX,
    P_SEL_MANAGED_CLIENT,
    P_SEL_FLOATING_CLIENT,
    P_CTL,
    P_LAST
};

/* array indexes of frame file pointers */
enum {
    C_PREFIX,
    C_NAME,
    C_GEOMETRY,
    C_BORDER,
    C_TAB,
    C_HANDLE_INC,
    C_LAST
};

/* array indexes of wm file pointers */
enum {
    WM_CTL,
    WM_TRANS_COLOR,
    WM_MANAGED_GEOMETRY,
    WM_SEL_BG_COLOR,
    WM_SEL_BORDER_COLOR,
    WM_SEL_FG_COLOR,
    WM_NORM_BG_COLOR,
    WM_NORM_BORDER_COLOR,
    WM_NORM_FG_COLOR,
    WM_FONT,
    WM_BORDER,
    WM_TAB,
    WM_HANDLE_INC,
    WM_SNAP_VALUE,
    WM_SEL_PAGE,
    WM_EVENT_PAGE_UPDATE,
    WM_EVENT_CLIENT_UPDATE,
    WM_EVENT_B1PRESS,
    WM_EVENT_B2PRESS,
    WM_EVENT_B3PRESS,
    WM_EVENT_B4PRESS,
    WM_EVENT_B5PRESS,
    WM_LAST
};

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
    Client **clients;
	size_t clientssz;
	size_t sel;
};

struct Page {
	Client **floating;
	Column **managed;
	size_t floatingsz;
	size_t managedsz;
	size_t sel_float;
	size_t sel_managed;
	Bool is_managed;
	XRectangle rect_managed;
    File *file[P_LAST];
};

struct Client {
	char name[256];
    int proto;
    unsigned int border;
    unsigned int ignore_unmap;
    Bool destroyed;
	Bool maximized;
	Bool framed;
	Bool managed;
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
	} frame;
	File *file[C_LAST];
};

/* global variables */
Page **pages;
size_t pagessz;
size_t sel_page;
Page **aqueue;
size_t aqueuesz;
Client **detached;
size_t detachedsz;
Client **clients;
size_t clientssz;

Display *dpy;
IXPServer *ixps;
int screen;
Window root;
Window transient; /* pager / attach */
XRectangle rect;
XFontStruct *font;
XColor color_xor;
GC gc_xor;
GC gc_transient;

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

/* default file pointers */
File *def[WM_LAST];

unsigned int valid_mask, num_lock_mask;


/* client.c */
void attach_client_to_array(Client *c, Client **array, size_t *size);
void detach_client_from_array(Client *c, Client **array);
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
void focus_client(Client *c);
Client *win_to_frame(Window w);
void resize_client(Client *c, XRectangle * r, XPoint * pt);
unsigned int tab_height(Client *c);
unsigned int border_width(Client *c);

/* event.c */
void init_event_hander();
void check_event(Connection * c);

/* mouse.c */
void mouse_resize(Client * f, Align align);
void mouse_move(Client * f);
Cursor cursor_for_motion(Client * f, int x, int y);
Align cursor_to_align(Cursor cursor);
Align xy_to_align(XRectangle * rect, int x, int y);
void drop_move(Client * f, XRectangle * new, XPoint * pt);

/* page.c */
size_t alloc_page();
void destroy_page(size_t idx);
void focus_page(size_t idx);
XRectangle *rectangles(unsigned int *num);

/* column.c */
void arrange_column(Page *p);
void attach_column(Client *c);
void detach_column(Client *c);
void resize_column(Client *c, XRectangle *r, XPoint *pt);

/* wm.c */
void invoke_wm_event(File * f);
void run_action(File * f, void *obj, Action * acttbl);
void scan_wins();
Client *win_to_client(Window w);
int win_proto(Window w);
int win_state(Window w);
void handle_after_write(IXPServer * s, File * f);
void detach(Client * f, int client_destroyed);
void set_client_state(Client * c, int state);
