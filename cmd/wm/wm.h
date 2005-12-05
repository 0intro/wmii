/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdio.h>
#include <X11/Xutil.h>

#include "cext.h"
#include "wmii.h"

/* array indexes of page file pointers */
enum {
	P_PREFIX,
	P_AREA_PREFIX,
	P_SEL_AREA,
	P_CTL,
	P_LAST
};

/* array indexes of area file pointers */
enum {
	A_PREFIX,
	A_FRAME_PREFIX,
	A_SEL_FRAME,
	A_CTL,
	A_GEOMETRY,
	A_LAYOUT,
	A_LAST
};

/* array indexes of frame file pointers */
enum {
	F_PREFIX,
	F_CLIENT_PREFIX,
	F_SEL_CLIENT,
	F_CTL,
	F_SIZE,
	F_BORDER,
	F_TAB,
	F_HANDLE_INC,
	F_LOCKED,
	F_SEL_BG_COLOR,
	F_SEL_FG_COLOR,
	F_SEL_BORDER_COLOR,
	F_NORM_BG_COLOR,
	F_NORM_FG_COLOR,
	F_NORM_BORDER_COLOR,
	F_EVENT_B2PRESS,
	F_EVENT_B3PRESS,
	F_EVENT_B4PRESS,
	F_EVENT_B5PRESS,
	F_LAST
};

/* array indexes of client file pointers */
enum {
	C_PREFIX,
	C_NAME,
	C_LAST
};

/* array indexes of wm file pointers */
enum {
	WM_CTL,
	WM_DETACHED_FRAME,
	WM_DETACHED_CLIENT,
	WM_TRANS_COLOR,
	WM_SEL_BG_COLOR,
	WM_SEL_BORDER_COLOR,
	WM_SEL_FG_COLOR,
	WM_NORM_BG_COLOR,
	WM_NORM_BORDER_COLOR,
	WM_NORM_FG_COLOR,
	WM_FONT,
	WM_PAGE_SIZE,
	WM_BORDER,
	WM_TAB,
	WM_HANDLE_INC,
	WM_LOCKED,
	WM_SNAP_VALUE,
	WM_SEL_PAGE,
	WM_LAYOUT,
	WM_EVENT_PAGE_UPDATE,
	WM_EVENT_CLIENT_UPDATE,
	WM_EVENT_B1PRESS,
	WM_EVENT_B2PRESS,
	WM_EVENT_B3PRESS,
	WM_EVENT_B4PRESS,
	WM_EVENT_B5PRESS,
	WM_LAST
};

#define PROTO_DEL              1
#define BORDER_WIDTH           3
#define LAYOUT                 "column"
#define GAP 5

#define ROOT_MASK              (SubstructureRedirectMask | SubstructureNotifyMask | ButtonPressMask | ButtonReleaseMask)
#define CLIENT_MASK            (SubstructureNotifyMask | PropertyChangeMask | EnterWindowMask)

typedef struct Page Page;
typedef struct Layout Layout;
typedef struct Area Area;
typedef struct Frame Frame;
typedef struct Client Client;

/* new layout interface:
 * /page/[1..n]/0/ floating space
 * /page/[1..n]/[1..n]/ layout space
 */

struct Page {
	Area **area;
	unsigned int sel;
	File *file[P_LAST];
};

struct Layout {
	char *name;
	void (*init) (Area *);		/* called when layout is initialized */
	void (*deinit) (Area *);	/* called when layout is uninitialized */
	void (*arrange) (Area *);	/* called when area is resized */
	void (*attach) (Area *, Client *);	/* called on attach */
	void (*detach) (Area *, Client *, int, int);	/* called on detach */
	void (*resize) (Frame *, XRectangle *, XPoint * pt);	/* called after resize */
};

struct Area {
	Layout *layout;
	Page *page;
	Frame **frame;
	unsigned int sel;
	XRectangle rect;
	void *aux;					/* free pointer */
	File *file[A_LAST];
};

struct Frame {
	Area *area;
	Window win;
	GC gc;
	XRectangle rect;
	Cursor cursor;
	Client **client;
	int sel;
	void *aux;					/* free pointer */
	File *file[F_LAST];
};

