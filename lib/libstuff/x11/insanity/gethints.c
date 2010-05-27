/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include <string.h>
#include "../x11.h"

void
gethints(Window *w) {
	XSizeHints xs;
	XWMHints *wmh;
	WinHints *h;
	Point p;
	long size;

	if(w->hints == nil)
		w->hints = emalloc(sizeof *h);

	h = w->hints;
	*h = ZWinHints;

	wmh = XGetWMHints(display, w->xid);
	if(wmh) {
		if(wmh->flags & WindowGroupHint)
			h->group = wmh->window_group;
		free(wmh);
	}

	if(!XGetWMNormalHints(display, w->xid, &xs, &size))
		return;

	if(xs.flags & PMinSize) {
		h->min.x = xs.min_width;
		h->min.y = xs.min_height;
	}
	if(xs.flags & PMaxSize) {
		h->max.x = xs.max_width;
		h->max.y = xs.max_height;
	}

	/* Goddamn buggy clients. */
	if(h->max.x < h->min.x)
		h->max.x = h->min.x;
	if(h->max.y < h->min.y)
		h->max.y = h->min.y;

	h->base = h->min;
	if(xs.flags & PBaseSize) {
		p.x = xs.base_width;
		p.y = xs.base_height;
		h->base = p;
		h->baspect = p;
	}

	if(xs.flags & PResizeInc) {
		h->inc.x = max(xs.width_inc, 1);
		h->inc.y = max(xs.height_inc, 1);
	}

	if(xs.flags & PAspect) {
		h->aspect.min.x = xs.min_aspect.x;
		h->aspect.min.y = xs.min_aspect.y;
		h->aspect.max.x = xs.max_aspect.x;
		h->aspect.max.y = xs.max_aspect.y;
	}

	h->position = (xs.flags & (USPosition|PPosition)) != 0;

	if(!(xs.flags & PWinGravity))
		xs.win_gravity = NorthWestGravity;
	p = ZP;
	switch (xs.win_gravity) {
	case EastGravity:
	case CenterGravity:
	case WestGravity:
		p.y = 1;
		break;
	case SouthEastGravity:
	case SouthGravity:
	case SouthWestGravity:
		p.y = 2;
		break;
	}
	switch (xs.win_gravity) {
	case NorthGravity:
	case CenterGravity:
	case SouthGravity:
		p.x = 1;
		break;
	case NorthEastGravity:
	case EastGravity:
	case SouthEastGravity:
		p.x = 2;
		break;
	}
	h->grav = p;
	h->gravstatic = (xs.win_gravity == StaticGravity);
}
