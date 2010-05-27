/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"
#include <string.h>

const WinHints ZWinHints = {
	.inc = {1, 1},
	.max = {INT_MAX, INT_MAX},
};

typedef struct GravityMap	GravityMap;

struct GravityMap {
	Point	point;
	int	gravity;
};

static GravityMap gravity_map[] = {
	{ {0, 0}, NorthWestGravity },
	{ {0, 1}, WestGravity },
	{ {0, 2}, SouthWestGravity },

	{ {1, 0}, NorthGravity },
	{ {1, 1}, CenterGravity },
	{ {1, 2}, SouthGravity },

	{ {2, 0}, NorthEastGravity },
	{ {2, 1}, EastGravity },
	{ {2, 2}, SouthEastGravity },
};

void
sethints(Window *w, WinHints *h) {
	XSizeHints xhints = { 0, };
	int i;

	/* TODO: Group hint */

	if(w->hints == nil)
		w->hints = emalloc(sizeof *h);

	*w->hints = *h;

	if(!eqpt(h->min, ZP)) {
		xhints.flags |= PMinSize;
		xhints.min_width = h->min.x;
		xhints.min_height = h->min.y;
	}
	if(!eqpt(h->max, Pt(INT_MAX, INT_MAX))) {
		xhints.flags |= PMaxSize;
		xhints.max_width = h->max.x;
		xhints.max_height = h->max.y;
	}

	if(!eqpt(h->base, ZP)) {
		xhints.flags |= PBaseSize;
		xhints.base_width  = h->baspect.x;
		xhints.base_height = h->baspect.y;
	}

	if(!eqrect(h->aspect, ZR)) {
		xhints.flags |= PAspect;

		xhints.base_width  = h->baspect.x;
		xhints.base_height = h->baspect.y;

		xhints.min_aspect.x = h->aspect.min.x;
		xhints.min_aspect.y = h->aspect.min.y;

		xhints.max_aspect.x = h->aspect.max.x;
		xhints.max_aspect.y = h->aspect.max.y;
	}

	if(!eqpt(h->inc, Pt(1, 1))) {
		xhints.flags |= PResizeInc;
		xhints.width_inc  = h->inc.x;
		xhints.height_inc = h->inc.y;
	}

	/* USPosition is probably an evil assumption, but it holds in our use cases. */
	if(h->position)
		xhints.flags |= USPosition | PPosition;

	xhints.flags |= PWinGravity;
	if(h->gravstatic)
		xhints.win_gravity = StaticGravity;
	else
		for(i=0; i < nelem(gravity_map); i++)
			if(h->grav.x == gravity_map[i].point.x &&
			   h->grav.y == gravity_map[i].point.y) {
				xhints.win_gravity = gravity_map[i].gravity;
				break;
			}

	XSetWMNormalHints(display, w->xid, &xhints);
}
