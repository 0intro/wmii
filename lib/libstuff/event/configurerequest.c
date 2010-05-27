/* Copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "event.h"

void
event_configurerequest(XConfigureRequestEvent *ev) {
	XWindowChanges wc;
	Window *w;

	if((w = findwin(ev->window)))
		event_handle(w, configreq, ev);
	else{
		wc.x = ev->x;
		wc.y = ev->y;
		wc.width = ev->width;
		wc.height = ev->height;
		wc.border_width = ev->border_width;
		wc.sibling = ev->above;
		wc.stack_mode = ev->detail;
		XConfigureWindow(display, ev->window, ev->value_mask, &wc);
	}
}

