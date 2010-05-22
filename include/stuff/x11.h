/* Copyright ©2007-2010 Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */
#define Window XWindow
#define Font XFont
#define Screen XScreen
#include <stuff/geom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xft/Xft.h>
#include <X11/extensions/Xrender.h>
#ifdef _X11_VISIBLE
#  include <X11/Xatom.h>
#  include <X11/extensions/shape.h>
#  include <X11/extensions/Xrandr.h>
#endif
#undef Window
#undef Font
#undef Screen

enum FontType {
	FX11 = 1,
	FFontSet,
	FXft,
};

enum WindowType {
	WWindow,
	WImage,
};

typedef enum FontType FontType;
typedef enum WindowType WindowType;

typedef XSetWindowAttributes WinAttr;

typedef struct Color Color;
typedef struct CTuple CTuple;
typedef struct ErrorCode ErrorCode;
typedef struct Ewmh Ewmh;
typedef struct Font Font;
typedef struct Handlers Handlers;
typedef struct Screen Screen;
typedef struct WinHints WinHints;
typedef struct Window Image;
typedef struct Window Window;

struct Color {
	ulong		pixel;
	XRenderColor	render;
};

struct CTuple {
	Color bg;
	Color fg;
	Color border;
	char colstr[24]; /* #RRGGBB #RRGGBB #RRGGBB */
};

struct ErrorCode {
	uchar rcode;
	uchar ecode;
};

struct Ewmh {
	long	type;
	long	ping;
	long	timer;
};

struct Font {
	int	type;
	union {
		XFontStruct*	x11;
		XFontSet	set;
		XftFont*	xft;
	} font;
	Rectangle pad;
	int	ascent;
	int	descent;
	uint	height;
	char*	name;
};

struct Handlers {
	Rectangle (*dndmotion)(Window*, Point);
	void (*bdown)(Window*, XButtonEvent*);
	void (*bup)(Window*, XButtonEvent*);
	void (*config)(Window*, XConfigureEvent*);
	void (*configreq)(Window*, XConfigureRequestEvent*);
	void (*destroy)(Window*, XDestroyWindowEvent*);
	void (*enter)(Window*, XCrossingEvent*);
	void (*expose)(Window*, XExposeEvent*);
	void (*focusin)(Window*, XFocusChangeEvent*);
	void (*focusout)(Window*, XFocusChangeEvent*);
	void (*kdown)(Window*, XKeyEvent*);
	void (*kup)(Window*, XKeyEvent*);
	void (*leave)(Window*, XCrossingEvent*);
	void (*map)(Window*, XMapEvent*);
	void (*mapreq)(Window*, XMapRequestEvent*);
	void (*motion)(Window*, XMotionEvent*);
	void (*property)(Window*, XPropertyEvent*);
	void (*unmap)(Window*, XUnmapEvent*);
};

struct WinHints {
	Point	min;
	Point	max;
	Point	base;
	Point	baspect;
	Point	inc;
	Point	grav;
	Rectangle aspect;
	XWindow	group;
	bool	gravstatic;
	bool	position;
};

struct Window {
	int		type;
	XID		xid;
	GC		gc;
	Visual*		visual;
	Colormap	colormap;
	XftDraw*	xft;
	Rectangle	r;
	int		border;
	Window*		parent;
	Window*		next;
	Window*		prev;
	Handlers*	handler;
	WinHints*	hints;
	Ewmh		ewmh;
	void*		dnd;
	void*		aux;
	bool		mapped;
	int		unmapped;
	int		depth;
};

struct Screen {
	int		screen;
	Window		root;
	GC		gc;
	Colormap	colormap;
	Visual*		visual;
	Visual*		visual32;
	Rectangle	rect;
	int		depth;
	int		fd;
	ulong		black;
	ulong		white;
};

#ifdef VARARGCK
# pragma varargck	type	"A"	Atom
# pragma varargck	type	"P"	Point
# pragma varargck	type	"R"	Rectangle
# pragma varargck	type	"W"	Window*	
#endif

