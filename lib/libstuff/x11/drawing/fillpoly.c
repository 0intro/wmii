/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"

void
fillpoly(Image *dst, Point *pt, int np, Color col) {
	XPoint *xp;

	xp = convpts(pt, np);
	setgccol(dst, col);
	XFillPolygon(display, dst->xid, dst->gc, xp, np, Complex, CoordModeOrigin);
	free(xp);
}
