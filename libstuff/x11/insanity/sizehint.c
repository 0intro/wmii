/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"

Rectangle
sizehint(WinHints *h, Rectangle r) {
	Point p, aspect, origin;

	if(h == nil)
		return r;

	origin = r.min;
	r = rectsubpt(r, origin);

	/* Min/max */
	r.max.x = max(r.max.x, h->min.x);
	r.max.y = max(r.max.y, h->min.y);
	r.max.x = min(r.max.x, h->max.x);
	r.max.y = min(r.max.y, h->max.y);

	/* Increment */
	p = subpt(r.max, h->base);
	r.max.x -= p.x % h->inc.x;
	r.max.y -= p.y % h->inc.y;

	/* Aspect */
	p = subpt(r.max, h->baspect);
	p.y = max(p.y, 1);

	aspect = h->aspect.min;
	if(p.x * aspect.y / p.y < aspect.x)
		r.max.y = h->baspect.y
			+ p.x * aspect.y / aspect.x;

	aspect = h->aspect.max;
	if(p.x * aspect.y / p.y > aspect.x)
		r.max.x = h->baspect.x
		        + p.y * aspect.x / aspect.y;

	return rectaddpt(r, origin);
}
