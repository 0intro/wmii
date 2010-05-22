/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"

void
changeprop_ulong(Window *w, char *prop, char *type, ulong data[], int len) {
	changeproperty(w, prop, type, 32, (uchar*)data, len);
}
