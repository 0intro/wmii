/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"

int
getprop_textlist(Window *w, char *name, char **ret[]) {
	XTextProperty prop;
	char **list;
	int n;

	*ret = nil;
	n = 0;

	XGetTextProperty(display, w->xid, &prop, xatom(name));
	if(prop.nitems > 0) {
		if(Xutf8TextPropertyToTextList(display, &prop, &list, &n) == Success)
			*ret = list;
		XFree(prop.value);
	}
	return n;
}
