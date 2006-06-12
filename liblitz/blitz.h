/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <X11/Xlib.h>
#include <X11/Xlocale.h>

#define BLITZ_FONT		"fixed"
#define BLITZ_SELCOLORS		"#ffffff #335577 #447799"
#define BLITZ_NORMCOLORS	"#222222 #eeeeee #666666"

typedef enum {
    NORTH = 0x01,
    EAST  = 0x02,
    SOUTH = 0x04,
    WEST  = 0x08,
    NEAST = NORTH | EAST,
    NWEST = NORTH | WEST,
    SEAST = SOUTH | EAST,
    SWEST = SOUTH | WEST,
    CENTER = NEAST | SWEST
} BlitzAlign;

typedef struct {
	unsigned long bg;
	unsigned long fg;
	unsigned long border;
} BlitzColor;

typedef struct {
	XFontStruct *xfont;
	XFontSet set;
	int ascent;
	int descent;
} BlitzFont;

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

/* font.c */
unsigned int blitz_textwidth(Display *dpy, BlitzFont *font, char *text);
void blitz_loadfont(Display *dpy, BlitzFont *font, char *fontstr);

/* color.c */
int blitz_loadcolor(Display *dpy, BlitzColor *c, int mon, char *colstr);

/* draw.c */
void blitz_drawlabel(Display *dpy, BlitzDraw *d);
void blitz_drawborder(Display *dpy, BlitzDraw *d);

/* geometry.c */
int blitz_strtorect(XRectangle *r, const char *val);
BlitzAlign blitz_quadofcoord(XRectangle *rect, int x, int y);
Bool blitz_ispointinrect(int x, int y, XRectangle *r);
