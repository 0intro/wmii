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
typedef struct BlitzLabel BlitzLabel;
#define BLITZLABEL(p) ((BlitzLabel *)(p))
typedef struct BlitzLayout BlitzLayout;
#define BLITZLAYOUT(p) ((BlitzLayout *)(p))
typedef union BlitzWidget BlitzWidget;
typedef struct BlitzWin BlitzWin;

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

struct BlitzWin{
	Drawable drawable;
	GC gc;
	XRectangle rect;
};

struct BlitzLayout {
	XRectangle rect;
	Bool expand;
	BlitzWidget *next;
	void (*draw)(BlitzWidget *);
	/* widget specific */
	BlitzWin *win;
	BlitzWidget *rows;
	BlitzWidget *cols;
	void (*scale)(BlitzWidget *);
};

struct BlitzLabel {
	XRectangle rect;
	Bool expand;
	BlitzWidget *next;
	void (*draw)(BlitzWidget *);
	/* widget specific */
	BlitzColor color;
	BlitzAlign align;
	BlitzFont font;
	char *text;
};

union BlitzWidget {
	XRectangle rect;
	Bool expand;
	BlitzWidget *next;
	void (*draw)(BlitzWidget *);
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

/* color.c */
int blitz_loadcolor(BlitzColor *c, char *colstr);

/* label.c */
void blitz_drawlabel(BlitzDraw *d);
void blitz_drawborder(BlitzDraw *d);

/* layout.c */
BlitzLayout *blitz_create_layout(BlitzWin *win);

/* font.c */
unsigned int blitz_textwidth(BlitzFont *font, char *text);
void blitz_loadfont(BlitzFont *font, char *fontstr);

/* window.c */
BlitzWin *blitz_create_win(unsigned long mask, 
							int x, int y, int w, int h);
void blitz_resize_win(BlitzWin *win,
						int x, int y, int w, int h);
void blitz_destroy_win(BlitzWin *win);
