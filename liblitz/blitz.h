/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <X11/Xlib.h>
#include <cext.h>

#define BLITZ_FONT				"fixed"
#define BLITZ_SEL_COLOR			"#ffffff #4c7899 #6e9abb"
#define BLITZ_NORM_COLOR		"#dddddd #222c33 #444e66"

typedef enum {
	CENTER, WEST, NWEST, NORTH, NEAST, EAST,
	SEAST, SOUTH, SWEST
} Align;

typedef struct {
	unsigned long bg;
	unsigned long fg;
	unsigned long border;
} Color;

typedef struct {
	Drawable drawable;
	GC gc;
	Color color;
	Align align;
	XFontStruct *font;
	XRectangle rect;			/* relative rect */
	XRectangle *notch;			/* relative notch rect */
	char *data;
} Draw;

/* draw.c */
XFontStruct *blitz_getfont(Display * dpy, char *fontstr);
int blitz_loadcolor(Display *dpy, int mon, char *colstr, Color *c);
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
unsigned long blitz_strtomod(char *val);

/* util.c */
long long blitz_strtonum(const char *numstr, long long minval, long long maxval);
