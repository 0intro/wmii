/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"

ulong
getproperty(Window *w, char *prop, char *type, Atom *actual,
	    ulong offset, uchar **ret, ulong length) {
	int format;

	return getprop(w, prop, type, actual, &format, offset, ret, length);
}
