/* Copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "event.h"

void
event_configurenotify(XConfigureEvent *ev) {
	Window *w;

	if(!ev->send_event)
		event_lastconfigure = ev->serial;
	if((w = findwin(ev->window)))
		event_handle(w, config, ev);
}
