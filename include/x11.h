#define Window XWindow
#define Font XFont
#define Screen XScreen
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#ifdef _X11_VISIBLE
#  include <X11/Xatom.h>
#  include <X11/extensions/shape.h>
#endif
#undef Window
#undef Font
#undef Screen

enum Align {
	NORTH = 0x01,
	EAST  = 0x02,
	SOUTH = 0x04,
	WEST  = 0x08,
	NEAST = NORTH | EAST,
	NWEST = NORTH | WEST,
	SEAST = SOUTH | EAST,
	SWEST = SOUTH | WEST,
	CENTER = NEAST | SWEST,
};

typedef enum Align Align;

typedef struct CTuple CTuple;
typedef struct Point Point;
typedef struct Rectangle Rectangle;
typedef struct Screen Screen;
typedef struct Ewmh Ewmh;
typedef struct Window Window;
typedef struct WinHints WinHints;
typedef struct Handlers Handlers;
typedef struct Window Image;
typedef struct Font Font;
typedef XSetWindowAttributes WinAttr;

struct CTuple {
	ulong bg;
	ulong fg;
	ulong border;
	char colstr[24]; /* #RRGGBB #RRGGBB #RRGGBB */
};

struct Point {
	int x, y;
};

struct Rectangle {
	Point min, max;
};

struct Ewmh {
	long	type;
	long	ping;
	long	timer;
};

struct Window {
	int type;
	XWindow w;
	Window *parent;
	Drawable image;
	GC gc;
	Rectangle r;
	void *aux;
	Handlers *handler;
	Window *next, *prev;
	WinHints *hints;
	Ewmh ewmh;
	bool mapped;
	int unmapped;
	int depth;
};

struct WinHints {
	Point min, max;
	Point base, baspect;
	Point inc;
	Rectangle aspect;
	Point grav;
	bool gravstatic;
	bool position;
};

struct Handlers {
	void (*bdown)(Window*, XButtonEvent*);
	void (*bup)(Window*, XButtonEvent*);
	void (*kdown)(Window*, XKeyEvent*);
	void (*kup)(Window*, XKeyEvent*);
	void (*focusin)(Window*, XFocusChangeEvent*);
	void (*focusout)(Window*, XFocusChangeEvent*);
	void (*enter)(Window*, XCrossingEvent*);
	void (*leave)(Window*, XCrossingEvent*);
	void (*motion)(Window*, XMotionEvent*);
	void (*destroy)(Window*, XDestroyWindowEvent*);
	void (*configreq)(Window*, XConfigureRequestEvent*);
	void (*map)(Window*, XMapEvent*);
	void (*unmap)(Window*, XUnmapEvent*);
	void (*property)(Window*, XPropertyEvent*);
	void (*expose)(Window*, XExposeEvent*);
};

struct Screen {
	int screen;
	Window root;
	Colormap colormap;
	Visual *visual;
	Rectangle rect;
	GC gc;
	int depth;
	int fd;
	ulong black, white;
};

enum { WWindow, WImage };

struct Font {
	XFontStruct *xfont;
	XFontSet set;
	int ascent;
	int descent;
	uint height;
	char *name;
};

#ifdef VARARGCK
# pragma varargck	type	"A"	Atom
# pragma varargck	type	"W"	Window*	
# pragma varargck	type	"P"	Point
# pragma varargck	type	"R"	Rectangle
#endif

Display *display;
Screen scr;

extern Point ZP;
extern Rectangle ZR;
extern Window* pointerwin;

Rectangle insetrect(Rectangle r, int n);

Point Pt(int x, int y);
Rectangle Rect(int x0, int y0, int x1, int y1);
Rectangle Rpt(Point min, Point max);

XRectangle XRect(Rectangle r);

#define Dx(r) ((r).max.x - (r).min.x)
#define Dy(r) ((r).max.y - (r).min.y)
#define Pt(x, y) ((Point){(x), (y)})
#define Rpt(p, q) ((Rectangle){p, q})
#define Rect(x0, y0, x1, y1) ((Rectangle){Pt(x0, y0), Pt(x1, y1)})
#define changeprop(w, prop, type, data, n) \
	changeproperty(w, prop, type, \
		((sizeof(*(data)) == 8 ? 4 : sizeof(*(data))) * 8), \
		(uchar*)(data), n)

/* x11.c */
Point	addpt(Point, Point);
Image*	allocimage(int w, int h, int depth);
void	border(Image *dst, Rectangle, int w, ulong col);
void	changeprop_char(Window*, char*, char*, char[], int);
void	changeprop_long(Window*, char*, char*, long[], int);
void	changeprop_short(Window*, char*, char*, short[], int);
void	changeprop_string(Window*, char*, char*);
void	changeprop_textlist(Window*, char*, char*, char*[]);
void	changeproperty(Window*, char*, char*, int width, uchar*, int);
void	copyimage(Image*, Rectangle, Image*, Point);
Window*	createwindow(Window *parent, Rectangle, int depth, uint class, WinAttr*, int valuemask);
void	delproperty(Window*, char*);
void	destroywindow(Window*);
Point	divpt(Point, Point);
void	drawline(Image*, Point, Point, int cap, int w, ulong col);
void	drawpoly(Image*, Point*, int, int cap, int w, ulong col);
uint	drawstring(Image*, Font*, Rectangle, Align, char*, ulong col);
int	eqpt(Point, Point);
int	eqrect(Rectangle, Rectangle);
void	fill(Image*, Rectangle, ulong col);
void	fillpoly(Image*, Point*, int, ulong col);
Window*	findwin(XWindow);
void	freefont(Font*);
void	freeimage(Image *);
void	freestringlist(char**);
ulong	getprop_long(Window*, char*, char*, ulong, long**, ulong);
char*	getprop_string(Window*, char*);
int	getprop_textlist(Window *w, char *name, char **ret[]);
ulong	getproperty(Window*, char *prop, char *type, Atom *actual, ulong offset, uchar **ret, ulong length);
int	grabpointer(Window*, Window *confine, Cursor, int mask);
void	initdisplay(void);
KeyCode	keycode(char*);
uint	labelh(Font*);
bool	loadcolor(CTuple*, char*);
Font*	loadfont(char*);
void	lowerwin(Window*);
int	mapwin(Window*);
void	movewin(Window*, Point);
Point	mulpt(Point p, Point q);
bool	namedcolor(char *name, ulong*);
int	pointerscreen(void);
Point	querypointer(Window*);
void	raisewin(Window*);
void	reparentwindow(Window*, Window*, Point);
void	reshapewin(Window*, Rectangle);
void	sendevent(Window*, bool propegate, long mask, XEvent*);
void	setfocus(Window*, int mode);
void	sethints(Window*);
void	setshapemask(Window *dst, Image *src, Point);
void	setwinattr(Window*, WinAttr*, int valmask);
char**	strlistdup(char**, int);
Point	subpt(Point, Point);
void	sync(void);
uint	textwidth(Font*, char*);
uint	textwidth_l(Font*, char*, uint len);
Point	translate(Window*, Window*, Point);
void	ungrabpointer(void);
int	unmapwin(Window*);
void	warppointer(Point);
Window*	window(XWindow);
long	winprotocols(Window*);
Atom	xatom(char*);
XRectangle	XRect(Rectangle);
Rectangle	gravitate(Rectangle dst, Rectangle src, Point grav);
Rectangle	insetrect(Rectangle, int);
Rectangle	rectaddpt(Rectangle, Point);
Rectangle	rectsubpt(Rectangle, Point);
Handlers*	sethandler(Window*, Handlers*);
Rectangle	sizehint(WinHints*, Rectangle);

