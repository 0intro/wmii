/* Copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "event.h"

void
event_unmapnotify(XUnmapEvent *ev) {
	Window *w;

	if(!ev->send_event)
		event_lastconfigure = ev->serial;
	if((w = findwin(ev->window))) {
		if(!ev->send_event)
			w->mapped = false;
		if(!ev->send_event && ev->event == ev->window)
			w->unmapped--;
		if(ev->send_event && ev->event != ev->window)
			event_handle(w, unmap, ev);
	}
	if((w = findwin(ev->event)))
		event_handle(w, unmap, ev);
}
