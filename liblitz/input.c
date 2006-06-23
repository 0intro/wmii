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
xdrawtextpart(BlitzInput *i, char *start, char *end,
				int *xoff, int yoff, unsigned int boxw)
{
	char *p, buf[2];

	buf[1] = 0;
	for(p = start; p && *p && p != end; p++) {
		*buf = *p;
		if(i->font->set)
			XmbDrawImageString(i->blitz->display, i->drawable, i->font->set, i->gc,
					*xoff, yoff, buf, 1);
		else
			XDrawImageString(i->blitz->display, i->drawable, i->gc, *xoff, yoff,
					buf, 1);
		*xoff += boxw;
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

	blitz_drawbg(i->blitz->display, i->drawable, i->gc, i->rect, i->color);
	xget_fontmetric(i, &xoff, &yoff, &boxw, &boxh);
	nbox = i->rect.width / boxw;

	/* draw normal text */
	xchangegc(i, &i->color, False);
	xdrawtextpart(i, i->text, i->curstart, &xoff, yoff, boxw);
	/* draw sel text */
	xchangegc(i, &i->color, True);
	xdrawtextpart(i, i->curstart, i->curend, &xoff, yoff, boxw);
	/* draw remaining normal text */
	xchangegc(i, &i->color, False);
	xdrawtextpart(i, i->curend, nil, &xoff, yoff, boxw);
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
