/* Copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "event.h"

void
event_focusin(XFocusChangeEvent *ev) {
	Window *w;

	/* Yes, we're focusing in on nothing, here. */
	if(ev->detail == NotifyDetailNone) {
		/* FIXME: Do something. */
		return;
	}

	if(!((ev->detail == NotifyNonlinear)
	   ||(ev->detail == NotifyNonlinearVirtual)
	   ||(ev->detail == NotifyVirtual)
	   ||(ev->detail == NotifyInferior)
	   ||(ev->detail == NotifyAncestor)))
		return;
	if((ev->mode == NotifyWhileGrabbed))
		return;

	if((w = findwin(ev->window))) 
		event_handle(w, focusin, ev);
}
