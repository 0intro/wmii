/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdio.h>
#include <X11/Xutil.h>

#include "wmii.h"

/* array indexes of page file pointers */
enum {
	P_PREFIX,
	P_NAME,
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
	A_LAYOUT,
	A_LAST
};

/* array indexes of frame file pointers */
enum {
	F_PREFIX,
	F_CLIENT_PREFIX,
	F_SEL_CLIENT,
	F_CTL,
	F_GEOMETRY,
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
	WM_AREA_GEOMETRY,
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
#define GAP                    5

#define ROOT_MASK              SubstructureRedirectMask
#define CLIENT_MASK            (StructureNotifyMask | PropertyChangeMask | EnterWindowMask)

typedef struct Page Page;
typedef struct Layout Layout;
typedef struct Area Area;
typedef struct Frame Frame;
typedef struct Client Client;

struct Page {
	Area *managed;
	Area *floating;
	Area *sel;
	File *file[P_LAST];
	Page *next;
	Page *prev;
};

struct Layout {
	char *name;
	void (*init) (Area *, Client *); /* called when layout is initialized */
	Client *(*deinit) (Area *); /* called when layout is uninitialized */
	void (*arrange) (Area *); /* called when area is resized */
	Bool (*attach) (Area *, Client *); /* called on attach */
	void (*detach) (Area *, Client *, Bool unmap); /* called on detach */
	void (*resize) (Frame *, XRectangle *, XPoint *); /* called after resize */
	void (*focus) (Frame *, Bool raise); /* focussing a frame */
	Frame *(*frames) (Area *); /* called for drawing */
	Frame *(*sel) (Area *); /* returns selected frame */
	Action *(*actions) (Area *); /* local action table */
	Layout *next;
};

struct Area {
	Page *page;
	Layout *layout;
	void *aux;					/* free pointer */
	File *file[A_LAST];
};

struct Frame {
	Area *area;
	Window win;
	Client *sel;
	Client *clients;
	size_t nclients;
	GC gc;
	XRectangle rect;
	Cursor cursor;
	void *aux;					/* free pointer */
	File *file[F_LAST];
	Frame *next;
	Frame *prev;
};

struct Client {
	int proto;
	unsigned int border;
	unsigned int ignore_unmap;
	Bool destroyed;
	Window win;
	Window trans;
	XRectangle rect;
	XSizeHints size;
	Frame *frame;
	File *file[C_LAST];
	Client *next;
	Client *prev;
};


/* global variables */
Page *pages;
Page *selpage;
size_t npages;
Client *detached;
size_t ndetached;
Layout *layouts;

Display *dpy;
IXPServer *ixps;
int screen_num;
Window root;
Window transient;
XRectangle rect;
XRectangle area_rect;
XFontStruct *font;
XColor xorcolor;
GC xorgc;
GC transient_gc;

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
Area *alloc_area(Page *p, char *layout);
void destroy_area(Area *a);
void focus_area(Area *a);
void hide_area(Area *a);
void show_area(Area *a, Bool raise);
Area *sel_area();
void attach_frame_to_area(Area *a, Frame *f);
void detach_frame_from_area(Frame *f);

/* client.c */
Client *alloc_client(Window w);
void init_client(Client * c, XWindowAttributes * wa);
void destroy_client(Client * c);
void configure_client(Client * c);
void handle_client_property(Client * c, XPropertyEvent * e);
void close_client(Client * c);
void draw_client(Client *client);
void draw_clients(Frame * f);
void gravitate(Client * c, unsigned int tabh, unsigned int bw, int invert);
void grab_client(Client * c, unsigned long mod, unsigned int button);
void ungrab_client(Client * c, unsigned long mod, unsigned int button);
void hide_client(Client * c);
void show_client(Client * c);
void reparent_client(Client * c, Window w, int x, int y);
void focus_client(Client *c);
void attach_client(Client *c);
void detach_client(Client *c, Bool unmap);
Client *sel_client();
Client *clientat(Client *clients, size_t idx);
void detach_detached(Client *c);
void attach_detached(Client *c);

/* frame.c */
Frame *win_to_frame(Window w);
Frame *alloc_frame(XRectangle * r);
void destroy_frame(Frame * f);
void resize_frame(Frame *f, XRectangle *r, XPoint *pt);
void draw_frame(Frame *f);
void handle_frame_buttonpress(XButtonEvent *e, Frame *f);
void attach_client_to_frame(Frame *f, Client *client);
void detach_client_from_frame(Client *client, Bool unmap);
unsigned int tab_height(Frame * f);
unsigned int border_width(Frame * f);
Frame *sel_frame();
Frame *bottom_frame(Frame *frames);

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
void center_pointer(Frame * f);

/* page.c */
Page *pageat(unsigned int idx);
Page *alloc_page();
void free_page(Page * p);
void destroy_page(Page * p);
void focus_page(Page *p);
XRectangle *rectangles(unsigned int *num);
void hide_page(Page * p);
void show_page(Page * p);

/* layout.c */
Layout *match_layout(char *name);

/* layoutdef.c */
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
