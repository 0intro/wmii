/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "x11.h"

void
sendevent(Window *w, bool propegate, long mask, void *e) {
	XSendEvent(display, w->xid, propegate, mask, e);
}

