/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <X11/Xlib.h>
#include <X11/Xlocale.h>

#define BLITZ_FONT		"fixed"
#define BLITZ_SELCOLORS		"#ffffff #335577 #447799"
#define BLITZ_NORMCOLORS	"#222222 #eeeeee #666666"
#define BLITZ_FRAME_MASK	SubstructureRedirectMask | SubstructureNotifyMask \
							| ExposureMask | ButtonPressMask | ButtonReleaseMask;

typedef struct {
	Display *display;
	int screen;
	Window root;
} Blitz;

typedef struct {
	Drawable drawable;
	GC gc;
	XRectangle rect;
} BlitzWin;

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

Blitz __blitz;

/* blitz.c */
void blitz_init(Display *dpy);

/* draw.c */
void blitz_drawlabel(BlitzDraw *d);
void blitz_drawborder(BlitzDraw *d);

/* font.c */
unsigned int blitz_textwidth(BlitzFont *font, char *text);
void blitz_loadfont(BlitzFont *font, char *fontstr);

/* color.c */
int blitz_loadcolor(BlitzColor *c, char *colstr);

/* window.c */
BlitzWin *blitz_create_win(unsigned long mask, 
							int x, int y, int w, int h);
void blitz_resize_win(BlitzWin *win,
						int x, int y, int w, int h);
void blitz_destroy_win(BlitzWin *win);
