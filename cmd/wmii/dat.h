/* Copyright ©2007-2009 Kris Maglione <jg@suckless.org>
 * See LICENSE file for license details.
 */

#define _XOPEN_SOURCE 600
#define IXP_P9_STRUCTS
#define IXP_NO_P9_
#include <assert.h>
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

enum {
	PingTime = 10000,
};

enum {
	CLeft = 1<<0,
	CCenter = 1<<1,
	CRight = 1<<2,
};

enum IncMode {
	IIgnore,
	IShow,
	ISqueeze,
};

enum {
	GInvert = 1<<0,
};

enum {
	UrgManager,
	UrgClient,
};

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
	Coldefault, Colstack, Colmax, Collast
};

extern char*	modes[];

#define TOGGLE(x) \
	(x == On ? "on" : \
	 x == Off ? "off" : \
	 x == Toggle ? "toggle" : \
	 "<toggle>")
enum {
	Off,
	On,
	Toggle,
};

enum Barpos {
	BBottom,
	BTop,
};

enum {
	CurNormal,
	CurNECorner, CurNWCorner, CurSECorner, CurSWCorner,
	CurDHArrow, CurDVArrow, CurMove, CurInput, CurSizing,
	CurTCross, CurIcon,
	CurNone,
	CurLast,
};

enum {
	NCOL = 16,
};

enum Protocols {
	ProtoDelete	= 1<<0,
	ProtoTakeFocus	= 1<<1,
	ProtoPing	= 1<<2,
};

enum DebugOpt {
	DDnd	= 1<<0,
	DEvent	= 1<<1,
	DEwmh	= 1<<2,
	DFocus	= 1<<3,
	DGeneric= 1<<4,
	DStack  = 1<<5,
	NDebugOpt = 6,
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
typedef struct Regex Regex;
typedef struct Rule Rule;
typedef struct Ruleset Ruleset;
typedef struct Strut Strut;
typedef struct View View;
typedef struct WMScreen WMScreen;

struct Area {
	Area*	next;
	Area*	prev;
	Frame*	frame;
	Frame*	frame_old;
	Frame*	stack;
	Frame*	sel;
	View*	view;
	bool	floating;
	ushort	id;
	int	mode;
	int	screen;
	bool	max;
	Rectangle	r;
	Rectangle	r_old;
};

struct Bar {
	Bar*	next;
	Bar*	smaller;
	char	buf[280];
	char	text[256];
	char	name[256];
	int	bar;
	ushort	id;
	CTuple	col;
	Rectangle	r;
	WMScreen*	screen;
};

struct Regex {
	char*	regex;
	Reprog*	regc;
};

struct Client {
	Client*	next;
	Frame*	frame;
	Frame*	sel;
	Window	w;
	Window*	framewin;
	Image**	ibuf;
	XWindow	trans;
	Regex	tagre;
	Regex	tagvre;
	Group*	group;
	Strut*	strut;
	Cursor	cursor;
	Rectangle r;
	char**	retags;
	char	name[256];
	char	tags[256];
	char	props[512];
	long	proto;
	uint	border;
	int	fullscreen;
	int	unmapped;
	char	floating;
	char	fixedsize;
	char	urgent;
	char	borderless;
	char	titleless;
	char	noinput;
};

struct Divide {
	Divide*	next;
	Window*	w;
	Area*	left;
	Area*	right;
	bool	mapped;
	int	x;
};

struct Frame {
	Frame*	cnext;
	Frame*	anext;
	Frame*	aprev;
	Frame*	anext_old;
	Frame*	snext;
	Frame*	sprev;
	Client*	client;
	View*	view;
	Area*	area;
	int	oldarea;
	int	column;
	ushort	id;
	bool	collapsed;
	int	dy;
	Rectangle	r;
	Rectangle	colr;
	Rectangle	colr_old;
	Rectangle	floatr;
	Rectangle	crect;
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

#define firstarea areas[screen->idx]
#define screenr r[screen->idx]
struct View {
	View*	next;
	char	name[256];
	ushort	id;
	Area*	floating;
	Area**	areas;
	Area*	sel;
	Area*	oldsel;
	Area*	revert;
	int	selcol;
	bool	dead;
	Rectangle *r;
	Rectangle *pad;
};

/* Yuck. */
#define VECTOR(type, nam, c) \
typedef struct Vector_##nam Vector_##nam;      \
struct Vector_##nam {                          \
	type*	ary;                           \
	long	n;                             \
	long	size;                          \
};                                             \
void	vector_##c##free(Vector_##nam*);       \
void	vector_##c##init(Vector_##nam*);       \
void	vector_##c##push(Vector_##nam*, type); \

VECTOR(long, long, l)
VECTOR(Rectangle, rect, r)
VECTOR(void*, ptr, p)
#undef  VECTOR

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
	int	incmode;
} def;

enum {
	BLeft, BRight
};

#define BLOCK(x) do { x; }while(0)

EXTERN struct WMScreen {
	Bar*	bar[2];
	Window*	barwin;
	bool	showing;
	int	barpos;
	int	idx;

	Rectangle r;
	Rectangle brect;
} **screens, *screen;
EXTERN uint	nscreens;

EXTERN struct {
	Client*	focus;
	Client*	hasgrab;
	Image*	ibuf;
	Image*	ibuf32;
	bool	sel;
} disp;

EXTERN Client*	client;
EXTERN View*	view;
EXTERN View*	selview;
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
EXTERN uint	valid_mask;
EXTERN uint	numlock_mask;
EXTERN Image*	ibuf;
EXTERN Image*	ibuf32;

EXTERN Cursor	cursor[CurLast];

typedef void (*XHandler)(XEvent*);
EXTERN XHandler handler[LASTEvent];

/* Misc */
EXTERN bool	starting;
EXTERN bool	resizing;
EXTERN long	ignoreenter;
EXTERN char*	user;
EXTERN char*	execstr;
EXTERN int	debugflag;
EXTERN int	debugfile;
EXTERN long	xtime;
EXTERN Visual*	render_visual;

EXTERN Client*	kludge;

extern char*	debugtab[];

#define Debug(x) if(((debugflag|debugfile)&(x)) && setdebug(x))
#define Dprint(x, ...) BLOCK( if((debugflag|debugfile)&(x)) debug(x, __VA_ARGS__) )

