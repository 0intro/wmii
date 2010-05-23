/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"

void
configwin(Window *w, Rectangle r, int border) {
	XWindowChanges wc;

	if(eqrect(r, w->r) && border == w->border)
		return;

	wc.x = r.min.x - border;
	wc.y = r.min.y - border;
	wc.width = Dx(r);
	wc.height = Dy(r);
	wc.border_width = border;
	XConfigureWindow(display, w->xid, CWX|CWY|CWWidth|CWHeight|CWBorderWidth, &wc);

	w->r = r;
	w->border = border;
}
