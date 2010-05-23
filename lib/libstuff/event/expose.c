/* Copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "event.h"

void
event_expose(XExposeEvent *ev) {
	Window *w;

	if(ev->count == 0 && (w = findwin(ev->window))) 
		handle(w, expose, ev);
}
