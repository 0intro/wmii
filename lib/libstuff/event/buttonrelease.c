/* Copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "event.h"

void
event_buttonrelease(XButtonPressedEvent *ev) {
	Window *w;

	if((w = findwin(ev->window)))
		handle(w, bup, ev);
}
