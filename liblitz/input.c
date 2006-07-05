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
xchangegc(BlitzInput *i, BlitzColor c)
{
	XGCValues gcv;

	gcv.foreground = c.fg;
	gcv.background = c.bg;
	if(i->font->set)
		XChangeGC(i->blitz->dpy, i->gc, GCForeground | GCBackground, &gcv);
	else {
		gcv.font = i->font->xfont->fid;
		XChangeGC(i->blitz->dpy, i->gc, GCForeground | GCBackground | GCFont, &gcv);
	}
}

static void
xdrawtextpart(BlitzInput *i, char *start, char *end,
				unsigned int *xoff, unsigned int yoff)
{
	unsigned int len;

	if(!start || !(len = end - start))
		return;

	if(i->font->set)
		XmbDrawImageString(i->blitz->dpy, i->drawable, i->font->set, i->gc,
				*xoff, yoff, start, len);
	else
		XDrawImageString(i->blitz->dpy, i->drawable, i->gc, *xoff, yoff,
				start, len);

	*xoff += blitz_textwidth_l(i->font, start, len);
}

void
blitz_setinput(BlitzInput *i, char *text)
{
	if(!text) {
		if(i->size) {
			i->len = 0;
			i->text[i->len] = 0;
			i->curstart = i->curend = i->text;
		}
		return;
	}

	i->len = strlen(text);
	if(i->len + 1 > i->size) {
		i->size = 2 * i->len + 1;
		i->text = cext_erealloc(i->text, i->size);
	}
	memcpy(i->text, text, i->len);
	i->text[i->len] = 0;
	i->curstart = i->curend = i->text + i->len;
}

static char *
curend(BlitzInput *i)
{
	if(i->curstart < i->curend)
		return i->curend;
	else
		return i->curstart;
}

static char *
curstart(BlitzInput *i)
{
	if(i->curstart < i->curend)
		return  i->curstart;
	else
		return i->curend;
}

