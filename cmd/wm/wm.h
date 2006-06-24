/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdio.h>
#include <regex.h>
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

enum { MIN_COLWIDTH = 64 };
enum { WM_PROTOCOL_DELWIN = 1 };

typedef struct View View;
typedef struct Area Area;
typedef struct Frame Frame;
typedef struct Client Client;

struct View {
	View *next;
	char name[256];
	unsigned short id;
	Area *area;
	Area *sel;
	Area *revert;
};

typedef struct ViewLink ViewLink;
struct ViewLink {
	ViewLink *next;
	View *view;
};

struct Area {
	Area *next;
	Frame *frame;
	Frame *sel;
	View *view;
	Bool floating;
	unsigned short id;
	int mode;
	XRectangle rect;
};

struct Frame {
	Frame *cnext;
	Frame *anext;
	Area *area;
	unsigned short id;
	XRectangle rect;
	XRectangle revert;
	Client *client;
	Bool collapsed;
	BlitzBrush tile;
	BlitzInput tagbar;
	BlitzBrush titlebar;
	BlitzBrush posbar;
};

struct Client {
	Client *next;
	ViewLink *views;
	Area *revert;
	Frame *frame;
	Frame *sel;
	char name[256];
	char tags[256];
	char props[512];
	unsigned short id;
	unsigned int border;
	int proto;
	Bool floating;
	Bool fixedsize;
	Window win;
	Window trans;
	Window framewin;
	XRectangle rect;
	XSizeHints size;
	GC gc;
};

typedef struct Key Key;
struct Key {
	Key *next;
	Key *lnext;
	Key *tnext;
	unsigned short id;
	char name[128];
	unsigned long mod;
	KeyCode key;
};

typedef struct Bar Bar;
struct Bar {
	Bar *next;
	char buf[280];
	char text[256];
	char name[256];
	unsigned short id;
	BlitzBrush brush;
};

typedef struct Rule Rule;
struct Rule {
	Rule *next;
	regex_t regex;
	char value[256];
};

typedef struct Rules {
	Rule		*rule;
	char		*string;
	unsigned int	size;
} Rules;

/* default values */
typedef struct {
	BlitzColor selcolor;
	BlitzColor normcolor;
	BlitzFont font;
	unsigned int border;
	unsigned int snap;
	char *keys;
	char *testtags;
	unsigned int keyssz;
	Rules	tagrules;
	Rules	colrules;
	char grabmod[5];
	unsigned long mod;
	int colmode;
} Default;

/* global variables */
View *view;
Client *client;
Key *key;
Bar *lbar;
Bar *rbar;

View *sel;
P9Srv p9srv;
Blitz blz;
XRectangle rect;
IXPServer srv;
Window barwin;
GC xorgc;
BlitzBrush bbrush;
XRectangle brect;
char *user;
Default def;
Atom wm_atom[WMLast];
Atom net_atom[NetLast];
Atom tags_atom;
Cursor cursor[CurLast];
unsigned int valid_mask;
unsigned int num_lock_mask;
Bool sel_screen;
Bool starting;
Pixmap pmap;
void (*handler[LASTEvent]) (XEvent *);

/* wm.c */
char *message_root(char *message);

/* area.c */
Area *create_area(View *v, Area *pos, unsigned int w);
void destroy_area(Area *a);
Area *area_of_id(View *t, unsigned short id);
char *select_area(Area *a, char *arg);
void send_to_area(Area *to, Area *from, Client *c);
void attach_to_area(Area *a, Client *c, Bool send);
void detach_from_area(Area *a, Client *c);
Bool is_of_area(Area *a, Client *c);
int idx_of_area(Area *a);
Client *sel_client_of_area(Area *a);

/* bar.c */
Bar *create_bar(Bar **b_link, char *name);
void destroy_bar(Bar **b_link, Bar *b);
void draw_bar();
Bar *bar_of_id(unsigned short id);
void resize_bar();
unsigned int height_of_bar();
Bar *bar_of_name(const char *name);

