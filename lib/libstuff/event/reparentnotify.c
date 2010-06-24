/* Copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "event.h"

void
event_reparentnotify(XReparentEvent *ev) {
	Window *target, *w;

	if(!ev->send_event)
		event_lastconfigure = ev->serial;
	w = nil;
	if((target = findwin(ev->window)) && (w = findwin(ev->parent)))
		target->parent = w;
	if((w = findwin(ev->event)))
		event_handle(w, reparent, ev);
	if(ev->send_event && target)
		event_handle(target, reparent, ev);
}
