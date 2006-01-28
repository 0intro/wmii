/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <X11/Xlib.h>
#include <cext.h>

#define BLITZ_FONT				"fixed"
#define BLITZ_SEL_FG_COLOR		"#eeeeee"
#define BLITZ_SEL_BG_COLOR		"#506070"
#define BLITZ_SEL_BORDER_COLOR	"#708090"
#define BLITZ_NORM_FG_COLOR		"#bbbbbb"
#define BLITZ_NORM_BG_COLOR		"#222222"
#define BLITZ_NORM_BORDER_COLOR	"#000000"
#define BLITZ_SEL_COLOR			"#eeeeee #506070 #708090"
#define BLITZ_NORM_COLOR		"#222222 #000000 #000000"

typedef enum {
	CENTER, WEST, NWEST, NORTH, NEAST, EAST,
	SEAST, SOUTH, SWEST
} Align;

typedef struct Draw Draw;

struct Draw {
	Drawable drawable;
	GC gc;
	unsigned long bg;
	unsigned long fg;
	unsigned long border;
	Align align;
	XFontStruct *font;
	XRectangle rect;			/* relative rect */
	XRectangle *notch;			/* relative notch rect */
	char *data;
};

/* draw.c */
XFontStruct *blitz_getfont(Display * dpy, char *fontstr);
unsigned long blitz_loadcolor(Display * dpy, int mon, char *colstr);
void blitz_drawlabel(Display * dpy, Draw * r);
void blitz_drawmeter(Display * dpy, Draw * r);
void blitz_drawlabelnoborder(Display * dpy, Draw * r);

/* geometry.c */
int blitz_strtoalign(Align *result, char *val);
int blitz_strtorect(XRectangle * root, XRectangle * r, char *val);
Bool blitz_ispointinrect(int x, int y, XRectangle * r);
int blitz_distance(XRectangle * origin, XRectangle * target);
void blitz_getbasegeometry(unsigned int size, unsigned int *cols, unsigned int *rows);

/* mouse.c */
char *blitz_buttontostr(unsigned int button);
unsigned int blitz_strtobutton(char *val);

/* kb.c */
char *blitz_modtostr(unsigned long mod);
unsigned long blitz_strtomod(char *val);

/* util.c */
long long blitz_strtonum(const char *numstr, long long minval, long long maxval);
