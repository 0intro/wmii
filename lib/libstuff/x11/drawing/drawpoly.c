/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"

void
drawpoly(Image *dst, Point *pt, int np, int cap, int w, Color *col) {
	XPoint *xp;

	xp = convpts(pt, np);
	XSetLineAttributes(display, dst->gc, w, LineSolid, cap, JoinMiter);
	setgccol(dst, col);
	XDrawLines(display, dst->xid, dst->gc, xp, np, CoordModeOrigin);
	free(xp);
}
