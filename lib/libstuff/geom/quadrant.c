/* Copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include <stuff/geom.h>

Align
quadrant(Rectangle r, Point pt) {
	Align ret;

	pt = subpt(pt, r.min);
	ret = 0;

	if(pt.x >= Dx(r) * .5)
		ret |= East;
	if(pt.x <= Dx(r) * .5)
		ret |= West;
	if(pt.y <= Dy(r) * .5)
		ret |= North;
	if(pt.y >= Dy(r) * .5)
		ret |= South;

	return ret;
}
