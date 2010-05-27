/* Copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "event.h"

void
event_reparentnotify(XReparentEvent *ev) {
	Window *w;

	if((w = findwin(ev->event)))
		event_handle(w, reparent, ev);
}
