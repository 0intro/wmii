/* © 2004-2006 Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <regex.h>
#include <ixp.h>

#define BLITZ_FONT		"-*-fixed-medium-r-normal-*-13-*-*-*-*-*-*-*"
#define BLITZ_FOCUSCOLORS	"#ffffff #335577 #447799"
#define BLITZ_NORMCOLORS	"#222222 #eeeeee #666666"

typedef struct Blitz Blitz;
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

typedef enum BlitzAlign BlitzAlign;

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

enum {
/* WM atom */
	WMState, WMProtocols, WMDelete,
/* NET atom */
	NetSupported, NetWMName,
/* Other */
	TagsAtom,
/* Last atom */
	AtomLast
};

/* Column modes */
enum { Coldefault, Colstack, Colmax };

/* Cursor */
enum {
	CurNormal,
	CurNECorner, CurNWCorner, CurSECorner, CurSWCorner,
	CurDHArrow, CurMove, CurInput,
	CurInvisible,
	CurLast
};

enum { NCOL = 16 };
enum { WM_PROTOCOL_DELWIN = 1 };

/* Data Structures */
typedef struct View View;
typedef struct Area Area;
typedef struct Frame Frame;
typedef struct Client Client;
typedef struct Divide Divide;
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

struct Divide {
	Divide *next;
	Window w;
	Bool mapped;
	int x;
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

enum { BarLeft, BarRight };

struct WMScreen {
	Bar *bar[2];
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
Divide *divs;
Client c_magic;
Client c_root;

char buffer[8092];

/* IXP */
IxpServer srv;
Ixp9Srv p9srv;

/* X11 */
uint num_screens;
uint valid_mask;
uint num_lock_mask;
Bool sel_screen;

Blitz blz;
GC xorgc;
Pixmap pmap;
Pixmap divmap, divmask;

Atom atom[AtomLast];
Cursor cursor[CurLast];
void (*handler[LASTEvent]) (XEvent *);

/* Misc */
Bool starting;
Bool verbose;
char *user;
char *execstr;