/* client.c */
Client *create_client(Window w, XWindowAttributes *wa);
void destroy_client(Client *c);
void configure_client(Client *c);
void prop_client(Client *c, XPropertyEvent *e);
void kill_client(Client *c);
void gravitate_client(Client *c, Bool invert);
void unmap_client(Client *c);
void map_client(Client *c);
void reparent_client(Client *c, Window w, int x, int y);
void manage_client(Client *c);
void focus_client(Client *c, Bool restack);
void focus(Client *c, Bool restack);
void resize_client(Client *c, XRectangle *r, Bool ignore_xcall);
void match_sizehints(Client *c, XRectangle *r, Bool floating, BlitzAlign sticky);
char *send_client(Frame *f, char *arg);
char * message_client(Client *c, char *message);
void move_client(Client *c, char *arg);
void size_client(Client *c, char *arg);
void newcol_client(Client *c, char *arg);
void resize_all_clients();
Client *sel_client();
Client *client_of_id(unsigned short id);
int idx_of_client_id(unsigned short id);
Client *client_of_win(Window w);
int idx_of_client(Client *c);
void update_client_grab(Client *c, Bool is_sel);
void apply_rules(Client *c);
void apply_tags(Client *c, const char *tags);

/* column.c */
void arrange_column(Area *a, Bool dirty);
void scale_column(Area *a, float h);
void resize_column(Client *c, XRectangle *r, XPoint *pt);
int column_mode_of_str(char *arg);
char *str_of_column_mode(int mode);
Area *new_column(View *v, Area *pos, unsigned int w);

/* event.c */
void check_x_event(IXPConn *c);
unsigned int flush_masked_events(long even_mask);

/* frame.c */
Frame *create_frame(Area *a, Client *c);
void destroy_frame(Frame *f);
void remove_frame(Frame *f);
void insert_frame(Frame *pos, Frame *f, Bool before);
int idx_of_frame(Frame *f);
Frame *frame_of_id(Area *a, unsigned short id);
Client *frame_of_win(Window w);
void draw_frame(Frame *f);
void draw_frames();
void resize_frame(Frame *f);
void update_frame_widget_colors(Frame *f);

/* fs.c */
void fs_attach(P9Req *r);
void fs_clunk(P9Req *r);
void fs_create(P9Req *r);
void fs_flush(P9Req *r);
void fs_freefid(Fid *f);
void fs_open(P9Req *r);
void fs_read(P9Req *r);
void fs_remove(P9Req *r);
void fs_stat(P9Req *r);
void fs_walk(P9Req *r);
void fs_write(P9Req *r);
void write_event(char *format, ...);

/* geom.c */
BlitzAlign quadofcoord(XRectangle *rect, int x, int y);
Bool ispointinrect(int x, int y, XRectangle * r);
int strtorect(XRectangle *r, const char *val);

/* key.c */
void handle_key(Window w, unsigned long mod, KeyCode keycode);
void update_keys();
void init_lock_keys();
unsigned long mod_key_of_str(char *val);

/* mouse.c */
void do_mouse_resize(Client *c,BlitzAlign align);
void grab_mouse(Window w, unsigned long mod, unsigned int button);
void ungrab_mouse(Window w, unsigned long mod, unsigned int button);
BlitzAlign snap_rect(XRectangle *rects, int num, XRectangle *current,
					 BlitzAlign *mask, int snap);

/* rule.c */
void update_rules(Rule **rule, const char *data);

/* view.c */
void arrange_view(View *v);
void scale_view(View *v, float w);
View *create_view(const char *name);
void focus_view(View *v);
void update_client_views(Client *c);
XRectangle *rects_of_view(View *v, unsigned int *num);
View *view_of_id(unsigned short id);
void select_view(const char *arg);
void detach_from_view(View *v, Client *c);
void attach_to_view(View *v, Client *c);
Client *sel_client_of_view(View *v);
char *message_view(View *v, char *message);
void restack_view(View *v);
View *view_of_name(const char *name);
unsigned char * view_index(View *v);
void destroy_view(View *v);
void update_views();
unsigned int newcolw_of_view(View *v);

/* wm.c */
int win_proto(Window w);
int win_state(Window w);
int wmii_error_handler(Display *dpy, XErrorEvent *error);
