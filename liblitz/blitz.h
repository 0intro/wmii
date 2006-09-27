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
	unsigned int height;
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

/* blitz.c */
extern unsigned char *blitz_getselection(unsigned long offset,
		unsigned long *len, unsigned long *remain);
extern void blitz_selrequest(Blitz *blitz, XSelectionRequestEvent *rq, char *text);

/* brush.c */
extern void blitz_draw_label(BlitzBrush *b, char *text);
extern void blitz_draw_tile(BlitzBrush *b);

/* color.c */
extern int blitz_loadcolor(Blitz *blitz, BlitzColor *c);

/* draw.c */
extern void blitz_drawbg(Display *dpy, Drawable drawable, GC gc,
		XRectangle rect, BlitzColor c, Bool border);
extern void blitz_drawcursor(Display *dpy, Drawable drawable, GC gc,
				int x, int y, unsigned int h, BlitzColor c);

/* font.c */
extern unsigned int blitz_textwidth(BlitzFont *font, char *text);
extern unsigned int blitz_textwidth_l(BlitzFont *font, char *text, unsigned int len);
extern void blitz_loadfont(Blitz *blitz, BlitzFont *font);
extern unsigned int blitz_labelh(BlitzFont *font);
