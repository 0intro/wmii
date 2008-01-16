/* © 2004-2006 Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#define _XOPEN_SOURCE 600
#define IXP_P9_STRUCTS
#define IXP_NO_P9_
#include <regexp9.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ixp.h>
#include <util.h>
#include <utf.h>
#include <fmt.h>
#include <x11.h>

#define FONT		"-*-fixed-medium-r-*-*-13-*-*-*-*-*-*-*"
#define FOCUSCOLORS	"#ffffff #335577 #447799"
#define NORMCOLORS	"#222222 #eeeeee #666666"

enum EWMHType {
	TypeDesktop	= 1<<0,
	TypeDock	= 1<<1,
	TypeToolbar	= 1<<2,
	TypeMenu	= 1<<3,
	TypeUtility	= 1<<4,
	TypeSplash	= 1<<5,
	TypeDialog	= 1<<6,
	TypeNormal	= 1<<7,
};

enum {
	Coldefault, Colstack, Colmax,
};

#define TOGGLE(x) \
	(x == On ? "On" : \
	 x == Off ? "Off" : \
	 x == Toggle ? "Toggle" : \
	 "<toggle>")
enum {
	Off,
	On,
	Toggle,
};

enum {
	CurNormal,
	CurNECorner, CurNWCorner, CurSECorner, CurSWCorner,
	CurDHArrow, CurMove, CurInput, CurSizing, CurIcon,
	CurNone,
	CurLast,
};

enum {
	NCOL = 16,
	WM_PROTOCOL_DELWIN = 1,
};

enum DebugOpt {
	DEvent	= 1<<0,
	DEwmh	= 1<<1,
	DFocus	= 1<<2,
	DGeneric= 1<<3,
};

/* Data Structures */
typedef struct Area Area;
typedef struct Bar Bar;
typedef struct Client Client;
typedef struct Divide Divide;
typedef struct Frame Frame;
typedef struct Group Group;
typedef struct Key Key;
typedef struct Map Map;
typedef struct MapEnt MapEnt;
typedef struct Rule Rule;
typedef struct Ruleset Ruleset;
typedef struct Strut Strut;
typedef struct View View;
typedef struct WMScreen WMScreen;

struct Area {
	Area*	next;
	Area*	prev;
	Frame*	frame;
	Frame*	stack;
	Frame*	sel;
	View*	view;
	bool	floating;
	ushort	id;
	int	mode;
	Rectangle	r;
};

struct Bar {
	Bar*	next;
	Bar*	smaller;
	char	buf[280];
	char	text[256];
	char	name[256];
	ushort	id;
	Rectangle r;
	CTuple	col;
};

struct Client {
	Client*	next;
	Area*	revert;
	Frame*	frame;
	Frame*	sel;
	Window	w;
	Window*	framewin;
	XWindow	trans;
	Group*	group;
	Strut*	strut;
	Cursor	cursor;
	Rectangle r;
	char	name[256];
	char	tags[256];
	char	props[512];
	uint	border;
	int	proto;
	char	floating;
	char	fixedsize;
	char	fullscreen;
	char	urgent;
	char	borderless;
	char	titleless;
	char	noinput;
	int	unmapped;
};

struct Divide {
	Divide*	next;
	Window*	w;
	bool	mapped;
	int	x;
};

struct Frame {
	Frame*	cnext;
	Frame*	anext;
	Frame*	aprev;
	Frame*	snext;
	Frame*	sprev;
	View*	view;
	Area*	area;
	Client*	client;
	ushort	id;
	bool	collapsed;
	float	ratio;
	Rectangle	r;
	Rectangle	crect;
	Rectangle	revert;
	Rectangle	grabbox;
	Rectangle	titlebar;
};

struct Group {
	Group*	next;
	XWindow	leader;
	Client	*client;
	int	ref;
};

struct Key {
	Key*	next;
	Key*	lnext;
	Key*	tnext;
	ushort	id;
	char	name[128];
	ulong	mod;
	KeyCode	key;
};

struct Map {
	MapEnt**bucket;
	uint	nhash;
};

struct MapEnt {
	ulong		hash;
	const char*	key;
	void*		val;
	MapEnt*		next;
};

struct Rule {
	Rule*	next;
	Reprog*	regex;
	char	value[256];
};

struct Ruleset {
	Rule*	rule;
	char*	string;
	uint	size;
};

struct Strut {
	Rectangle	left;
	Rectangle	right;
	Rectangle	top;
	Rectangle	bottom;
};

struct View {
	View*	next;
	char	name[256];
	ushort	id;
	Area*	area;
	Area*	sel;
	Area*	oldsel;
	Area*	revert;
};

#ifndef EXTERN
#  define EXTERN extern
#endif

/* global variables */
EXTERN struct {
	CTuple	focuscolor;
	CTuple	normcolor;
	Font*	font;
	char*	keys;
	uint	keyssz;
	Ruleset	tagrules;
	Ruleset	colrules;
	char	grabmod[5];
	ulong	mod;
	uint	border;
	uint	snap;
	int	colmode;
} def;

enum {
	BarLeft, BarRight
};

#define BLOCK(x) do { x; }while(0)

EXTERN struct WMScreen {
	Bar*	bar[2];
	View*	sel;
	Client*	focus;
	Client*	hasgrab;
	Window*	barwin;
	Image*	ibuf;

	Rectangle r;
	Rectangle brect;
} *screens, *screen;

EXTERN Client*	client;
EXTERN View*	view;
EXTERN Key*	key;
EXTERN Divide*	divs;
EXTERN Client	c_magic;
EXTERN Client	c_root;

EXTERN Handlers	framehandler;

EXTERN char	buffer[8092];
EXTERN char*	_buffer;
static char*	const _buf_end = buffer + sizeof buffer;

#define bufclear() \
	BLOCK( _buffer = buffer; _buffer[0] = '\0' )
#define bufprint(...) \
	_buffer = seprint(_buffer, _buf_end, __VA_ARGS__)

/* IXP */
EXTERN IxpServer srv;
EXTERN Ixp9Srv	p9srv;

/* X11 */
EXTERN uint	num_screens;
EXTERN uint	valid_mask;
EXTERN uint	numlock_mask;
EXTERN bool	sel_screen;

EXTERN Cursor	cursor[CurLast];

typedef void (*XHandler)(XEvent*);
EXTERN XHandler handler[LASTEvent];

/* Misc */
EXTERN bool	starting;
EXTERN char*	user;
EXTERN char*	execstr;
EXTERN int	debug;

#define Debug(x) if(debug&(x))
#define Dprint(x, ...) BLOCK( Debug(x) fprint(2, __VA_ARGS__) )

