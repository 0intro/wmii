/* Copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include <stuff/geom.h>

bool
rect_haspoint_p(Rectangle r, Point pt) {
	return (pt.x >= r.min.x) && (pt.x < r.max.x)
	    && (pt.y >= r.min.y) && (pt.y < r.max.y);
}
