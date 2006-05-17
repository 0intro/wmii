/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <X11/Xlib.h>
#include <X11/Xlocale.h>

#define BLITZ_FONT		"fixed"
#define BLITZ_SELCOLORS		"#ffffff #285577 #4c7899"
#define BLITZ_NORMCOLORS	"#222222 #eeeeee #666666"

/*
#ifdef X_HAVE_UTF8_STRING
#define Xi18nTextPropertyToTextList Xutf8TextPropertyToTextList
#define Xi18nDrawString Xutf8DrawString
#define Xi18nTextExtents Xutf8TextExtents
#else
*/
#define Xi18nTextPropertyToTextList XmbTextPropertyToTextList
#define Xi18nDrawString XmbDrawString
#define Xi18nTextExtents XmbTextExtents
/*
#endif
*/

typedef enum {
	CENTER, WEST, NWEST, NORTH, NEAST, EAST,
	SEAST, SOUTH, SWEST
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
BlitzAlign blitz_align_of_rect(XRectangle *rect, int x, int y);
int blitz_strtoalign(BlitzAlign *result, char *val);
int blitz_strtorect(XRectangle *root, XRectangle *r, char *val);
Bool blitz_ispointinrect(int x, int y, XRectangle *r);
int blitz_distance(XRectangle *origin, XRectangle *target);
void blitz_getbasegeometry(unsigned int size, unsigned int *cols, unsigned int *rows);
