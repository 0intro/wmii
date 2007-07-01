/* © 2004-2006 Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <regexp9.h>
#define IXP_P9_STRUCTS
#define IXP_NO_P9_
#include <ixp.h>
#include <utf.h>
#include <fmt.h>
#include "x11.h"

#define FONT		"-*-fixed-medium-r-*-*-13-*-*-*-*-*-*-*"
#define FOCUSCOLORS	"#ffffff #335577 #447799"
#define NORMCOLORS	"#222222 #eeeeee #666666"

typedef struct CTuple CTuple;

enum Align {
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

typedef enum Align Align;

struct CTuple {
	ulong bg;
	ulong fg;
	ulong border;
	char colstr[24]; /* #RRGGBB #RRGGBB #RRGGBB */
};

enum {
	Coldefault, Colstack, Colmax
};

enum {
	CurNormal,
	CurNECorner, CurNWCorner, CurSECorner, CurSWCorner,
	CurDHArrow, CurMove, CurInput, CurSizing, CurIcon,
	CurNone,
	CurLast
};

enum { 
	NCOL = 16,
	WM_PROTOCOL_DELWIN = 1
};

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
typedef struct Map Map;
typedef struct MapEnt MapEnt;

struct Map {
	MapEnt **bucket;
	uint nhash;
};

struct MapEnt {
	ulong hash;
	char *key;
	void *val;
	MapEnt *next;
};

struct View {
	View *next;
	char name[256];
	ushort id;
	Area *area;
	Area *sel;
	Area *revert;
};

struct Area {
	Area *next, *prev;
	Frame *frame;
	Frame *stack;
	Frame *sel;
	View *view;
	Bool floating;
	ushort id;
	int mode;
	Rectangle r;
};

struct Frame {
	Frame *cnext;
	Frame *anext, *aprev;
	Frame *snext, *sprev;
	View *view;
	Area *area;
	ushort id;
	Rectangle r;
	Rectangle crect;
	Rectangle revert;
	Client *client;
	Bool collapsed;
	Rectangle grabbox;
	Rectangle titlebar;
	float ratio;
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
	Bool borderless;
	Bool titleless;
	Bool noinput;
	int unmapped;
	Window w;
	XWindow trans;
	Window *framewin;
	Cursor cursor;
	Rectangle r;
	XSizeHints size;
	GC gc;
};

struct Divide {
	Divide *next;
	Window *w;
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
	Rectangle r;
	CTuple col;
};

struct Rule {
	Rule *next;
	Reprog *regex;
	char value[256];
};

struct Ruleset {
	Rule		*rule;
	char		*string;
	uint		size;
};

/* global variables */
struct {
	CTuple focuscolor;
	CTuple normcolor;
	Font *font;
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

enum {
	BarLeft, BarRight
};

struct WMScreen {
	Bar *bar[2];
	View *sel;
	Client *focus;
	Client *hasgrab;
	Window *barwin;
	Image *ibuf;

	Rectangle r;
	Rectangle brect;
} *screens, *screen;

Client *client;
View *view;
Key *key;
Divide *divs;
Client c_magic;
Client c_root;

Handlers framehandler;

char buffer[8092];

/* IXP */
IxpServer srv;
Ixp9Srv p9srv;

/* X11 */
uint num_screens;
uint valid_mask;
uint num_lock_mask;
Bool sel_screen;

Image xor;

Cursor cursor[CurLast];
void (*handler[LASTEvent]) (XEvent *);

/* Misc */
Image *broken;
Bool starting;
Bool verbose;
char *user;
char *execstr;

#define Debug if(verbose)
#define Dprint(...) do{ Debug fprint(2, __VA_ARGS__); }while(0)