Display *display;
Screen scr;

extern const Point ZP;
extern const Rectangle ZR;
extern Window* pointerwin;

XRectangle XRect(Rectangle r);

#define changeprop(w, prop, type, data, n) \
	changeproperty(w, prop, type, \
		((sizeof(*(data)) == 8 ? 4 : sizeof(*(data))) * 8), \
		(uchar*)(data), n)

/* x11.c */
Image*	allocimage(int w, int h, int depth);
void	border(Image *dst, Rectangle, int w, Color);
void	changeprop_char(Window*, char*, char*, char[], int);
void	changeprop_long(Window*, char*, char*, long[], int);
void	changeprop_short(Window*, char*, char*, short[], int);
void	changeprop_string(Window*, char*, char*);
void	changeprop_textlist(Window*, char*, char*, char*[]);
void	changeprop_ulong(Window*, char*, char*, ulong[], int);
void	changeproperty(Window*, char*, char*, int width, uchar*, int);
void	copyimage(Image*, Rectangle, Image*, Point);
Window*	createwindow(Window*, Rectangle, int depth, uint class, WinAttr*, int valuemask);
Window* createwindow_visual(Window*, Rectangle, int depth, Visual*, uint class, WinAttr*, int);
void	delproperty(Window*, char*);
void	destroywindow(Window*);
void	drawline(Image*, Point, Point, int cap, int w, Color);
void	drawpoly(Image*, Point*, int, int cap, int w, Color);
uint	drawstring(Image*, Font*, Rectangle, Align, char*, Color);
void	fill(Image*, Rectangle, Color);
void	fillpoly(Image*, Point*, int, Color);
Window*	findwin(XWindow);
void	freefont(Font*);
void	freeimage(Image *);
void	freestringlist(char**);
XWindow	getfocus(void);
ulong	getprop_long(Window*, char*, char*, ulong, long**, ulong);
char*	getprop_string(Window*, char*);
int	getprop_textlist(Window *w, char *name, char **ret[]);
ulong	getprop_ulong(Window*, char*, char*, ulong, ulong**, ulong);
ulong	getproperty(Window*, char *prop, char *type, Atom *actual, ulong offset, uchar **ret, ulong length);
int	grabkeyboard(Window*);
int	grabpointer(Window*, Window *confine, Cursor, int mask);
void	initdisplay(void);
KeyCode	keycode(char*);
uint	labelh(Font*);
bool	loadcolor(CTuple*, char*);
Font*	loadfont(char*);
void	lowerwin(Window*);
int	mapwin(Window*);
void	movewin(Window*, Point);
bool	namedcolor(char *name, Color*);
bool	parsekey(char*, int*, char**);
int	pointerscreen(void);
Point	querypointer(Window*);
void	raisewin(Window*);
void	reparentwindow(Window*, Window*, Point);
void	reshapewin(Window*, Rectangle);
void	selectinput(Window*, long);
void	sendevent(Window*, bool propagate, long mask, XEvent*);
void	setborder(Window*, int, Color);
void	setfocus(Window*, int mode);
void	sethints(Window*);
void	setshapemask(Window *dst, Image *src, Point);
void	setwinattr(Window*, WinAttr*, int valmask);
char**	strlistdup(char**);
void	sync(void);
uint	textwidth(Font*, char*);
uint	textwidth_l(Font*, char*, uint len);
Rectangle	textextents_l(Font*, char*, uint, int*);
int	traperrors(bool);
Point	translate(Window*, Window*, Point);
void	ungrabkeyboard(void);
void	ungrabpointer(void);
int	unmapwin(Window*);
void	warppointer(Point);
Window*	window(XWindow);
long	winprotocols(Window*);
Atom	xatom(char*);
void	sendmessage(Window*, char*, long, long, long, long, long);
XRectangle	XRect(Rectangle);
Rectangle	getwinrect(Window*);
Handlers*	sethandler(Window*, Handlers*);
Rectangle	sizehint(WinHints*, Rectangle);

