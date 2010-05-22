/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"

void
reparentwindow(Window *w, Window *par, Point p) {
	assert(w->type == WWindow);
	XReparentWindow(display, w->xid, par->xid, p.x, p.y);
	w->parent = par;
	w->r = rectsubpt(w->r, w->r.min);
	w->r = rectaddpt(w->r, p);
}
