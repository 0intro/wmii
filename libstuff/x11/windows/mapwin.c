/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"

int
mapwin(Window *w) {
	assert(w->type == WWindow);
	if(!w->mapped) {
		XMapWindow(display, w->xid);
		w->mapped = 1;
		return 1;
	}
	return 0;
}
