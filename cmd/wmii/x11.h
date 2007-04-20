#define Window XWindow
#define Font XFont
#define Screen XScreen
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/shape.h>
#undef Window
#undef Font
#undef Screen

typedef struct Point Point;
typedef struct Rectangle Rectangle;
typedef struct Screen Screen;
typedef struct Window Window;
typedef struct WinHints WinHints;
typedef struct Handlers Handlers;
typedef struct Window Image;
typedef struct Font Font;
typedef XSetWindowAttributes WinAttr;

struct Point {
	int x, y;
};

struct Rectangle {
	Point min, max;
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
	Bool mapped;
	int unmapped;
	int depth;
};

struct WinHints {
	Point min, max;
	Point base, baspect;
	Point inc;
	Rectangle aspect;
	Point grav;
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

Display *display;
Screen scr;

extern Point ZP;
extern Rectangle ZR;

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
