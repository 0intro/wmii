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
	unsigned int xoff, yoff, xcursor, h;
	char *start, *end;

	if (!i)
		return;

	blitz_drawbg(i->blitz->display, i->drawable, i->gc, i->rect, i->color, True);

	h = (i->font->ascent + i->font->descent);
	yoff = i->rect.y + (i->rect.height - h) / 2 + i->font->ascent;
	xcursor = xoff = i->rect.x + i->rect.height / 2;

	start = i->curstart;
	end = i->curend;
	if(i->curstart && i->curend) {
		if(i->curstart < i->curend) {
			start = i->curstart;
			end = i->curend;
		}
		else {
			start = i->curend;
			end = i->curstart;
		}
	}

	/* draw normal text */
	xchangegc(i, &i->color, False);
	xdrawtextpart(i, i->text, start, &xoff, yoff);
	xcursor = xoff;
	/* draw sel text */
	xchangegc(i, &i->color, True);
	xdrawtextpart(i, start, end, &xoff, yoff);
	/* draw remaining normal text */
	xchangegc(i, &i->color, False);
	xdrawtextpart(i, end, nil, &xoff, yoff);

	/* draw cursor */
	if(!start && !end)
		xcursor = xoff;
	if(start == end)
		blitz_drawcursor(i->blitz->display, i->drawable, i->gc,
				xcursor, yoff - h + 2, h - 1, i->color);
}

Bool
blitz_ispointinrect(int x, int y, XRectangle * r)
{
	return (x >= r->x) && (x <= r->x + r->width)
		&& (y >= r->y) && (y <= r->y + r->height);
}

static char *
xcharof(BlitzInput *i, int x, char *start, unsigned int len)
{
	unsigned int piv, tw;

	if(!(piv = len / 2))
		return start; /* found */

	tw = blitz_textwidth_l(i->font, start, piv);

	if(x < tw)
		return xcharof(i, x, start, piv);
	else
		return xcharof(i, x - tw, start + piv, strlen(start + piv));
}

static char *
charof(BlitzInput *i, int x, int y)
{
	unsigned int len;

	if(!i->text || !blitz_ispointinrect(x, y, &i->rect))
		return nil;

	len = strlen(i->text);
	/* normalize and check x */
	if((x -= (i->rect.x + i->rect.height / 2)) < 0)
		return i->text;
	else if(x > blitz_textwidth_l(i->font, i->text, len))
		return nil;

	return xcharof(i, x, i->text, strlen(i->text));
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
	return (i->curstart == ostart) && (i->curend == oend);
}

Bool
blitz_brelease_input(BlitzInput *i, int x, int y)
{
	char *oend;

	if(!(i->drag = blitz_ispointinrect(x, y, &i->rect)) ||
			!i->curstart)
		return False;
	oend = i->curend;
	i->curend = charof(i, x, y);
	i->drag = False;
	return i->curend == oend;
}

Bool
blitz_bmotion_input(BlitzInput *i, int x, int y)
{
	char *oend;

	if(!i->drag || !(i->drag = blitz_ispointinrect(x, y, &i->rect))
			|| !i->curstart)
		return False;

	oend = i->curend;
	i->curend = charof(i, x, y);
	return i->curend == oend;
}
