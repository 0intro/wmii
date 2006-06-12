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
	FsFtagrules,
	FsFcolrules,
	FsFprops,
	FsFmode,
	FsFtags,
	FsFindex,
	FsLast
};

enum { MIN_COLWIDTH = 64 };
enum { WM_PROTOCOL_DELWIN = 1 };

typedef struct View View;
#define VIEW(p) ((View *)(p))
typedef struct Area Area;
#define AREA(p) ((Area *)(p))
typedef struct Frame Frame;
#define FRAME(p) ((Frame *)(p))
typedef struct Client Client;
#define CLIENT(p) ((Client *)(p))

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

#define BAR(p) ((Bar *)(p))
typedef struct Bar Bar;
struct Bar {
	Bar *next;
	char name[256];
	char data[256];
	char colstr[24];
	unsigned short id;
	BlitzColor color;
	XRectangle rect;
};

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
	char *tagrules;
	unsigned int tagrulessz;
	char *colrules;
	unsigned int colrulessz;
	char grabmod[5];
	unsigned long mod;
	int colmode;
} Default;

typedef struct {
	union {
		Qid qid;
		struct {
			unsigned char type;
			unsigned int version;
			unsigned char ptype;
			unsigned short i1id;
			unsigned short i2id;
			unsigned short i3id;
		};
	};
} PackedQid;

/* global variables */
typedef struct Rule Rule;
struct Rule {
	Rule *next;
	regex_t regex;
	char value[256];
};

/* global variables */
View *view;
Client *client;
Key *key;
Bar *bar;
Rule *trule;
Rule *vrule;

View *sel;
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
PackedQid root_qid;
Default def;
Atom wm_atom[WMLast];
Atom net_atom[NetLast];
Cursor cursor[CurLast];
unsigned int valid_mask;
unsigned int num_lock_mask;
void (*handler[LASTEvent]) (XEvent *);


/* area.c */
Area *create_area(View *v, Area *pos, unsigned int w);
void destroy_area(Area *a);
Area *area_of_id(View *t, unsigned short id);
void select_area(Area *a, char *arg);
void send_to_area(Area *to, Area *from, Client *c);
void attach_to_area(Area *a, Client *c, Bool send);
void detach_from_area(Area *a, Client *c);
Bool is_of_area(Area *a, Client *c);
int idx_of_area(Area *a);
Client *sel_client_of_area(Area *a);

/* bar.c */
Bar *create_bar(char *name);
void destroy_bar(Bar *b);
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
Client *selected_client();
void match_sizehints(Client *c, XRectangle *r, Bool floating, BlitzAlign sticky);
void send_client(Client *c, char *arg);
void move_client(Client *c, char *arg);
void size_client(Client *c, char *arg);
void newcol_client(Client *c, char *arg);
void resize_all_clients();
Client *sel_client();
Client *client_of_id(unsigned short id);
int idx_of_client_id(unsigned short id);
Client *client_of_win(Window w);
int idx_of_client(Client *c);
void draw_clients();
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
void init_x_event_handler();
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
XRectangle *rects_of_view(View *v, unsigned int *num);
View *view_of_id(unsigned short id);
void select_view(const char *arg);
void detach_from_view(View *v, Client *c);
void attach_to_view(View *v, Client *c);
Client *sel_client_of_view(View *v);
void restack_view(View *v);
View *view_of_name(const char *name);
void destroy_view(View *v);
void update_views();
unsigned int newcolw_of_view(View *v);

/* wm.c */
void scan_wins();
int win_proto(Window w);
int win_state(Window w);
int wmii_error_handler(Display *dpy, XErrorEvent *error);
