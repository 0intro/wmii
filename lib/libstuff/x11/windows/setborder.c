/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"

void
setborder(Window *w, int width, Color *col) {

	assert(w->type == WWindow);
	if(width)
		XSetWindowBorder(display, w->xid, pixelvalue(w, col));
	if(width != w->border)
		configwin(w, w->r, width);
}
