/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"

void
fill(Image *dst, Rectangle r, Color *col) {
	setgccol(dst, col);
	XFillRectangle(display, dst->xid, dst->gc,
		r.min.x, r.min.y, Dx(r), Dy(r));
}
