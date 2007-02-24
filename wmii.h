/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdio.h>
#include <regex.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <ixp.h>

#define nil	((void*)0)

/* Types */
#define uchar	_wmiiuchar
#define ushort	_wmiiushort
#define uint	_wmiiuint
#define ulong	_wmiiulong
#define vlong	_wmiivlong
#define uvlong	_wmiiuvlong
typedef unsigned char		uchar;
typedef unsigned short		ushort;
typedef unsigned int		uint;
typedef unsigned long		ulong;
typedef unsigned long long	uvlong;
typedef long long		vlong;

#define BLITZ_FONT		"-*-fixed-medium-r-normal-*-13-*-*-*-*-*-*-*"
#define BLITZ_FOCUSCOLORS	"#ffffff #335577 #447799"
#define BLITZ_NORMCOLORS	"#222222 #eeeeee #666666"

typedef struct Blitz Blitz;
typedef enum BlitzAlign BlitzAlign;
typedef struct BlitzColor BlitzColor;
typedef struct BlitzFont BlitzFont;
typedef struct BlitzBrush BlitzBrush;

struct Blitz {
	Display *dpy;
	int screen;
	Window root;
};

enum BlitzAlign {
	NORTH = 0x01,
	EAST  = 0x02,
	SOUTH = 0x04,
	WEST  = 0x08,
	NEAST = NORTH | EAST,
	NWEST = NORTH | WEST,
	SEAST = SOUTH | EAST,
	SWEST = SOUTH | WEST,
	CENTER = NEAST | SWEST
};

struct BlitzColor {
	vlong bg;
	vlong fg;
	vlong border;
	char colstr[24]; /* #RRGGBB #RRGGBB #RRGGBB */
};

struct BlitzFont {
	XFontStruct *xfont;
	XFontSet set;
	int ascent;
	int descent;
	uint height;
	char *fontstr;
};

struct BlitzBrush {
	Blitz *blitz;
	Drawable drawable;
	GC gc;
	int border;
	BlitzColor color;
	BlitzAlign align;
	BlitzFont *font;
	XRectangle rect;	/* relative rect */
};

/* WM atoms */
enum { WMState, WMProtocols, WMDelete, WMLast };

/* NET atoms */
enum { NetSupported, NetWMName, NetLast };

/* Column modes */
enum { Coldefault, Colstack, Colmax };

/* Cursor */
enum { CurNormal, CurNECorner, CurNWCorner, CurSECorner, CurSWCorner,
	CurMove, CurInput, CurInvisible, CurLast };

enum { NCOL = 16 };
enum { WM_PROTOCOL_DELWIN = 1 };

/* Data Structures */
typedef struct View View;
typedef struct Area Area;
typedef struct Frame Frame;
typedef struct Client Client;
typedef struct Key Key;
typedef struct Bar Bar;
typedef struct Rule Rule;
typedef struct Ruleset Ruleset;
typedef struct WMScreen WMScreen;

struct View {
	View *next;
	char name[256];
	ushort id;
	Area *area;
	Area *sel;
	Area *revert;
};

struct Area {
	Area *next;
	Frame *frame;
	Frame *stack;
	Frame *sel;
	View *view;
	Bool floating;
	ushort id;
	int mode;
	XRectangle rect;
};

struct Frame {
	Frame *cnext;
	Frame *anext;
	Frame *snext;
	View *view;
	Area *area;
	ushort id;
	XRectangle rect;
	XRectangle crect;
	XRectangle revert;
	Client *client;
	Bool collapsed;
	XRectangle grabbox;
	XRectangle titlebar;
};

struct Client {
	Client *next;
	Area *revert;
	Frame *frame;
	Frame *sel;
	char name[256];
	char tags[256];
	char props[512];
	uint border;
	int proto;
	Bool floating;
	Bool fixedsize;
	Bool fullscreen;
	Bool urgent;
	Bool mapped;
	Bool frame_mapped;
	int unmapped;
	Window win;
	Window trans;
	Window framewin;
	Cursor cursor;
	XRectangle rect;
	XSizeHints size;
	GC gc;
};

