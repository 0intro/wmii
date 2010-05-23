/* Copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "event.h"

void
event_enternotify(XCrossingEvent *ev) {
	Window *w;

	event_xtime = ev->time;
	if(ev->mode != NotifyNormal)
		return;

	if((w = findwin(ev->window))) 
		handle(w, enter, ev);
}
