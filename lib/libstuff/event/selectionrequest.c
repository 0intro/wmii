/* Copyright ©2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "event.h"

void
event_selectionrequest(XSelectionRequestEvent *ev) {
	Window *w;

	if(!ev->send_event)
		event_xtime = ev->time;
	if((w = findwin(ev->owner))) 
		event_handle(w, selectionrequest, ev);
}

