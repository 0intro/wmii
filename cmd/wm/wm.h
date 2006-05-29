/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <ixp.h>
#include <blitz.h>

/* WM atoms */
enum {
	WMState,
	WMProtocols,
	WMDelete,
	WMLast
};

/* NET atoms */
enum {
	NetSupported,
	NetWMName,
	NetLast
};

/* Column modes */
enum {
	Coldefault,
	Colstack,
	Colmax
};

/* Cursor */
enum {
	CurNormal,
	CurResize,
	CurMove,
	CurLast
};

/* 8-bit qid.path.type */
enum {                          
	FsDroot,
	FsDdef,
	FsDtag,
	FsDview,
	FsDarea,
	FsDclients,
	FsDclient,
	FsDGclient,
	FsDbars,
	FsDbar,
	FsFdata,
	FsFcolors,
	FsFfont,
	FsFselcolors,
	FsFnormcolors,
	FsFkeys,
	FsFgrabmod,
	FsFborder,
	FsFbar,
	FsFgeom,
	FsFevent,
	FsFctl,
	FsFname,
	FsFrules,
	FsFclass,
	FsFmode,
	FsFtags,
	FsFindex,
	FsFcolw,
	FsLast
};

enum { MIN_COLWIDTH = 64 };
enum { WM_PROTOCOL_DELWIN = 1 };

typedef struct View View;
typedef struct Area Area;
typedef struct Frame Frame;
typedef struct Client Client;

VECTOR(AreaVector, Area *);
struct View {
	char name[256];
	unsigned short id;
	AreaVector area;
	unsigned int sel;
	unsigned int revert;
};

VECTOR(FrameVector, Frame *);
struct Area {
	unsigned short id;
	FrameVector frame;
	View *view;
	unsigned int sel;
	int mode;
	XRectangle rect;
};

struct Frame {
	Area *area;
	unsigned short id;
	XRectangle rect;
	Client *client;
};

VECTOR(ViewVector, View *);
struct Client {
	unsigned short id;
	char name[256];
	char tags[256];
	ViewVector view;
	char classinst[256];
	int proto;
	unsigned int border;
	Bool floating;
	Bool fixedsize;
	Window win;
	Window trans;
	XRectangle rect;
	XSizeHints size;
	Window framewin;
	GC gc;
	FrameVector frame;
	unsigned int sel;
	Area *revert;
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
	char name[256];
	unsigned short id;
	char data[256];
	char colstr[24];
	BlitzColor color;
	XRectangle rect;
	Bool intern;
} Bar;

/* default values */
typedef struct {
	char selcolor[24];
	char normcolor[24];
	char *font;
	BlitzColor sel;
	BlitzColor norm;
	unsigned int border;
	unsigned int snap;
	char *keys;
	unsigned int keyssz;
	char *rules;
	unsigned int rulessz;
	char grabmod[5];
	unsigned long mod;
	int colmode;
	unsigned int colw;
} Default;

/* global variables */
VECTOR(ClientVector, Client *);
VECTOR(KeyVector, Key *);
VECTOR(BarVector, Bar *);

/* global variables */
ViewVector view;
unsigned int sel;
ClientVector client;
KeyVector key;
BarVector bar;
Display *dpy;
int screen;
Window root;
XRectangle rect;
BlitzFont blitzfont;
IXPServer srv;
Pixmap barpmap;
Window barwin;
GC bargc;
GC xorgc;
XRectangle brect;
Qid root_qid;
Default def;
Atom wm_atom[WMLast];
Atom net_atom[NetLast];
Cursor cursor[CurLast];
unsigned int valid_mask;
unsigned int num_lock_mask;
void (*handler[LASTEvent]) (XEvent *);

/* area.c */
Area *create_area(View *v, unsigned int pos);
void destroy_area(Area *a);
int idx_of_area(Area *a);
int idx_of_area_id(View *t, unsigned short id);
void select_area(Area *a, char *arg);
void send_to_area(Area *to, Area *from, Client *c);
void attach_to_area(Area *a, Client *c);
void detach_from_area(Area *a, Client *c);
Bool is_of_area(Area *a, Client *c);
Client *sel_client_of_area(Area *a);

