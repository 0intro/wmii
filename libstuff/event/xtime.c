/* Copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include <stuff/x.h>

static int
findtime(Display *d, XEvent *e, XPointer v) {
	Window *w;

	w = (Window*)v;
	if(e->type == PropertyNotify && e->xproperty.window == w->xid) {
		event_xtime = e->xproperty.time;
		return true;
	}
	return false;
}

void
event_updatextime(void) {
	Window *w;
	WinAttr wa;
	XEvent e;
	long l;

	w = createwindow(&scr.root, Rect(0, 0, 1, 1), 0, InputOnly, &wa, 0);

	XSelectInput(display, w->xid, PropertyChangeMask);
	changeprop_long(w, "ATOM", "ATOM", &l, 0);
	XIfEvent(display, &e, findtime, (void*)w);

	destroywindow(w);
}

