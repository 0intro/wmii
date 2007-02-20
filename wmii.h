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
extern Bool starting;
extern Bool verbose;

/* wm.c */
extern char *message_root(char *message);

/* area.c */
extern Area *create_area(View *v, Area *pos, uint w);
extern void destroy_area(Area *a);
extern Area *area_of_id(View *t, ushort id);
extern void focus_area(Area *a);
extern char *select_area(Area *a, char *arg);
extern void send_to_area(Area *to, Frame *f);
extern void attach_to_area(Area *a, Frame *f, Bool send);
extern void detach_from_area(Frame *f);
extern Client *sel_client_of_area(Area *a);

/* bar.c */
extern Bar *create_bar(Bar **b_link, char *name);
extern void destroy_bar(Bar **b_link, Bar *b);
extern void draw_bar(WMScreen *s);
void draw_border(BlitzBrush *b);
extern void resize_bar();
extern Bar *bar_of_name(Bar *b_link, const char *name);

/* client.c */
extern Client *create_client(Window w, XWindowAttributes *wa);
extern void destroy_client(Client *c);
extern void configure_client(Client *c);
extern void prop_client(Client *c, XPropertyEvent *e);
extern void kill_client(Client *c);
extern void gravitate_client(Client *c, Bool invert);
extern void map_client(Client *c);
extern void unmap_client(Client *c, int state);
extern void map_frame(Client *c);
extern void unmap_frame(Client *c);
extern void set_cursor(Client *c, Cursor cur);
extern void focus_frame(Frame *f, Bool restack);
extern void reparent_client(Client *c, Window w, int x, int y);
extern void manage_client(Client *c);
extern void focus(Client *c, Bool restack);
extern void focus_client(Client *c);
extern void resize_client(Client *c, XRectangle *r);
extern void match_sizehints(Client *c, XRectangle *r, Bool floating, BlitzAlign sticky);
extern char *send_client(Frame *f, char *arg, Bool swap);
extern char * message_client(Client *c, char *message);
extern void move_client(Client *c, char *arg);
extern void size_client(Client *c, char *arg);
extern Client *sel_client();
extern Frame *frame_of_win(Window w);
extern Client *client_of_win(Window w);
extern void update_client_grab(Client *c);
extern void apply_rules(Client *c);
extern void apply_tags(Client *c, const char *tags);

/* column.c */
extern void arrange_column(Area *a, Bool dirty);
extern void resize_column(Client *c, XRectangle *r);
extern int column_mode_of_str(char *arg);
extern char *str_of_column_mode(int mode);
extern Area *new_column(View *v, Area *pos, uint w);

/* draw.c */
extern int loadcolor(Blitz *blitz, BlitzColor *c);
extern void draw_label(BlitzBrush *b, char *text);
extern void draw_tile(BlitzBrush *b);
extern void draw_rect(BlitzBrush *b);

extern void drawbg(Display *dpy, Drawable drawable, GC gc,
		XRectangle *rect, BlitzColor c, Bool fill, Bool border);
extern void drawcursor(Display *dpy, Drawable drawable, GC gc,
				int x, int y, uint h, BlitzColor c);
extern uint textwidth(BlitzFont *font, char *text);
extern uint textwidth_l(BlitzFont *font, char *text, uint len);
extern void loadfont(Blitz *blitz, BlitzFont *font);
extern uint labelh(BlitzFont *font);
extern char *parse_colors(char **buf, int *buflen, BlitzColor *col);

/* event.c */
extern void check_x_event(IXPConn *c);
extern uint flush_masked_events(long even_mask);

/* frame.c */
extern Frame *create_frame(Client *c, View *v);
extern void remove_frame(Frame *f);
extern void insert_frame(Frame *pos, Frame *f, Bool before);
extern void resize_frame(Frame *f, XRectangle *r);
extern Bool frame_to_top(Frame *f);
extern void swap_frames(Frame *fa, Frame *fb);
extern int frame_delta_h();
extern void draw_frame(Frame *f);
extern void draw_frames();
extern void update_frame_widget_colors(Frame *f);
void check_frame_constraints(XRectangle *rect);

/* fs.c */
extern void fs_attach(P9Req *r);
extern void fs_clunk(P9Req *r);
extern void fs_create(P9Req *r);
extern void fs_flush(P9Req *r);
extern void fs_freefid(Fid *f);
extern void fs_open(P9Req *r);
extern void fs_read(P9Req *r);
extern void fs_remove(P9Req *r);
extern void fs_stat(P9Req *r);
extern void fs_walk(P9Req *r);
extern void fs_write(P9Req *r);
extern void write_event(char *format, ...);

/* geom.c */
extern Bool ispointinrect(int x, int y, XRectangle * r);
extern BlitzAlign quadofcoord(XRectangle *rect, int x, int y);
extern Cursor cursor_of_quad(BlitzAlign align);
extern int strtorect(XRectangle *r, const char *val);
extern BlitzAlign get_sticky(XRectangle *src, XRectangle *dst);
extern int r_east(XRectangle *r);
extern int r_south(XRectangle *r);

/* key.c */
extern void kpress(Window w, ulong mod, KeyCode keycode);
extern void update_keys();
extern void init_lock_keys();
extern ulong mod_key_of_str(char *val);

/* mouse.c */
extern void do_mouse_resize(Client *c, Bool grabbox, BlitzAlign align);
extern void grab_mouse(Window w, ulong mod, ulong button);
extern void ungrab_mouse(Window w, ulong mod, uint button);
extern BlitzAlign snap_rect(XRectangle *rects, int num, XRectangle *current,
					 BlitzAlign *mask, int snap);
extern void grab_button(Window w, uint button, ulong mod);

/* rule.c */
extern void update_rules(Rule **rule, const char *data);
extern void trim(char *str, const char *chars);

/* util.c */
extern uint tokenize(char *res[], uint reslen, char *str, char delim);
extern char *estrdup(const char *str);
extern void *erealloc(void *ptr, uint size);
extern void *emallocz(uint size);
extern void *emalloc(uint size);
extern void fatal(const char *fmt, ...);
extern int max(int a, int b);
extern char *str_nil(char *s);

/* view.c */
extern void arrange_view(View *v);
extern void scale_view(View *v, float w);
extern View *get_view(const char *name);
extern View *create_view(const char *name);
extern void focus_view(WMScreen *s, View *v);
extern void update_client_views(Client *c, char **tags);
extern XRectangle *rects_of_view(View *v, uint *num, Frame *ignore);
extern View *view_of_id(ushort id);
extern void select_view(const char *arg);
extern void attach_to_view(View *v, Frame *f);
extern Client *sel_client_of_view(View *v);
extern char *message_view(View *v, char *message);
extern void restack_view(View *v);
extern uchar *view_index(View *v);
extern void destroy_view(View *v);
extern void update_views();
extern uint newcolw_of_view(View *v);

/* wm.c */
extern int wmii_error_handler(Display *dpy, XErrorEvent *error);
extern int win_proto(Window w);
