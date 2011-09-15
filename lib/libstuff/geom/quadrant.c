/* Copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include <stuff/geom.h>

Align
quadrant(Rectangle r, Point pt) {
	Align ret;

	pt = subpt(pt, r.min);
	ret = East  * (pt.x >= Dx(r) * .7)
	    | West  * (pt.x <= Dx(r) * .3)
	    | South * (pt.y >= Dy(r) * .7)
	    | North * (pt.y <= Dy(r) * .3);

	return ret;
}
