/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <X11/Xlib.h>
#include <X11/Xlocale.h>

#define BLITZ_FONT		"fixed"
#define BLITZ_SELCOLORS		"#ffffff #335577 #447799"
#define BLITZ_NORMCOLORS	"#222222 #eeeeee #666666"
#define BLITZ_B1COLORS		"#000000 #00ffff #000000"
#define BLITZ_B2COLORS		"#000000 #ff0000 #000000"
#define BLITZ_B3COLORS		"#000000 #00ff00 #000000"

typedef struct Blitz Blitz;
typedef enum BlitzAlign BlitzAlign;
typedef struct BlitzColor BlitzColor;
typedef struct BlitzFont BlitzFont;
typedef struct BlitzBrush BlitzBrush;
typedef struct BlitzInput BlitzInput;

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

struct BlitzColor {
	unsigned long bg;
	unsigned long fg;
	unsigned long border;
	char colstr[24]; /* #RRGGBB #RRGGBB #RRGGBB */
};

struct BlitzFont {
	XFontStruct *xfont;
	XFontSet set;
	int ascent;
	int descent;
	char *fontstr;
};

struct BlitzBrush {
	Blitz *blitz;
	Drawable drawable;
	GC gc;
	Bool border;
	BlitzColor color;
	BlitzAlign align;
	BlitzFont *font;
	XRectangle rect;	/* relative rect */
};

struct BlitzInput {
	Blitz *blitz;
	char *text;
	char *curstart;
	char *curend;
	Cursor cursor;
	Cursor input;
	Cursor def;
	Bool drag;
	unsigned int size;
	unsigned int len;
	unsigned long tdbclk;
	int xdbclk, ydbclk;
	int button;
	Drawable drawable;
	Window win;
	GC gc;
	BlitzColor color;
	BlitzColor bcolor[3];
	BlitzFont *font;
	XRectangle rect;	/* relative rect */
	void *aux;
	void (*draw)(void *aux);
};

/* blitz.c */
unsigned char *blitz_getselection(unsigned long offset,
		unsigned long *len, unsigned long *remain);
void blitz_selrequest(Blitz *blitz, XSelectionRequestEvent *rq, char *text);

/* brush.c */
void blitz_draw_label(BlitzBrush *b, char *text);
void blitz_draw_tile(BlitzBrush *b);

/* color.c */
int blitz_loadcolor(Blitz *blitz, BlitzColor *c);

/* draw.c */
void blitz_drawbg(Display *dpy, Drawable drawable, GC gc,
		XRectangle rect, BlitzColor c, Bool border);
void blitz_drawcursor(Display *dpy, Drawable drawable, GC gc,
				int x, int y, unsigned int h, BlitzColor c);

/* font.c */
unsigned int blitz_textwidth(BlitzFont *font, char *text);
unsigned int blitz_textwidth_l(BlitzFont *font, char *text, unsigned int len);
void blitz_loadfont(Blitz *blitz, BlitzFont *font);

/* input.c */
void blitz_draw_input(BlitzInput *i);
/* blitz_b* functions return True on expose */
void blitz_bpress_input(BlitzInput *i, int button, int x, int y);
void blitz_brelease_input(BlitzInput *i, int button, int x, int y, unsigned long time);
void blitz_bmotion_input(BlitzInput *i, int x, int y);
Bool blitz_ispointinrect(int x, int y, XRectangle * r);
void blitz_setinput(BlitzInput *i, char *text);
void blitz_kpress_input(BlitzInput *i, unsigned long mod, KeySym k, char *text);
