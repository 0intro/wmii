/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"

void
drawline(Image *dst, Point p1, Point p2, int cap, int w, Color col) {
	XSetLineAttributes(display, dst->gc, w, LineSolid, cap, JoinMiter);
	setgccol(dst, col);
	XDrawLine(display, dst->xid, dst->gc, p1.x, p1.y, p2.x, p2.y);
}
