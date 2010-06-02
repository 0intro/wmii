/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"

void
changeproperty(Window *w, const char *prop, const char *type,
	       int width, const uchar data[], int n) {
	XChangeProperty(display, w->xid, xatom(prop), xatom(type), width,
			PropModeReplace, data, n);
}
