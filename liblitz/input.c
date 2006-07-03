/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cext.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
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

static char *
curend(BlitzInput *i)
{
	if(i->curstart && i->curend) {
		if(i->curstart < i->curend)
			return i->curend;
		else
			return i->curstart;
	}
	else if(i->curend)
		return nil;
	return i->curend;
}

static char *
curstart(BlitzInput *i)
{
	if(i->curstart && i->curend) {
		if(i->curstart < i->curend)
			return  i->curstart;
		else
			return i->curend;
	}
	else if(i->curend)
		return i->curend;
	return i->curstart;
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

	start = curstart(i);
	end = curend(i);

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
	unsigned int tw;

	while((len /= 2)) {
		tw = blitz_textwidth_l(i->font, start, len);

		if(x >= tw) {
			x -= tw;
			start += len;
			len = strlen(start);
		}
	}
	return start; /* found */
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

static char *
cursor(BlitzInput *i)
{
	char *start = curstart(i);
	char *end = curend(i);

	if(!start && i->text)
		return i->text + (i->size - 1);
	return end;
}

static Bool
insert(BlitzInput *i, const char s)
{
	char *buf, *c, *p, *q;

	if(!(c = cursor(i)))
		return False;

	buf = cext_emallocz(++i->size);
	for(p = i->text, q = buf; p != c; p++, q++)
		*q = *p;
	*q = s;
	for(q++; *p; p++, q++)
		*q = *p;

	return True;
}

static Bool
delete(BlitzInput *i)
{
	char *c, *p, *q;

	if(!(c = cursor(i)))
		return False;

	for(q = c, p = c + 1; *p; p++, q++)
		*q = *p;
	return True;
}

static void
left(BlitzInput *i)
{
	char *c;
	if(!(c = cursor(i)))
		return;

	if(c > i->text)
		i->curstart = i->curend = c - 1;
	else
		i->curstart = i->curstart = i->text;
}

static void
right(BlitzInput *i)
{
	char *c = cursor(i);

	if(!c)
		i->curstart = i->curend = nil;
	else {
		if(c < i->text + (i->size - 1))
			i->curstart = i->curend = c + 1;
		else
			i->curstart = i->curend = nil;
	}
}

void
blitz_settext_input(BlitzInput *i, const char *text)
{
	unsigned int len = 0;

	if(!text) {
		i->size = 0;
		if(i->text)
			free(i->text);
		i->text = nil;
		return;
	}

	if(!(len = strlen(text)))
		return;

	i->size = len + 1;
	i->text = realloc(i->text, i->size);
	memcpy(i->text, text, i->size);
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

	if(!(i->drag = blitz_ispointinrect(x, y, &i->rect)))
		return False;
	/*XGrabKeyboard(i->blitz->display, i->window, True,
					GrabModeAsync, GrabModeAsync, CurrentTime);*/
	oend = i->curend;
	i->curend = charof(i, x, y);
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
	return i->curend == oend;
}

Bool
blitz_kpress_input(BlitzInput *i, unsigned long mod, KeySym k, const char *ks)
{
	if(mod & ControlMask) {
		switch (k) {
		case XK_a:
			k = XK_Begin;
			break;
		case XK_b:
			k = XK_Left;
			break;
		case XK_e:
			k = XK_End;
			break;
		case XK_h:
			k = XK_BackSpace;
			break;
		case XK_j:
			k= XK_Return;
			break;
		case XK_k:
			k = XK_Delete;
			break;
		case XK_u:
			blitz_settext_input(i, nil);
			return True;
		case XK_w:
			k = XK_BackSpace;
			break;
		default:
			return False;
		}
	}

	if(IsCursorKey(k)) {
		switch(k) {
		case XK_Home:
		case XK_Begin:
			i->curstart = i->curend = i->text;
			return True;
		case XK_Left:
			left(i);
			return True;
		case XK_Right:
			right(i);
			return True;
		case XK_End:
			i->curstart = i->curend = nil;
			return True;
		}
	}
	else {
		switch(k) {
		case XK_Tab:
		case XK_Num_Lock:
		case XK_Return:
			break;
		case XK_Delete:
		case XK_BackSpace:
			return delete(i);
		default:
			return insert(i, *ks);
		}
	}
	return False;
}
