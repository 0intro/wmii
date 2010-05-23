/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"

void
setwinattr(Window *w, WinAttr *wa, int valmask) {
	assert(w->type == WWindow);
	XChangeWindowAttributes(display, w->xid, valmask, wa);
}
