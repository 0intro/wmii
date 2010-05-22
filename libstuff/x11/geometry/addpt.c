/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"

Point
addpt(Point p, Point q) {
	p.x += q.x;
	p.y += q.y;
	return p;
}
