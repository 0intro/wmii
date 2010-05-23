/* Copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include <stuff/geom.h>

bool
rect_contains_p(Rectangle r, Rectangle r2) {
	return r2.min.x >= r.min.x
	    && r2.max.x <= r.max.x
	    && r2.min.y >= r.min.y
	    && r2.max.y <= r.max.y;
}
