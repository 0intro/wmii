/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#define Window XWindow
#define Font XFont
#define Screen XScreen
#include <stuff/geom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
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

typedef union ClientMessageData ClientMessageData;
typedef struct Color Color;
typedef struct CTuple CTuple;
typedef struct ErrorCode ErrorCode;
typedef struct Ewmh Ewmh;
typedef struct Font Font;
typedef struct Handlers Handlers;
typedef struct HandlersLink HandlersLink;
typedef struct Screen Screen;
typedef struct WinHints WinHints;
typedef struct Window Image;
typedef struct Window Window;
typedef struct Xft Xft;
typedef struct XftColor XftColor;
typedef void XftDraw;
typedef struct XftFont XftFont;

union ClientMessageData {
	char b[20];
	short s[10];
	long l[5];
};

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
	Rectangle (*dndmotion)(Window*, void*, Point);
	bool (*bdown)(Window*, void*, XButtonEvent*);
	bool (*bup)(Window*, void*, XButtonEvent*);
	bool (*config)(Window*, void*, XConfigureEvent*);
	bool (*configreq)(Window*, void*, XConfigureRequestEvent*);
	bool (*destroy)(Window*, void*, XDestroyWindowEvent*);
	bool (*enter)(Window*, void*, XCrossingEvent*);
	bool (*expose)(Window*, void*, XExposeEvent*);
	bool (*focusin)(Window*, void*, XFocusChangeEvent*);
	bool (*focusout)(Window*, void*, XFocusChangeEvent*);
	bool (*kdown)(Window*, void*, XKeyEvent*);
	bool (*kup)(Window*, void*, XKeyEvent*);
	bool (*leave)(Window*, void*, XCrossingEvent*);
	bool (*map)(Window*, void*, XMapEvent*);
	bool (*mapreq)(Window*, void*, XMapRequestEvent*);
	bool (*message)(Window*, void*, XClientMessageEvent*);
	bool (*motion)(Window*, void*, XMotionEvent*);
	bool (*property)(Window*, void*, XPropertyEvent*);
	bool (*reparent)(Window*, void*, XReparentEvent*);
	bool (*selection)(Window*, void*, XSelectionEvent*);
	bool (*selectionclear)(Window*, void*, XSelectionClearEvent*);
	bool (*selectionrequest)(Window*, void*, XSelectionRequestEvent*);
	bool (*unmap)(Window*, void*, XUnmapEvent*);
};

struct HandlersLink {
	HandlersLink*	next;
	void*		aux;
	Handlers*	handler;
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
	HandlersLink*	handler_link;
	WinHints*	hints;
	Ewmh		ewmh;
	long		eventmask;
	void*		dnd;
	void*		aux;
	bool		mapped;
	int		unmapped;
	int		depth;
};

struct Xft {
	XftDraw*	(*drawcreate)(Display*, Drawable, Visual*, Colormap);
	void		(*drawdestroy)(XftDraw*);
	XftFont*	(*fontopen)(Display*, int, const char*);
	XftFont*	(*fontopenname)(Display*, int, const char*);
	XftFont*	(*fontclose)(Display*, XftFont*);
	void		(*textextents)(Display*, XftFont*, char*, int len, XGlyphInfo*);
	void		(*drawstring)(Display*, XftColor*, XftFont*, int x, int y, char*, int len);
};

struct XftColor {
    ulong		pixel;
    XRenderColor	color;
};

struct XftFont {
    int		ascent;
    int		descent;
    int		height;
    int		max_advance_width;
    void*	charset;
    void*	pattern;
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

extern struct Map	windowmap;
extern struct Map	atommap;
extern const Point ZP;
extern const Rectangle ZR;
extern const WinHints ZWinHints;
extern Window* pointerwin;
extern Xft* xft;

XRectangle XRect(Rectangle r);

#define changeprop(w, prop, type, data, n) \
	changeproperty(w, prop, type, \
		((sizeof(*(data)) == 8 ? 4 : sizeof(*(data))) * 8), \
		(uchar*)(data), n)

/* x11.c */
XRectangle	XRect(Rectangle);
Image*	allocimage(int w, int h, int depth);
void	border(Image *dst, Rectangle, int w, Color);
void	changeprop_char(Window*, char*, char*, char[], int);
void	changeprop_long(Window*, char*, char*, long[], int);
void	changeprop_short(Window*, char*, char*, short[], int);
void	changeprop_string(Window*, char*, char*);
void	changeprop_textlist(Window*, char*, char*, char*[]);
void	changeprop_ulong(Window*, char*, char*, ulong[], int);
void	changeproperty(Window*, char*, char*, int width, uchar*, int);
void	clientmessage(Window*, char*, long, int, ClientMessageData);
void	copyimage(Image*, Rectangle, Image*, Point);
Window*	createwindow(Window*, Rectangle, int depth, uint class, WinAttr*, int valuemask);
Window*	createwindow_visual(Window*, Rectangle, int depth, Visual*, uint class, WinAttr*, int);
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
void	gethints(Window*);
ulong	getprop_long(Window*, char*, char*, ulong, long**, ulong);
char*	getprop_string(Window*, char*);
int	getprop_textlist(Window *w, char *name, char **ret[]);
ulong	getprop_ulong(Window*, char*, char*, ulong, ulong**, ulong);
ulong	getproperty(Window*, char *prop, char *type, Atom *actual, ulong offset, uchar **ret, ulong length);
Rectangle	getwinrect(Window*);
int	grabkeyboard(Window*);
int	grabpointer(Window*, Window *confine, Cursor, int mask);
bool	havexft(void);
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
bool	pophandler(Window*, Handlers*);
void	pushhandler(Window*, Handlers*, void*);
Point	querypointer(Window*);
void	raisewin(Window*);
void	reparentwindow(Window*, Window*, Point);
void	reshapewin(Window*, Rectangle);
void	selectinput(Window*, long);
void	sendevent(Window*, bool propagate, long mask, void*);
void	sendmessage(Window*, char*, long, long, long, long, long);
void	setborder(Window*, int, Color);
void	setfocus(Window*, int mode);
Handlers*	sethandler(Window*, Handlers*);
void	sethints(Window*, WinHints*);
void	setshapemask(Window *dst, Image *src, Point);
void	setwinattr(Window*, WinAttr*, int valmask);
Rectangle	sizehint(WinHints*, Rectangle);
char**	strlistdup(char**);
void	sync(void);
Rectangle	textextents_l(Font*, char*, uint, int*);
uint	textwidth(Font*, char*);
uint	textwidth_l(Font*, char*, uint len);
Point	translate(Window*, Window*, Point);
int	traperrors(bool);
void	ungrabkeyboard(void);
void	ungrabpointer(void);
int	unmapwin(Window*);
void	warppointer(Point);
Window*	window(XWindow);
char*	windowname(Window*);
long	winprotocols(Window*);
Atom	xatom(char*);

