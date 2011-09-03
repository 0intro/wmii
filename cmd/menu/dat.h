#define _XOPEN_SOURCE 600
#define IXP_P9_STRUCTS
#define IXP_NO_P9_
#include <fmt.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ixp.h>
#include <stuff/x.h>
#include <stuff/util.h>

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
	LDELETE,
	LFIRST,
	LFORWARD,
	LHISTORY,
	LKILL,
	LLAST,
	LLINE,
	LLITERAL,
	LNEXT,
	LNEXTPAGE,
	LPASTE,
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

EXTERN struct {
	Window*		win;
	Image*		buf;
	char*		prompt;
	int		height;
	int		rows;
	bool		ontop;
	Rectangle	itemr;
	Point		arrow;
} menu;

extern char	binding_spec[];

EXTERN IxpServer	srv;

EXTERN struct {
	Item*	all;
	Item*	first;
	Item*	start;
	Item*	end;
	Item*	sel;
	int	maxwidth;
} match;

Font*		font;
CTuple		cnorm;
CTuple		csel;

EXTERN Item	hist;
EXTERN Item*	histsel;

EXTERN int	itempad;
EXTERN int	result;

EXTERN char*	(*find)(const char*, const char*);
EXTERN int	(*compare)(const char*, const char*, size_t);