struct Key {
	Key *next;
	Key *lnext;
	Key *tnext;
	ushort id;
	char name[128];
	ulong mod;
	KeyCode key;
};

struct Bar {
	Bar *next;
	Bar *smaller;
	char buf[280];
	char text[256];
	char name[256];
	ushort id;
	BlitzBrush brush;
};

struct Rule {
	Rule *next;
	regex_t regex;
	char value[256];
};

struct Ruleset {
	Rule		*rule;
	char		*string;
	uint		size;
};

/* global variables */
struct {
	BlitzColor focuscolor;
	BlitzColor normcolor;
	BlitzFont font;
	uint	 border;
	uint	 snap;
	char *keys;
	uint	 keyssz;
	Ruleset	tagrules;
	Ruleset	colrules;
	char grabmod[5];
	ulong mod;
	int colmode;
} def;

struct WMScreen {
	Bar *lbar;
	Bar *rbar;
	View *sel;
	Client *focus;
	Client *hasgrab;
	Window barwin;

	XRectangle rect;
	XRectangle brect;
	BlitzBrush bbrush;
} *screens, *screen;

Client *client;
View *view;
Key *key;
Client c_magic;

enum { BUFFER_SIZE = 8092 };
char buffer[BUFFER_SIZE];

/* IXP */
IXPServer srv;
P9Srv p9srv;

/* X11 */
uint num_screens;
Blitz blz;
GC xorgc;
char *user;
Atom wm_atom[WMLast];
Atom net_atom[NetLast];
Atom tags_atom;
Cursor cursor[CurLast];
uint valid_mask;
uint num_lock_mask;
Bool sel_screen;
Pixmap pmap;
void (*handler[LASTEvent]) (XEvent *);

/* Misc */
Bool starting;
Bool verbose;

/* wm.c */
char *message_root(char *message);

/* area.c */
Area *create_area(View *v, Area *pos, uint w);
void destroy_area(Area *a);
Area *area_of_id(View *t, ushort id);
void focus_area(Area *a);
char *select_area(Area *a, char *arg);
void send_to_area(Area *to, Frame *f);
void attach_to_area(Area *a, Frame *f, Bool send);
void detach_from_area(Frame *f);
Client *sel_client_of_area(Area *a);

/* bar.c */
Bar *create_bar(Bar **b_link, char *name);
void destroy_bar(Bar **b_link, Bar *b);
void draw_bar(WMScreen *s);
void draw_border(BlitzBrush *b);
void resize_bar();
Bar *bar_of_name(Bar *b_link, const char *name);

/* client.c */
Client *create_client(Window w, XWindowAttributes *wa);
void destroy_client(Client *c);
void configure_client(Client *c);
void prop_client(Client *c, XPropertyEvent *e);
void kill_client(Client *c);
void gravitate_client(Client *c, Bool invert);
void map_client(Client *c);
void unmap_client(Client *c, int state);
void map_frame(Client *c);
void unmap_frame(Client *c);
void set_cursor(Client *c, Cursor cur);
void focus_frame(Frame *f, Bool restack);
void reparent_client(Client *c, Window w, int x, int y);
void manage_client(Client *c);
void focus(Client *c, Bool restack);
void focus_client(Client *c);
void resize_client(Client *c, XRectangle *r);
void match_sizehints(Client *c, XRectangle *r, Bool floating, BlitzAlign sticky);
char *send_client(Frame *f, char *arg, Bool swap);
char * message_client(Client *c, char *message);
void move_client(Client *c, char *arg);
void size_client(Client *c, char *arg);
Client *sel_client();
Frame *frame_of_win(Window w);
Client *client_of_win(Window w);
void update_client_grab(Client *c);
void apply_rules(Client *c);
void apply_tags(Client *c, const char *tags);

