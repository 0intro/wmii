#define _XOPEN_SOURCE 600
#define IXP_P9_STRUCTS
#define IXP_NO_P9_
#include <fmt.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stuff/x.h>
#include <stuff/util.h>
#include <ixp.h>

#define BLOCK(x) do { x; }while(0)

#ifndef EXTERN
# define EXTERN extern
#endif

enum {
	FORWARD,
	BACKWARD,
	LINE,
	WORD,
	CHAR,
	CARET_LAST,
};

enum {
	LACCEPT,
	LBACKWARD,
	LCHAR,
	LCOMPLETE,
	LFIRST,
	LFORWARD,
	LHISTORY,
	LKILL,
	LLAST,
	LLINE,
	LLITERAL,
	LNEXT,
	LNEXTPAGE,
	LPREV,
	LPREVPAGE,
	LREJECT,
	LWORD,
};

typedef struct Item	Item;

struct Item {
	char*	string;
	char*	retstring;
	Item*	next_link;
	Item*	next;
	Item*	prev;
	int	len;
	int	width;
};

EXTERN struct {
	char*	string;
	char*	end;
	char*	pos;
	char*	pos_end;
	int	size;

	char*	filter;
	int	filter_start;
} input;

extern char	binding_spec[];

EXTERN int	numlock;

EXTERN long	xtime;
EXTERN Image*	ibuf;
EXTERN Font*	font;
EXTERN CTuple	cnorm, csel;
EXTERN bool	ontop;

EXTERN Cursor	cursor[1];
EXTERN Visual*	render_visual;

EXTERN IxpServer	srv;

EXTERN Window*	barwin;

EXTERN Item*	items;
EXTERN Item*	matchfirst;
EXTERN Item*	matchstart;
EXTERN Item*	matchend;
EXTERN Item*	matchidx;

EXTERN Item	hist;
EXTERN Item*	histidx;

EXTERN int	maxwidth;
EXTERN int	result;

EXTERN  char*	(*find)(const char*, const char*);
EXTERN  int	(*compare)(const char*, const char*, size_t);

EXTERN char*	prompt;
EXTERN int	promptw;

