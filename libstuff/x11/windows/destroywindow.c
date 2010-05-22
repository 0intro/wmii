/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"

void
destroywindow(Window *w) {
	assert(w->type == WWindow);
	sethandler(w, nil);
	if(w->xft)
		XftDrawDestroy(w->xft);
	if(w->gc)
		XFreeGC(display, w->gc);
	XDestroyWindow(display, w->xid);
	free(w);
}
