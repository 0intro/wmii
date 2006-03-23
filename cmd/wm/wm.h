/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "ixp.h"
#include "blitz.h"

/* WM atoms */
enum {
	WMState,
	WMProtocols,
	WMDelete,
	WMLast
};

/* Column modes */
enum {
	Colequal,
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
	FsDview,
	FsDarea,
	FsDclients,
	FsDclient,
	FsDGclient,
	FsDbar,
	FsDlabel,
	FsFexpand,
	FsFdata,
	FsFcolors,
	FsFfont,
	FsFselcolors,
	FsFnormcolors,
	FsFkeys,
	FsFborder,
	FsFbar,
	FsFgeom,
	FsFevent,
	FsFctl,
	FsFname,
	FsFrules,
	FsFtags,
	FsFclass,
	FsFtag,
	FsFmode
};

#define MAX_TAGS		8
#define MAX_TAGLEN		32
#define WM_PROTOCOL_DELWIN	1

typedef struct View View;
typedef struct Area Area;
typedef struct Frame Frame;
typedef struct Client Client;

struct View {
	char tag[MAX_TAGS][MAX_TAGLEN];
	unsigned int ntag;
	unsigned short id;
	Area **area;
	unsigned int areasz;
	unsigned int narea;
	unsigned int sel;
	unsigned int revert;
};

struct Area {
	unsigned short id;
	Frame **frame;
	View *view;
	unsigned int framesz;
	unsigned int sel;
	unsigned int nframe;
	int mode;
	XRectangle rect;
};

struct Frame {
	Area *area;
	unsigned short id;
	XRectangle rect;
	Client *client;
};

struct Client {
	unsigned short id;
	char name[256];
	char tag[MAX_TAGS][MAX_TAGLEN];
	unsigned int ntag;
	char classinst[256];
	int proto;
	unsigned int border;
	Bool destroyed;
	Window win;
	Window trans;
	XRectangle rect;
	XSizeHints size;
	Window framewin;
	GC gc;
	Frame **frame;
	unsigned int framesz;
	unsigned int sel;
	unsigned int nframe;
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
	Color color;
	XRectangle rect;
} Label;

/* default values */
typedef struct {
	char selcolor[24];
	char normcolor[24];
	char tag[256];
	char *font;
	Color sel;
	Color norm;
	unsigned int border;
	unsigned int snap;
	char *keys;
	unsigned int keyssz;
	char *rules;
	unsigned int rulessz;
} Default;

/* global variables */
View **view;
unsigned int nview;
unsigned int viewsz;
unsigned int sel;
Client **client;
unsigned int nclient;
unsigned int clientsz;
Key **key;
unsigned int keysz;
unsigned int nkey;
Label **label;
unsigned int nlabel;
unsigned int labelsz;
char expand[256];
char **tag;
unsigned int ntag;
unsigned int tagsz;

Display *dpy;
IXPServer *ixps;
int screen;
Window root;
XRectangle rect;
XFontStruct *xfont;
GC xorgc;
IXPServer srv;
Pixmap barpmap;
Window barwin;
GC bargc;
XRectangle brect;
Qid root_qid;

Default def;
Atom wm_atom[WMLast];

Cursor cursor[CurLast];
unsigned int valid_mask, num_lock_mask;

/* area.c */
Area *alloc_area(View *t);
void destroy_area(Area *a);
int area2index(Area *a);
int aid2index(View *t, unsigned short id);
void select_area(Area *a, char *arg);
void send2area(Area *to, Area *from, Client *c);
void attach_toarea(Area *a, Client *c);
void detach_fromarea(Area *a, Client *c);
void arrange_tag(View *t, Bool updategeometry);
void arrange_area(Area *a);
void resize_area(Client *c, XRectangle *r, XPoint *pt);
int str2mode(char *arg);
char *mode2str(int mode);
Bool clientofarea(Area *a, Client *c);

/* bar.c */
Label *get_label(char *name);
void destroy_label(Label *l);
void draw_bar();
int lid2index(unsigned short id);
void update_bar_geometry();
unsigned int bar_height();
Label *name2label(const char *name);
int label2index(Label *l);

/* client.c */
Client *alloc_client(Window w, XWindowAttributes *wa);
void configure_client(Client *c);
void update_client_property(Client *c, XPropertyEvent *e);
void kill_client(Client *c);
void draw_client(Client *client);
void gravitate(Client *c, Bool invert);
void unmap_client(Client *c);
void map_client(Client *c);
void reparent_client(Client *c, Window w, int x, int y);
void manage_client(Client *c);
void destroy_client(Client *c);
Client *sel_client();
void focus_client(Client *c);
void resize_client(Client *c, XRectangle *r, Bool ignore_xcall);
void select_client(Client *c, char *arg);
void send2area_client(Client *c, char *arg);
void resize_all_clients();
void focus(Client *c);
int cid2index(unsigned short id);
Bool clienthastag(Client *c, const char *t);

/* event.c */
void init_x_event_handler();
void check_x_event(IXPConn *c);

/* frame.c */
int frid2index(Area *a, unsigned short id);
int frame2index(Frame *f);
Client *win2clientframe(Window w);

/* fs.c */
unsigned long long mkqpath(unsigned char type, unsigned short pg,
		unsigned short area, unsigned short cl);
void write_event(char *event, Bool enqueue);
void new_ixp_conn(IXPConn *c);

/* kb.c */
void handle_key(Window w, unsigned long mod, KeyCode keycode);
void update_keys();
void init_lock_modifiers();

/* mouse.c */
void mouse_resize(Client *c, Align align);
void mouse_move(Client *c);
Align xy2align(XRectangle *rect, int x, int y);
void drop_move(Client *c, XRectangle *new, XPoint *pt);
void grab_mouse(Window w, unsigned long mod, unsigned int button);
void ungrab_mouse(Window w, unsigned long mod, unsigned int button);

/* rule.c */
void match_tags(Client *c);

/* tag.c */
unsigned int str2tags(char tags[MAX_TAGS][MAX_TAGLEN], const char *stags);
void tags2str(char *stags, unsigned int stagsz,
		char tags[MAX_TAGS][MAX_TAGLEN], unsigned int ntags);
Bool istag(char **tags, unsigned int ntags, char *tag);
void update_tags();

/* view.c */
View *alloc_view(char *name);
void focus_view(View *v);
XRectangle *rectangles(View *v, Bool isfloat, unsigned int *num);
int tid2index(unsigned short id);
void select_view(char *arg);
int view2index(View *v);
Bool clientofview(View *v, Client *c);
void detach_fromview(View *v, Client *c);
void attach_toview(View *v, Client *c);
Client *sel_client_of_view(View *v);
void restack_view(View *v);
Bool hasclient(View *v);

/* wm.c */
void scan_wins();
Client *win2client(Window w);
int win_proto(Window w);
int win_state(Window w);
int wmii_error_handler(Display *dpy, XErrorEvent *error);
