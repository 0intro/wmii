/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include <string.h>
#include <cext.h>
#include "blitz.h"

static void
xdrawbg(Display *dpy, Drawable drawable, GC gc, XRectangle rect, BlitzColor c)
{
	XPoint points[5];
	XSetForeground(dpy, gc, c.bg);
	XFillRectangles(dpy, drawable, gc, &rect, 1);
	XSetLineAttributes(dpy, gc, 1, LineSolid, CapButt, JoinMiter);
	XSetForeground(dpy, gc, c.border);
	points[0].x = rect.x;
	points[0].y = rect.y;
	points[1].x = rect.width - 1;
	points[1].y = 0;
	points[2].x = 0;
	points[2].y = rect.height - 1;
	points[3].x = -(rect.width - 1);
	points[3].y = 0;
	points[4].x = 0;
	points[4].y = -(rect.height - 1);
	XDrawLines(dpy, drawable, gc, points, 5, CoordModePrevious);
}

void
blitz_draw_tile(BlitzBrush *b)
{
	xdrawbg(b->blitz->display, b->drawable, b->gc, b->rect, b->color);
}

void
blitz_draw_label(BlitzBrush *b, char *text)
{
	unsigned int x, y, w, h, shortened, len;
	static char buf[2048];
	XGCValues gcv;

	blitz_draw_tile(b);

	if (!text)
		return;

	x = y = shortened = 0;
	w = h = 1;
	cext_strlcpy(buf, text, sizeof(buf));
	len = strlen(buf);
	gcv.foreground = b->color.fg;
	gcv.background = b->color.bg;
	if(b->font->set)
		XChangeGC(b->blitz->display, b->gc, GCForeground | GCBackground, &gcv);
	else {
		gcv.font = b->font->xfont->fid;
		XChangeGC(b->blitz->display, b->gc, GCForeground | GCBackground | GCFont, &gcv);
	}

	h = b->font->ascent + b->font->descent;
	y = b->rect.y + b->rect.height / 2 - h / 2 + b->font->ascent;

	/* shorten text if necessary */
	while (len && (w = blitz_textwidth(b->font, buf)) > b->rect.width) {
		buf[len - 1] = 0;
		len--;
		shortened = 1;
	}

	if (w > b->rect.width)
		return;

	/* mark shortened info in the string */
	if (shortened) {
		if (len > 3)
			buf[len - 3] = '.';
		if (len > 2)
			buf[len - 2] = '.';
		if (len > 1)
			buf[len - 1] = '.';
	}
	switch (b->align) {
	case EAST:
		x = b->rect.x + b->rect.width - (h / 2 + w);
		break;
	case CENTER:
		x = b->rect.x + (b->rect.width - w) / 2;
		break;
	default:
		x = b->rect.x + h / 2;
		break;
	}
	if(b->font->set)
		XmbDrawString(b->blitz->display, b->drawable, b->font->set, b->gc,
				x, y, buf, len);
	else
		XDrawString(b->blitz->display, b->drawable, b->gc, x, y, buf, len);
}

void
blitz_draw_input(BlitzInput *i)
{
	xdrawbg(i->blitz->display, i->drawable, i->gc, i->rect, i->norm);

	if (!i)
		return;
}
