/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdio.h>
#include <string.h>
#include "blitz.h"

XFontStruct *blitz_getfont(Display * dpy, char *fontstr)
{
	XFontStruct *font;
	font = XLoadQueryFont(dpy, fontstr);
	if (!font) {
		font = XLoadQueryFont(dpy, "fixed");
		if (!font) {
			fprintf(stderr, "%s", "wmii: error, cannot load fixed font\n");
			return 0;
		}
	}
	return font;
}

unsigned long blitz_loadcolor(Display * dpy, int mon, char *colstr)
{
	XColor color;
	char col[8];

	cext_strlcpy(col, colstr, sizeof(col));
	col[7] = 0;
	XAllocNamedColor(dpy, DefaultColormap(dpy, mon), col, &color, &color);
	return color.pixel;
}

static void draw_bg(Display * dpy, Draw * d)
{
	XRectangle rect[4];
	XSetForeground(dpy, d->gc, d->bg);
	if (!d->notch) {
		XFillRectangles(dpy, d->drawable, d->gc, &d->rect, 1);
		return;
	}
	rect[0] = d->rect;
	rect[0].height = d->notch->y;
	rect[1] = d->rect;
	rect[1].y = d->notch->y;
	rect[1].width = d->notch->x;
	rect[1].height = d->notch->height;
	rect[2].x = d->notch->x + d->notch->width;
	rect[2].y = d->notch->y;
	rect[2].width = d->rect.width - (d->notch->x + d->notch->width);
	rect[2].height = d->notch->height;
	rect[3] = d->rect;
	rect[3].y = d->notch->y + d->notch->height;
	rect[3].height = d->rect.height - (d->notch->y + d->notch->height);
	XFillRectangles(dpy, d->drawable, d->gc, rect, 4);
}

static void xdraw_border(Display * dpy, Draw * d)
{
	XPoint points[5];

	XSetLineAttributes(dpy, d->gc, 1, LineSolid, CapButt, JoinMiter);
	XSetForeground(dpy, d->gc, d->border);
	points[0].x = d->rect.x;
	points[0].y = d->rect.y;
	points[1].x = d->rect.width - 1;
	points[1].y = 0;
	points[2].x = 0;
	points[2].y = d->rect.height - 1;
	points[3].x = -(d->rect.width - 1);
	points[3].y = 0;
	points[4].x = 0;
	points[4].y = -(d->rect.height - 1);
	XDrawLines(dpy, d->drawable, d->gc, points, 5, CoordModePrevious);
}

static void draw_text(Display * dpy, Draw * d)
{
	unsigned int x, y, w, h, shortened = 0;
	size_t len = 0;
	static char text[2048];

	if (!d->data)
		return;

	len = strlen(d->data);
	cext_strlcpy(text, d->data, sizeof(text));
	XSetFont(dpy, d->gc, d->font->fid);
	h = d->font->ascent + d->font->descent;
	y = d->rect.y + d->rect.height / 2 - h / 2 + d->font->ascent;

	/* shorten text if necessary */
	while (len && (w = XTextWidth(d->font, text, len)) > d->rect.width) {
		text[len - 1] = 0;
		len--;
		shortened = 1;
	}

	if (w > d->rect.width)
		return;

	/* mark shortened info in the string */
	if (shortened) {
		if (len > 3)
			text[len - 3] = '.';
		if (len > 2)
			text[len - 2] = '.';
		if (len > 1)
			text[len - 1] = '.';
	}
	switch (d->align) {
	case WEST:
		x = d->rect.x + h / 2;
		break;
	case EAST:
		x = d->rect.x + d->rect.width - (h / 2 + w);
		break;
	default:					/* CENTER */
		x = d->rect.x + (d->rect.width - w) / 2;
		break;
	}

	XSetBackground(dpy, d->gc, d->bg);
	/*
	 * uncomment, if you want get an shadow effect XSetForeground(dpy,
	 * d->gc, BlackPixel(dpy, DefaultScreen(dpy))); XDrawString(dpy,
	 * d->drawable, d->gc, x + 1, y + 1, text, len);
	 */
	XSetForeground(dpy, d->gc, d->fg);
	XDrawString(dpy, d->drawable, d->gc, x, y, text, len);
}

/* draws meter */
void blitz_drawmeter(Display * dpy, Draw * d)
{
	unsigned int offy, mh, val, w = d->rect.width - 4;

	if (!d->data || strncmp(d->data, "%m:", 3))
		return;

	val = blitz_strtonum(&d->data[3], 0, 100);
	draw_bg(dpy, d);
	xdraw_border(dpy, d);

	/* draw bg gradient */
	mh = ((d->rect.height - 4) * val) / 100;
	offy = d->rect.y + d->rect.height - 2 - mh;
	XSetForeground(dpy, d->gc, d->fg);
	XFillRectangle(dpy, d->drawable, d->gc, d->rect.x + 2, offy, w, mh);
}

static void xdraw_label(Display * dpy, Draw * d)
{
	draw_bg(dpy, d);
	if (d->data)
		draw_text(dpy, d);
}

/* draws label */
void blitz_drawlabel(Display * dpy, Draw * d)
{
	xdraw_label(dpy, d);
	xdraw_border(dpy, d);
}

void blitz_drawlabelnoborder(Display * dpy, Draw * d)
{
	xdraw_label(dpy, d);
}
