/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cext.h>
#include "blitz.h"

static void
xchangegc(BlitzInput *i, BlitzColor *c, Bool invert)
{
	XGCValues gcv;

	if(invert) {
		gcv.foreground = c->bg;
		gcv.background = c->fg;
	}
	else {
		gcv.foreground = c->fg;
		gcv.background = c->bg;
	}
	if(i->font->set)
		XChangeGC(i->blitz->display, i->gc, GCForeground | GCBackground, &gcv);
	else {
		gcv.font = i->font->xfont->fid;
		XChangeGC(i->blitz->display, i->gc, GCForeground | GCBackground | GCFont, &gcv);
	}
}

static void
xdrawtextpart(BlitzInput *i, BlitzColor *c, char *start, char *end,
				int *xoff, int yoff, unsigned int boxw)
{
	char *p, buf[2];

	xchangegc(i, c, False);
	buf[1] = 0;
	for(p = start; p && *p && p != end; p++) {
		*buf = *p;
		if(p == i->cursor)
			xchangegc(i, c, True);
		if(i->font->set)
			XmbDrawImageString(i->blitz->display, i->drawable, i->font->set, i->gc,
					*xoff, yoff, buf, 1);
		else
			XDrawImageString(i->blitz->display, i->drawable, i->gc, *xoff, yoff,
					buf, 1);
		*xoff += boxw;
		if(p == i->cursor)
			xchangegc(i, c, False);
	}
}


void
xget_fontmetric(BlitzInput *i, int *x, int *y, unsigned int *w, unsigned int *h)
{
	*w = i->font->rbearing - i->font->lbearing;
	*h = i->font->ascent + i->font->descent;
	/* XXX: This is a temporary hack */
	*x = i->rect.x + (i->rect.height - *h) / 2 + i->font->rbearing;
	*y = i->rect.y + (i->rect.height - *h) / 2 + i->font->ascent;
}

void
blitz_draw_input(BlitzInput *i)
{
	int xoff, yoff;
	unsigned int boxw, boxh, nbox;

	if (!i)
		return;

	blitz_drawbg(i->blitz->display, i->drawable, i->gc, i->rect, i->norm);
	xget_fontmetric(i, &xoff, &yoff, &boxw, &boxh);
	nbox = i->rect.width / boxw;

	/* draw normal text */
	xdrawtextpart(i, &i->norm, i->text, i->selstart, &xoff, yoff, boxw);
	/* draw sel text */
	xdrawtextpart(i, &i->sel, i->selstart, i->selend, &xoff, yoff, boxw);
	/* draw remaining normal text */
	xdrawtextpart(i, &i->norm, i->selend, nil, &xoff, yoff, boxw);
}

char *
blitz_charof(BlitzInput *i, int x, int y)
{
	int xoff, yoff;
	unsigned int boxw, boxh, nbox, cbox, l;

	if(!i->text || (y < i->rect.y) || (y > i->rect.y + i->rect.height))
		return nil;
	xget_fontmetric(i, &xoff, &yoff, &boxw, &boxh);
	nbox = i->rect.width / boxw;
	cbox = (x - i->rect.x) / boxw;

	if(cbox > nbox)
		return nil;

	if((l = strlen(i->text)) > cbox)
		return i->text + cbox;
	else
		return i->text + l;
}
