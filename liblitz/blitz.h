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

enum {BLITZ_LABEL, BLITZ_LAST};

typedef struct Blitz Blitz;
typedef enum BlitzAlign BlitzAlign;
typedef struct BlitzColor BlitzColor;
typedef struct BlitzFont BlitzFont;
typedef struct BlitzLabel BlitzLabel;
typedef struct BlitzWin BlitzWin;
typedef union BlitzWidget BlitzWidget;

struct Blitz {
	Display *display;
	int screen;
	Window root;

};

struct BlitzWin{
	Drawable drawable;
	GC gc;
	XRectangle rect;

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

struct BlitzLabel {
	int type;
	BlitzWin *win;
	XRectangle rect;
	BlitzColor color;
	/* widget specific */
	BlitzAlign align;
	BlitzFont font;
	char *data;
	void (*draw)(char *text); /* also called on expose */
};

union BlitzWidget {
	int type;
	BlitzLabel label;
};

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
void blitz_x11_init(Display *dpy);
void blitz_process_x11_event();

/* draw.c */
void blitz_drawlabel(BlitzDraw *d);
void blitz_drawborder(BlitzDraw *d);

/* label.c */
BlitzWidget *blitz_create_label(BlitzWin *win);
void blitz_destroy_label(BlitzWidget *widget);

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
