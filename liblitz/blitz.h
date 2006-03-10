/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <X11/Xlib.h>
#include <X11/xpm.h>
#include <cext.h>

#define BLITZ_FONT				"fixed"
#define BLITZ_SELCOLORS			"#ffffff #285577 #4c7899"
#define BLITZ_NORMCOLORS		"#222222 #eeeeee #666666"

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
	Align align;
	Drawable drawable;
	GC gc;
	Color color;
	XFontStruct *font;
	XRectangle rect;			/* relative rect */
	XRectangle *notch;			/* relative notch rect */
	char *data;
} Draw;

typedef struct {
	XImage* image;
	Pixmap mask;
} Icon;

/* draw.c */
XFontStruct *blitz_getfont(Display *dpy, char *fontstr);
int blitz_loadcolor(Display *dpy, int mon, char *colstr, Color *c);
void blitz_drawlabel(Display *dpy, Draw *r);
void blitz_drawmeter(Display *dpy, Draw *r);
int blitz_createicon(Display *dpy, Icon *ico, char *data[]);
void blitz_freeicon(Display *dpy, Icon *ico);
void blitz_drawicon(Display *dpy, Draw *d, Icon *ico);
void blitz_drawborder(Display *dpy, Draw *r);

/* geometry.c */
int blitz_strtoalign(Align *result, char *val);
int blitz_strtorect(XRectangle *root, XRectangle *r, char *val);
Bool blitz_ispointinrect(int x, int y, XRectangle *r);
int blitz_distance(XRectangle *origin, XRectangle *target);
void blitz_getbasegeometry(unsigned int size, unsigned int *cols, unsigned int *rows);