struct Client {
	int proto;
	unsigned int border;
	Window win;
	Window trans;
	XRectangle rect;
	XSizeHints size;
	Frame *frame;
	File *file[C_LAST];
};

#define SELAREA (page ? page[sel]->area[page[sel]->sel] : 0)
#define SELFRAME(x) (x && x->area[x->sel]->frame ?  x->area[x->sel]->frame[x->area[x->sel]->sel] : 0)
#define ISSELFRAME(x) (page && SELFRAME(page[sel]) == x)

/* global variables */
Display *dpy;
IXPServer *ixps;
int screen_num;
Window root;
Window transient;
XRectangle rect;
Client **detached;
Page **page;
unsigned int sel;
Frame **frame;
Client **client;
XFontStruct *font;
XColor xorcolor;
GC xorgc;
GC transient_gc;
Layout **layouts;

Atom wm_state;
Atom wm_change_state;
Atom wm_protocols;
Atom wm_delete;
Atom motif_wm_hints;
Atom net_wm_desktop;

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

/* area.c */
Area *alloc_area(Page *p, XRectangle * r, char *layout);
void destroy_area(Area * a);
void sel_area(Area * a, int raise, int up, int down);
void attach_frame_to_area(Area * a, Frame * f);
void detach_frame_from_area(Frame * f, int ignore_sel_and_destroy);
void draw_area(Area * a);
void hide_area(Area * a);
void show_area(Area * a);

/* client.c */
Client *alloc_client(Window w);
void init_client(Client * c, XWindowAttributes * wa);
void destroy_client(Client * c);
void configure_client(Client * c);
void handle_client_property(Client * c, XPropertyEvent * e);
void close_client(Client * c);
void draw_client(Client * c);
void draw_clients(Frame * f);
void gravitate(Client * c, unsigned int tabh, unsigned int bw, int invert);
void grab_client(Client * c, unsigned long mod, unsigned int button);
void ungrab_client(Client * c, unsigned long mod, unsigned int button);
void hide_client(Client * c);
void show_client(Client * c);
void reparent_client(Client * c, Window w, int x, int y);
void sel_client(Client * c, int raise, int up);
void attach_client(Client * c);

/* frame.c */
void sel_frame(Frame * f, int raise, int up, int down);
Frame *win_to_frame(Window w);
Frame *alloc_frame(XRectangle * r);
void destroy_frame(Frame * f);
void resize_frame(Frame * f, XRectangle * r, XPoint * pt, int ignore_layout);
void draw_frame(Frame * f);
void handle_frame_buttonpress(XButtonEvent * e, Frame * f);
void attach_client_to_frame(Frame * f, Client * c);
void detach_client_from_frame(Client * c, int unmapped, int destroyed);
void draw_tab(Frame * f, char *text, int x, int y, int w, int h, int sel);
unsigned int tab_height(Frame * f);
unsigned int border_width(Frame * f);

/* event.c */
void init_event_hander();
void check_event(Connection * c);

/* mouse.c */
void mouse_resize(Frame * f, Align align);
void mouse_move(Frame * f);
Cursor cursor_for_motion(Frame * f, int x, int y);
Align cursor_to_align(Cursor cursor);
Align xy_to_align(XRectangle * rect, int x, int y);
void drop_move(Frame * f, XRectangle * new, XPoint * pt);

/* page.c */
Page *alloc_page();
void free_page(Page * p);
void destroy_page(Page * p);
void sel_page(Page * p, int raise, int down);
XRectangle *rectangles(unsigned int *num);
void hide_page(Page * p);
void show_page(Page * p);
void draw_page(Page * p);

/* layout.c */
void init_layouts();

/* wm.c */
void invoke_wm_event(File * f);
void run_action(File * f, void *obj, Action * acttbl);
void scan_wins();
Client *win_to_client(Window w);
int win_proto(Window w);
int win_state(Window w);
void handle_after_write(IXPServer * s, File * f);
void detach(Frame * f, int client_destroyed);
void set_client_state(Client * c, int state);
Layout *get_layout(char *name);