void
blitz_draw_input(BlitzInput *i)
{
	char *start, *end;
	unsigned int xoff, yoff, xcursor, h;

	if (!i)
		return;

	blitz_drawbg(i->blitz->dpy, i->drawable, i->gc, i->rect, i->color, True);

	h = (i->font->ascent + i->font->descent);
	yoff = i->rect.y + (i->rect.height - h) / 2 + i->font->ascent;
	xcursor = xoff = i->rect.x + i->rect.height / 2;

	start = curstart(i);
	end = curend(i);

	/* draw normal text */
	xchangegc(i, i->color);
	xdrawtextpart(i, i->text, start, &xoff, yoff);
	xcursor = xoff;
	/* draw sel text */
	xchangegc(i, i->bcolor[i->button]);
	xdrawtextpart(i, start, end, &xoff, yoff);
	/* draw remaining normal text */
	xchangegc(i, i->color);
	xdrawtextpart(i, end, i->text + i->len, &xoff, yoff);

	/* draw cursor */
	if(!start && !end)
		xcursor = xoff;
	if(start == end)
		blitz_drawcursor(i->blitz->dpy, i->drawable, i->gc,
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
	if(!i->text || !blitz_ispointinrect(x, y, &i->rect))
		return nil;

	/* normalize and check x */
	if((x -= (i->rect.x + i->rect.height / 2)) < 0)
		return i->text;
	else if(x > blitz_textwidth_l(i->font, i->text, i->len))
		return i->text + i->len;

	return xcharof(i, x, i->text, i->len);
}

Bool
blitz_bpress_input(BlitzInput *i, int button, int x, int y)
{
	char *ostart, *oend;

	if(!(i->drag = blitz_ispointinrect(x, y, &i->rect)))
		return False;
	XSetInputFocus(i->blitz->dpy, i->win,
			RevertToPointerRoot, CurrentTime);
	ostart = i->curstart;
	oend = i->curend;
	i->curstart = i->curend = charof(i, x, y);
	if((i->button = button - Button1) > 2)
		i->button = 0;
	return (i->curstart == ostart) && (i->curend == oend);
}

static void
mark(BlitzInput *i, int x, int y)
{
	char *start, *end;

	start = curstart(i);
	end = curend(i);

	if(!start)
		return;

	if(start != end) {
		i->curstart = i->curend = charof(i, x, y);
		return;
	}

	if(start == i->text + i->len) {
		/* mark everything */
		i->curstart = i->text;
		i->curend = i->text + i->len;
		return;
	}

	if(*start == ' ' && start > i->text && *(start - 1) != ' ')
		start = --end;

	while(start > i->text && *(start - 1) != ' ')
		start--;
	while(*end && *end != ' ')
		end++;

	i->curstart = start;
	i->curend = end;
}

Bool
blitz_brelease_input(BlitzInput *i, int button, int x, int y, unsigned long time)
{
	char *oend;

	if(!(i->drag = blitz_ispointinrect(x, y, &i->rect)))
		return False;
	XSetInputFocus(i->blitz->dpy, i->win,
			RevertToPointerRoot, CurrentTime);
	oend = i->curend;

	if((i->button = button - Button1) > 2)
		i->button = 0;

	if(!i->button && (time - i->tdbclk < 1000)
			&& (x == i->xdbclk && y == i->ydbclk))
	{
		mark(i, x, y);
		i->drag = False;
		i->tdbclk = 0;
		i->xdbclk = i->ydbclk = 0;
		return True;
	}

	i->curend = charof(i, x, y);
	if(i->button)
		i->curstart = i->curend;
	i->tdbclk = time;
	i->xdbclk = x;
	i->ydbclk = y;
	i->drag = False;

	return i->curend == oend;
}

Bool
blitz_bmotion_input(BlitzInput *i, int x, int y)
{
	char *oend;
	Bool focus;

	focus = blitz_ispointinrect(x, y, &i->rect);

	if(focus)
		XSetInputFocus(i->blitz->dpy, i->win,
				RevertToPointerRoot, CurrentTime);
	else
		return False;

	if(!i->drag)
		return False;
	oend = i->curend;
	i->curend = charof(i, x, y);
	return i->curend == oend;
}

Bool
blitz_kpress_input(BlitzInput *i, unsigned long mod, KeySym k, char *ks)
{
	char *start, *end;
	unsigned int len;
	int s, e;

	start = curstart(i);
	end = curend(i);
	if(mod & ControlMask) {
		switch (k) {
		case XK_A:
		case XK_a:
			k = XK_Begin;
			break;
		case XK_E:
		case XK_e:
			k = XK_End;
			break;
		case XK_H:
		case XK_h:
			k = XK_BackSpace;
			break;
		case XK_U:
		case XK_u:
			k = XK_BackSpace;
			start = i->text;
			break;
		case XK_W:
		case XK_w:
			k = XK_BackSpace;
			while(start > i->text && (*(--start) == ' '));
			while(start > i->text && (*(start - 1) != ' '))
				--start;
			break;
		default: /* ignore other control sequences */
			return False;
		}
	}

	if(IsCursorKey(k)) {
		switch(k) {
		case XK_Begin:
			i->curstart = i->curend = i->text;
			return True;
		case XK_End:
			i->curstart = i->curend = i->text + i->len;
			return True;
		case XK_Left:
			if(start != end)
				i->curstart = i->curend = start;
			else if(start > i->text)
				i->curstart = i->curend = --start;
			else
				i->curstart = i->curend = i->text;
			return True;
		case XK_Right:
			if(start != end)
				i->curstart = i->curend = end;
			else if(start < i->text + i->len)
				i->curstart = i->curend = ++start;
			else
				i->curstart = i->curend = i->text + i->len;
			return True;
		}
	}
	else {
		switch(k) {
		case XK_BackSpace:
			if(!start)
				return False;
			else if((start == end) && (start != i->text)) {
				i->curstart = i->curend = --start;
				memmove(start, start + 1, strlen(start + 1));
				i->len--;
			}
			else {
				i->curstart = i->curend = start;
				memmove(start, end, strlen(end));
				i->len -= (end - start);
			}
			i->text[i->len] = 0;
			return True;
		default:
			if(!(len = strlen(ks)))
				return False;
			if(!start) {
				blitz_setinput(i, ks);
				return True;
			}
			i->len = i->len - (end - start) + len;
			if(i->len + 1 > i->size) {
				s = start - i->text;
				e = end - i->text;
				i->size = 2 * i->len + 1;
				i->text = cext_erealloc(i->text, i->size);
				start = i->text + s;
				end = i->text + e;
			}
			memmove(start + len, end, strlen(end));
			memcpy(start, ks, len);
			i->curstart = i->curend = start + len;
			i->text[i->len] = 0;
			return True;
		}
	}
	return False;
}
