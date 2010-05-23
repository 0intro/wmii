/* Copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include <stuff/geom.h>
#include <stuff/util.h>

Rectangle
rect_intersection(Rectangle r, Rectangle r2) {
	Rectangle ret;

	/* ret != canonrect(ret) ≡ no intersection. */
	ret.min.x = max(r.min.x, r2.min.x);
	ret.max.x = min(r.max.x, r2.max.x);
	ret.min.y = max(r.min.y, r2.min.y);
	ret.max.y = min(r.max.y, r2.max.y);
	return ret;
}
