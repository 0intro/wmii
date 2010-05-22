/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"

ulong
getprop_ulong(Window *w, char *prop, char *type,
	      ulong offset, ulong **ret, ulong length) {
	return getprop_long(w, prop, type, offset, (long**)ret, length);
}
