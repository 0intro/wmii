/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"

void
setshapemask(Window *dst, Image *src, Point pt) {
	/* Assumes that we have the shape extension... */
	XShapeCombineMask (display, dst->xid,
		ShapeBounding, pt.x, pt.y, src->xid, ShapeSet);
}
