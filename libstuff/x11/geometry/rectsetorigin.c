/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"

Rectangle
rectsetorigin(Rectangle r, Point p) {
	Rectangle ret;

	ret.min.x = p.x;
	ret.min.y = p.y;
	ret.max.x = p.x + Dx(r);
	ret.max.y = p.y + Dy(r);
	return ret;
}
