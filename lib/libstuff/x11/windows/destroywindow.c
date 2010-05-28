/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"

void
cleanupwindow(Window *w) {
	assert(w->type == WWindow);
	sethandler(w, nil);
	while(w->handler_link)
		pophandler(w, w->handler_link->handler);
	free(w->hints);
	if(w->xft)
		xft->drawdestroy(w->xft);
	if(w->gc)
		XFreeGC(display, w->gc);
}

void
destroywindow(Window *w) {
	cleanupwindow(w);
	XDestroyWindow(display, w->xid);
	free(w);
}
