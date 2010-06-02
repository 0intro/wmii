/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"

ulong
getprop(Window *w, const char *prop, const char *type, Atom *actual, int *format,
	ulong offset, uchar **ret, ulong length) {
	Atom typea;
	ulong n, extra;
	int status;

	typea = (type ? xatom(type) : 0L);

	status = XGetWindowProperty(display, w->xid,
		xatom(prop), offset, length, false /* delete */,
		typea, actual, format, &n, &extra, ret);

	if(status != Success) {
		*ret = nil;
		return 0;
	}
	if(n == 0) {
		free(*ret);
		*ret = nil;
	}
	return n;
}
