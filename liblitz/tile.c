/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include <cext.h>
#include "blitz.h"

BlitzWidget *
blitz_create_tile(Drawable drawable, GC gc)
{
	BlitzWidget *t = cext_emallocz(sizeof(BlitzWidget));
	t->drawable = drawable;
	t->gc = gc;
	return t;
}

void
blitz_draw_tile(BlitzWidget *t)
{
	XPoint points[5];
	XSetForeground(__blitz.display, t->gc, t->color.bg);
	XFillRectangles(__blitz.display, t->drawable, t->gc, &t->rect, 1);
	XSetLineAttributes(__blitz.display, t->gc, 1, LineSolid, CapButt, JoinMiter);
	XSetForeground(__blitz.display, t->gc, t->color.border);
	points[0].x = t->rect.x;
	points[0].y = t->rect.y;
	points[1].x = t->rect.width - 1;
	points[1].y = 0;
	points[2].x = 0;
	points[2].y = t->rect.height - 1;
	points[3].x = -(t->rect.width - 1);
	points[3].y = 0;
	points[4].x = 0;
	points[4].y = -(t->rect.height - 1);
	XDrawLines(__blitz.display, t->drawable, t->gc, points, 5, CoordModePrevious);
}

void
blitz_destroy_tile(BlitzWidget *t)
{
	free(t);
}
