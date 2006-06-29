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
				unsigned int *xoff, unsigned int yoff)
{
	char c;

	if(!start)
		return;
	if(end) {
		c = *end;
		*end = 0;
	}
	if(i->font->set)
		XmbDrawImageString(i->blitz->display, i->drawable, i->font->set, i->gc,
				*xoff, yoff, start, strlen(start));
	else
		XDrawImageString(i->blitz->display, i->drawable, i->gc, *xoff, yoff,
				start, strlen(start));

	*xoff += blitz_textwidth(i->font, start);
	if(end)
		*end = c;
}


void
blitz_draw_input(BlitzInput *i)
{
	unsigned int xoff, yoff;
	if (!i)
		return;

	blitz_drawbg(i->blitz->display, i->drawable, i->gc, i->rect, i->color, True);

	yoff = i->rect.y + (i->rect.height - (i->font->ascent + i->font->descent))
			/ 2 + i->font->ascent;
	xoff = i->rect.x + i->rect.height / 2;

	/* draw normal text */
	xchangegc(i, &i->color, False);
	xdrawtextpart(i, i->text, i->curstart, &xoff, yoff);
	/* draw sel text */
	xchangegc(i, &i->color, True);
	xdrawtextpart(i, i->curstart, i->curend, &xoff, yoff);
	/* draw remaining normal text */
	xchangegc(i, &i->color, False);
	xdrawtextpart(i, i->curend, nil, &xoff, yoff);
}

Bool
blitz_ispointinrect(int x, int y, XRectangle * r)
{
	return (x >= r->x) && (x <= r->x + r->width)
		&& (y >= r->y) && (y <= r->y + r->height);
}

static char *
charof(BlitzInput *i, int x, int y)
{
	char *p, c;
	unsigned int avg;

	if(!i->text || !blitz_ispointinrect(x, y, &i->rect))
		return nil;

	/* normalize x */
	x -= i->rect.x;
	if(x < i->rect.height / 2)
		return nil;
	x -= i->rect.height / 2;

	avg = blitz_textwidth(i->font, i->text) / strlen(i->text);
	for(p=i->text; *p; p++) {
		c = *p;
		*p = 0;
		if(x <= blitz_textwidth(i->font, i->text) + avg) {
			*p = c;
			return p;
		}
		*p = c;
	}
	return nil;
}

Bool
blitz_bpress_input(BlitzInput *i, int x, int y)
{
	char *ostart, *oend;

	if(!(i->drag = blitz_ispointinrect(x, y, &i->rect)))
		return False;
	ostart = i->curstart;
	oend = i->curend;
	i->curstart = i->curend = charof(i, x, y);
	if(i->curend && *i->curend)
		i->curend++;
	return (i->curstart == ostart) && (i->curend == oend);
}

Bool
blitz_brelease_input(BlitzInput *i, int x, int y)
{
	char *oend;

	if(!(i->drag = blitz_ispointinrect(x, y, &i->rect)))
		return False;
	oend = i->curend;
	i->curend = charof(i, x, y);
	if(i->curend && *i->curend)
		i->curend++;
	i->drag = False;
	return i->curend == oend;
}

Bool
blitz_bmotion_input(BlitzInput *i, int x, int y)
{
	char *oend;

	if(!i->drag || !(i->drag = blitz_ispointinrect(x, y, &i->rect)))
		return False;

	oend = i->curend;
	i->curend = charof(i, x, y);
	if(i->curstart > i->curend) {
		char *tmp = i->curend;
		i->curend = i->curstart;
		i->curstart = tmp;
	}
	if(i->curend && *i->curend)
		i->curend++;
	return i->curend == oend;
}
