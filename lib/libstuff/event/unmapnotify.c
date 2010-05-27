/* Copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "event.h"

void
event_unmapnotify(XUnmapEvent *ev) {
	Window *w;

	if((w = findwin(ev->window)) && (ev->event == w->parent->xid)) {
		w->mapped = false;
		if(w->parent && (ev->send_event || w->unmapped-- == 0))
			event_handle(w, unmap, ev);
	}
}
