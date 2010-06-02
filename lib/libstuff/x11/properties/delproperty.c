/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"

void
delproperty(Window *w, const char *prop) {
	XDeleteProperty(display, w->xid, xatom(prop));
}
