/* Copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "event.h"

void
event_maprequest(XMapRequestEvent *ev) {
	Window *w;

	if((w = findwin(ev->parent)))
		event_handle(w, mapreq, ev);
}
