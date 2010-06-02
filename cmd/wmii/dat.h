/* Copyright ©2007-2010 Kris Maglione <jg@suckless.org>
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
#include <unistd.h>
#include <limits.h>
#include <utf.h>
#include <ixp.h>
#include <stuff/x.h>
#include <stuff/util.h>
#include "debug.h"

#define FONT		"-*-fixed-medium-r-*-*-13-*-*-*-*-*-*-*"
#define FOCUSCOLORS	"#ffffff #335577 #447799"
#define NORMCOLORS	"#222222 #eeeeee #666666"

enum {
	PingTime = 10000,
	PingPeriod = 2000,
	PingPartition = 20,
};

enum IncMode {
	IIgnore,
	IShow,
	ISqueeze,
};

enum {
	UrgManager,
	UrgClient,
};

enum {
	SourceUnknown,
	SourceClient,
	SourcePager
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

#define toggle(val, x) \
	((x) == On     ? true   : \
	 (x) == Off    ? false  : \
	 (x) == Toggle ? !(val) : (val))
#define TOGGLE(x) \
	((x) == On     ? "on"     : \
	 (x) == Off    ? "off"    : \
	 (x) == Toggle ? "toggle" : "<toggle>")
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
	NCOL = 16,
};

enum Protocols {
	ProtoDelete	= 1<<0,
	ProtoTakeFocus	= 1<<1,
	ProtoPing	= 1<<2,
};

enum {
	CurNormal,
	CurNECorner, CurNWCorner, CurSECorner, CurSWCorner,
	CurDHArrow, CurDVArrow, CurMove, CurInput, CurSizing,
	CurTCross, CurIcon,
	CurNone,
	CurLast,
};
Cursor	cursor[CurLast];


/* Data Structures */
typedef struct Area Area;
typedef struct Bar Bar;
typedef struct Client Client;
typedef struct Divide Divide;
typedef struct Frame Frame;
typedef struct Group Group;
typedef struct Key Key;
typedef struct Rule Rule;
typedef struct Ruleset Ruleset;
typedef struct Ruleval Ruleval;
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
	int	dead;
	int	fullscreen;
	bool	floating;
	bool	fixedsize;
	bool	urgent;
	bool	borderless;
	bool	titleless;
	bool	nofocus;
	bool	noinput;
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
	int	oldscreen;
	int	oldarea;
	int	screen;
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

struct Rule {
	Rule*		next;
	Reprog*		regex;
	char*		value;
	Ruleval*	values;
};

struct Ruleset {
	Rule*	rule;
	char*	string;
	uint	size;
};

struct Ruleval {
	Ruleval*	next;
	char*		key;
	char*		value;
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
	int	selscreen;
	bool	dead;
	Rectangle *r;
	Rectangle *pad;
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
	Ruleset	colrules;
	Ruleset	tagrules;
	Ruleset	rules;
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

EXTERN Client	c_magic;
EXTERN Client	c_root;
EXTERN Client*	client;
EXTERN Divide*	divs;
EXTERN Key*	key;
EXTERN View*	selview;
EXTERN View*	view;

EXTERN Handlers	framehandler;

/* IXP */
EXTERN IxpServer srv;
EXTERN Ixp9Srv	p9srv;

/* X11 */
EXTERN Image*	ibuf32;
EXTERN Image*	ibuf;
EXTERN uint	numlock_mask;
EXTERN uint	valid_mask;

/* Misc */
EXTERN char*	execstr;
EXTERN char	hostname[HOST_NAME_MAX + 1];
EXTERN long	ignoreenter;
EXTERN bool	resizing;
EXTERN int	starting;
EXTERN char*	user;

EXTERN Client*	kludge;

extern char*	debugtab[];