/* column.c */
void arrange_column(Area *a, Bool dirty);
void resize_column(Client *c, XRectangle *r);
int column_mode_of_str(char *arg);
char *str_of_column_mode(int mode);
Area *new_column(View *v, Area *pos, uint w);

/* draw.c */
int loadcolor(Blitz *blitz, BlitzColor *c);
void draw_label(BlitzBrush *b, char *text);
void draw_tile(BlitzBrush *b);
void draw_rect(BlitzBrush *b);

void drawbg(Display *dpy, Drawable drawable, GC gc,
		XRectangle *rect, BlitzColor c, Bool fill, Bool border);
void drawcursor(Display *dpy, Drawable drawable, GC gc,
				int x, int y, uint h, BlitzColor c);
uint textwidth(BlitzFont *font, char *text);
uint textwidth_l(BlitzFont *font, char *text, uint len);
void loadfont(Blitz *blitz, BlitzFont *font);
uint labelh(BlitzFont *font);
char *parse_colors(char **buf, int *buflen, BlitzColor *col);

/* event.c */
void check_x_event(IXPConn *c);
uint flush_masked_events(long even_mask);

/* frame.c */
Frame *create_frame(Client *c, View *v);
void remove_frame(Frame *f);
void insert_frame(Frame *pos, Frame *f, Bool before);
void resize_frame(Frame *f, XRectangle *r);
Bool frame_to_top(Frame *f);
void set_frame_cursor(Frame *f, int x, int y);
void swap_frames(Frame *fa, Frame *fb);
int frame_delta_h();
void draw_frame(Frame *f);
void draw_frames();
void update_frame_widget_colors(Frame *f);
void check_frame_constraints(XRectangle *rect);

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
Bool ispointinrect(int x, int y, XRectangle * r);
BlitzAlign quadofcoord(XRectangle *rect, int x, int y);
Cursor cursor_of_quad(BlitzAlign align);
int strtorect(XRectangle *r, const char *val);
BlitzAlign get_sticky(XRectangle *src, XRectangle *dst);
int r_east(XRectangle *r);
int r_south(XRectangle *r);

/* key.c */
void kpress(Window w, ulong mod, KeyCode keycode);
void update_keys();
void init_lock_keys();
ulong mod_key_of_str(char *val);

/* mouse.c */
void do_mouse_resize(Client *c, Bool grabbox, BlitzAlign align);
void grab_mouse(Window w, ulong mod, ulong button);
void ungrab_mouse(Window w, ulong mod, uint button);
BlitzAlign snap_rect(XRectangle *rects, int num, XRectangle *current,
					 BlitzAlign *mask, int snap);
void grab_button(Window w, uint button, ulong mod);

/* rule.c */
void update_rules(Rule **rule, const char *data);
void trim(char *str, const char *chars);

/* util.c */
uint tokenize(char *res[], uint reslen, char *str, char delim);
char *estrdup(const char *str);
void *erealloc(void *ptr, uint size);
void *emallocz(uint size);
void *emalloc(uint size);
void fatal(const char *fmt, ...);
int max(int a, int b);
char *str_nil(char *s);

/* view.c */
void arrange_view(View *v);
void scale_view(View *v, float w);
View *get_view(const char *name);
View *create_view(const char *name);
void focus_view(WMScreen *s, View *v);
void update_client_views(Client *c, char **tags);
XRectangle *rects_of_view(View *v, uint *num, Frame *ignore);
View *view_of_id(ushort id);
void select_view(const char *arg);
void attach_to_view(View *v, Frame *f);
Client *sel_client_of_view(View *v);
char *message_view(View *v, char *message);
void restack_view(View *v);
uchar *view_index(View *v);
void destroy_view(View *v);
void update_views();
uint newcolw_of_view(View *v);

/* wm.c */
int wmii_error_handler(Display *dpy, XErrorEvent *error);
int win_proto(Window w);
