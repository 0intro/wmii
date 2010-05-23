/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"

Point
translate(Window *src, Window *dst, Point sp) {
	Point pt;
	XWindow w;

	XTranslateCoordinates(display, src->xid, dst->xid, sp.x, sp.y,
			      &pt.x, &pt.y, &w);
	return pt;
}