/* bar.c */
Bar *create_bar(char *name, Bool intern);
void destroy_bar(Bar *b);
void draw_bar();
int idx_of_bar_id(unsigned short id);
void resize_bar();
unsigned int height_of_bar();
Bar *bar_of_name(const char *name);
int idx_of_bar(Bar *b);
void update_view_bars();

/* client.c */
Client *create_client(Window w, XWindowAttributes *wa);
void destroy_client(Client *c);
void configure_client(Client *c);
void prop_client(Client *c, XPropertyEvent *e);
void kill_client(Client *c);
void draw_client(Client *client);
void gravitate_client(Client *c, Bool invert);
void unmap_client(Client *c);
void map_client(Client *c);
void reparent_client(Client *c, Window w, int x, int y);
void manage_client(Client *c);
void focus_client(Client *c, Bool restack);
void focus(Client *c, Bool restack);
void resize_client(Client *c, XRectangle *r, Bool ignore_xcall);
void select_client(Client *c, char *arg);
void send_client(Client *c, char *arg);
void move_client(Client *c, char *arg);
void size_client(Client *c, char *arg);
void newcol_client(Client *c, char *arg);
void resize_all_clients();
Client *sel_client();
int idx_of_client_id(unsigned short id);
Client *client_of_win(Window w);
void draw_clients();
void update_client_grab(Client *c, Bool is_sel);

/* column.c */
void arrange_column(Area *a, Bool dirty);
void scale_column(Area *a, float h);
void resize_column(Client *c, XRectangle *r, XPoint *pt);
int column_mode_of_str(char *arg);
char *str_of_column_mode(int mode);
Area *new_column(View *v, unsigned int pos);

/* event.c */
void init_x_event_handler();
void check_x_event(IXPConn *c);
unsigned int flush_masked_events(long even_mask);

/* frame.c */
Vector *vector_of_frames(FrameVector *fv);
Frame *create_frame(Area *a, Client *c);
void destroy_frame(Frame *f);
int idx_of_frame_id(Area *a, unsigned short id);
int idx_of_frame(Frame *f);
Client *frame_of_win(Window w);

/* fs.c */
unsigned long long pack_qpath(unsigned char type, unsigned short i1,
		unsigned short i2, unsigned short i3);
void write_event(char *event);
void new_ixp_conn(IXPConn *c);

/* key.c */
void handle_key(Window w, unsigned long mod, KeyCode keycode);
void update_keys();
void init_lock_keys();
unsigned long mod_key_of_str(char *val);

/* mouse.c */
void do_mouse_resize(Client *c,BlitzAlign align);
void do_mouse_move(Client *c);
void grab_mouse(Window w, unsigned long mod, unsigned int button);
void ungrab_mouse(Window w, unsigned long mod, unsigned int button);
void snap_move(XRectangle *r, XRectangle *rects, unsigned int num,
		int snapw, int snaph);

/* rule.c */
void update_rules();
void apply_rules(Client *c);
Bool permit_tags(const char *tags);

/* view.c */
void arrange_view(View *v);
void scale_view(View *v, float w);
View *create_view(const char *name);
void focus_view(View *v);
XRectangle *rects_of_view(View *v, unsigned int *num);
int idx_of_view_id(unsigned short id);
void select_view(const char *arg);
int idx_of_view(View *v);
void detach_from_view(View *v, Client *c);
void attach_to_view(View *v, Client *c);
Client *sel_client_of_view(View *v);
void restack_view(View *v);
View *view_of_name(const char *name);
void destroy_view(View *v);
void update_views();

/* wm.c */
void scan_wins();
int win_proto(Window w);
int win_state(Window w);
int wmii_error_handler(Display *dpy, XErrorEvent *error);
