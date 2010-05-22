/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"

int
unmapwin(Window *w) {
	assert(w->type == WWindow);
	if(w->mapped) {
		XUnmapWindow(display, w->xid);
		w->mapped = 0;
		w->unmapped++;
		return 1;
	}
	return 0;
}
