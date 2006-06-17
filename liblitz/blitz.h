/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <X11/Xlib.h>
#include <X11/Xlocale.h>

#define BLITZ_FONT		"fixed"
#define BLITZ_SELCOLORS		"#ffffff #335577 #447799"
#define BLITZ_NORMCOLORS	"#222222 #eeeeee #666666"

typedef struct Blitz Blitz;
typedef enum BlitzAlign BlitzAlign;
typedef struct BlitzColor BlitzColor;
typedef struct BlitzFont BlitzFont;
typedef struct BlitzWidget BlitzWidget;

struct Blitz {
	Display *display;
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
};

struct BlitzFont {
	XFontStruct *xfont;
	XFontSet set;
	int ascent;
	int descent;
};

struct BlitzWidget {
	Drawable drawable;
	GC gc;
	BlitzColor color;
	BlitzAlign align;
	BlitzFont font;
	XRectangle rect;	/* relative rect */
	XRectangle *notch;	/* relative notch rect */
	char *text;
	void (*draw)(BlitzWidget *);
};

/* obsolete, will be replaced soon */
typedef struct {
	BlitzAlign align;
	Drawable drawable;
	GC gc;
	BlitzColor color;
	BlitzFont font;
	XRectangle rect;	/* relative rect */
	XRectangle *notch;	/* relative notch rect */
	char *data;
} BlitzDraw;
/***/

Blitz __blitz;

/* blitz.c */
void blitz_x11_init(Display *dpy);
Bool blitz_x11_event(XEvent *ev);

/* color.c */
int blitz_loadcolor(BlitzColor *c, char *colstr);

/* label.c */
void blitz_drawlabel(BlitzDraw *d);
void blitz_drawborder(BlitzDraw *d);

/* input.c */
BlitzWidget *blitz_create_input(Drawable drawable, GC gc);
void blitz_draw_input(BlitzWidget *i);
void blitz_destroy_input(BlitzWidget *i);

/* tile.c */
BlitzWidget *blitz_create_tile(Drawable drawable, GC gc);
void blitz_draw_tile(BlitzWidget *t);
void blitz_destroy_tile(BlitzWidget *t);

/* font.c */
unsigned int blitz_textwidth(BlitzFont *font, char *text);
void blitz_loadfont(BlitzFont *font, char *fontstr);
