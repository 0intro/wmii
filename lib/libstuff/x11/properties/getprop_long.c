/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"

ulong
getprop_long(Window *w, const char *prop, const char *type,
	     ulong offset, long **ret, ulong length) {
	Atom actual;
	ulong n;
	int format;

	n = getprop(w, prop, type, &actual, &format, offset, (uchar**)ret, length);
	if(n == 0 || format == 32 && xatom(type) == actual)
		return n;
	free(*ret);
	*ret = 0;
	return 0;
}

ulong
getprop_ulong(Window *w, const char *prop, const char *type,
	      ulong offset, ulong **ret, ulong length) {
	return getprop_long(w, prop, type, offset, (long**)ret, length);
}

