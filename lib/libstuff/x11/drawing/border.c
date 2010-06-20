/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"

void
border(Image *dst, Rectangle r, int w, Color *col) {
	if(w == 0)
		return;

	r = insetrect(r, w/2);
	r.max.x -= w%2;
	r.max.y -= w%2;

	XSetLineAttributes(display, dst->gc, w, LineSolid, CapButt, JoinMiter);
	setgccol(dst, col);
	XDrawRectangle(display, dst->xid, dst->gc,
		       r.min.x, r.min.y, Dx(r), Dy(r));
}
