/* Copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include <stuff/geom.h>

Align
quadrant(Rectangle r, Point pt) {
	Align ret;

	pt = subpt(pt, r.min);
	ret = East  * (pt.x >= Dx(r) * .5)
	    | West  * (pt.x <  Dx(r) * .5)
	    | South * (pt.y >= Dy(r) * .5)
	    | North * (pt.y <  Dy(r) * .5);

	return ret;
}
