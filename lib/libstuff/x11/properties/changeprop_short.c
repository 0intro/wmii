/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"

void
changeprop_short(Window *w, char *prop, char *type, short data[], int len) {
	changeproperty(w, prop, type, 16, (uchar*)data, len);
}
