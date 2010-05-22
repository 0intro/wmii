/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"

Rectangle
getwinrect(Window *w) {
	XWindowAttributes wa;
	Point p;

	if(!XGetWindowAttributes(display, w->xid, &wa))
		return ZR;
	p = translate(w, &scr.root, ZP);
	return rectaddpt(Rect(0, 0, wa.width, wa.height), p);
}
