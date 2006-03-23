/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdio.h>
#include <string.h>
#include "blitz.h"

XFontStruct *
blitz_getfont(Display *dpy, char *fontstr)
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

static unsigned long
xloadcolor(Display *dpy, int mon, char *colstr)
{
	XColor color;
	char col[8];

	cext_strlcpy(col, colstr, sizeof(col));
	col[7] = 0;
	XAllocNamedColor(dpy, DefaultColormap(dpy, mon), col, &color, &color);
	return color.pixel;
}

int
blitz_loadcolor(Display *dpy, int mon, char *colstr, Color *c)
{
	if(!colstr || strlen(colstr) != 23)
		return -1;
	c->fg = xloadcolor(dpy, mon, &colstr[0]);
	c->bg = xloadcolor(dpy, mon, &colstr[8]);
	c->border = xloadcolor(dpy, mon, &colstr[16]);
	return 0;
}

static void
xdrawbg(Display *dpy, Draw *d)
{
	XRectangle rect[4];
	XSetForeground(dpy, d->gc, d->color.bg);
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

void
blitz_drawborder(Display *dpy, Draw *d)
{
	XPoint points[5];

	XSetLineAttributes(dpy, d->gc, 1, LineSolid, CapButt, JoinMiter);
	XSetForeground(dpy, d->gc, d->color.border);
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

static void
xdrawtext(Display *dpy, Draw *d)
{
	unsigned int x = 0, y = 0, w = 1, h = 1, shortened = 0;
	unsigned int len = 0;
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
	case EAST:
		x = d->rect.x + d->rect.width - (h / 2 + w);
		break;
	case CENTER:
		x = d->rect.x + (d->rect.width - w) / 2;
		break;
	default:
		x = d->rect.x + h / 2;
		break;
	}
	XSetBackground(dpy, d->gc, d->color.bg);
	XSetForeground(dpy, d->gc, d->color.fg);
	XDrawString(dpy, d->drawable, d->gc, x, y, text, len);
}

void
blitz_drawlabel(Display *dpy, Draw * d)
{
	xdrawbg(dpy, d);
	if (d->data)
		xdrawtext(dpy, d);
}
