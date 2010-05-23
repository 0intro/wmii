/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"

int
eqpt(Point p, Point q) {
	return p.x==q.x && p.y==q.y;
}
