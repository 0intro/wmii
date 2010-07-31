/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#define Window XWindow
#define Font XFont
#define Screen XScreen
#define Mask XMask
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
#undef Mask

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
	ushort	red;
	ushort	green;
	ushort	blue;
	ushort	alpha;
	ulong	pixel;
};

struct CTuple {
	Color bg;
	Color fg;
	Color border;
	char colstr[64];
};

struct ErrorCode {
	uchar rcode;
	uchar ecode;
};

struct Ewmh {
	long	type;
	ulong	ping;
	ulong	lag;
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
	XIC		xic;
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
	void		(*textextents)(Display*, XftFont*, const char*, int len, XGlyphInfo*);
	void		(*drawstring)(Display*, XftColor*, XftFont*, int x, int y, const char*, int len);
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
	Colormap	colormap32;
	Visual*		visual;
	Visual*		visual32;
	Rectangle	rect;
	int		depth;
	int		fd;
	XIM		xim;
};

#ifdef VARARGCK
# pragma varargck	type	"A"	Atom
# pragma varargck	type	"P"	Point
# pragma varargck	type	"R"	Rectangle
# pragma varargck	type	"W"	Window*
#endif

Display *display;
Screen scr;

extern char*		modkey_names[];
extern struct Map	windowmap;
extern struct Map	atommap;
extern struct Map	atomnamemap;
extern const Point ZP;
extern const Rectangle ZR;
extern const WinHints ZWinHints;
extern Window* pointerwin;
extern Xft* xft;

XRectangle XRect(Rectangle r);

#define RGBA_P(tuple) (\
	((long)(tuple).fg.alpha + (long)(tuple).bg.alpha + (long)(tuple).border.alpha) < 3 * 0xff00)

#define changeprop(w, prop, type, data, n) \
	changeproperty(w, prop, type, \
		((sizeof(*(data)) == 8 ? 4 : sizeof(*(data))) * 8), \
		(uchar*)(data), n)

/* x11.c */
XRectangle	XRect(Rectangle);
Image*	allocimage(int w, int h, int depth);
char*	atomname(ulong);
void	border(Image *dst, Rectangle, int w, Color*);
void	changeprop_char(Window*, const char*, const char*, const char*, int);
void	changeprop_long(Window*, const char*, const char*, long[], int);
void	changeprop_short(Window*, const char*, const char*, short[], int);
void	changeprop_string(Window*, const char*, const char*);
void	changeprop_textlist(Window*, const char*, const char*, char*[]);
void	changeprop_ulong(Window*, const char*, const char*, ulong[], int);
void	changeproperty(Window*, const char*, const char*, int width, const uchar*, int);
void	cleanupwindow(Window*);
void	clientmessage(Window*, const char*, long, int, ClientMessageData);
void	copyimage(Image*, Rectangle, Image*, Point);
Window*	createwindow(Window*, Rectangle, int depth, uint class, WinAttr*, int valuemask);
Window*	createwindow_rgba(Window*, Rectangle, WinAttr*, int valuemask);
Window*	createwindow_visual(Window*, Rectangle, int depth, Visual*, uint class, WinAttr*, int);
void	delproperty(Window*, const char*);
void	destroywindow(Window*);
void	drawline(Image*, Point, Point, int cap, int w, Color*);
void	drawpoly(Image*, Point*, int, int cap, int w, Color*);
uint	drawstring(Image*, Font*, Rectangle, Align, const char*, Color*);
void	fill(Image*, Rectangle, Color*);
void	fillpoly(Image*, Point*, int, Color*);
Window*	findwin(XWindow);
void	freefont(Font*);
void	freeimage(Image *);
void	freestringlist(char**);
XWindow	getfocus(void);
void	gethints(Window*);
ulong	getprop(Window*, const char*, const char*, Atom*, int*, ulong, uchar**, ulong);
ulong	getprop_long(Window*, const char*, const char*, ulong, long**, ulong);
char*	getprop_string(Window*, const char*);
int	getprop_textlist(Window *w, const char *name, char **ret[]);
ulong	getprop_ulong(Window*, const char*, const char*, ulong, ulong**, ulong);
ulong	getproperty(Window*, char *prop, char *type, Atom *actual, ulong offset, uchar **ret, ulong length);
Rectangle	getwinrect(Window*);
int	grabkeyboard(Window*);
int	grabpointer(Window*, Window *confine, Cursor, int mask);
bool	havexft(void);
void	initdisplay(void);
KeyCode	keycode(const char*);
uint	labelh(Font*);
int	loadcolor(CTuple*, const char*, const char*);
Font*	loadfont(const char*);
void	lowerwin(Window*);
int	mapwin(Window*);
void	movewin(Window*, Point);
int	numlockmask(void);
bool	parsecolor(const char *name, Color*);
bool	parsekey(char*, int*, char**);
ulong	pixelvalue(Image*, Color*);
int	pointerscreen(void);
bool	pophandler(Window*, Handlers*);
void	pushhandler(Window*, Handlers*, void*);
Point	querypointer(Window*);
void	raisewin(Window*);
void	reparentwindow(Window*, Window*, Point);
void	reshapewin(Window*, Rectangle);
void	selectinput(Window*, long);
void	sendevent(Window*, bool propagate, long mask, void*);
void	sendmessage(Window*, const char*, long, long, long, long, long);
void	setborder(Window*, int, Color*);
void	setfocus(Window*, int mode);
Handlers*	sethandler(Window*, Handlers*);
void	sethints(Window*, WinHints*);
void	setshapemask(Window *dst, Image *src, Point);
void	setwinattr(Window*, WinAttr*, int valmask);
Rectangle	sizehint(WinHints*, Rectangle);
char**	strlistdup(char**);
void	sync(void);
Rectangle	textextents_l(Font*, const char*, uint, int*);
uint	textwidth(Font*, const char*);
uint	textwidth_l(Font*, const char*, uint len);
Point	translate(Window*, Window*, Point);
int	traperrors(bool);
void	ungrabkeyboard(void);
void	ungrabpointer(void);
int	unmapwin(Window*);
void	warppointer(Point);
Window*	window(XWindow);
char*	windowname(Window*);
long	winprotocols(Window*);
Atom	xatom(const char*);

