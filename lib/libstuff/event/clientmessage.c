/* Copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "event.h"

void
event_clientmessage(XClientMessageEvent *ev) {
	Window *w;

	if((w = findwin(ev->window))) 
		event_handle(w, message, ev);
}
