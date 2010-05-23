/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"

void
copyimage(Image *dst, Rectangle r, Image *src, Point p) {
	XCopyArea(display,
		  src->xid, dst->xid,
		  dst->gc,
		  r.min.x, r.min.y, Dx(r), Dy(r),
		  p.x, p.y);
}
