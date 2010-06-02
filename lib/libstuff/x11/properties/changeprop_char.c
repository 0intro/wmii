/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"

void
changeprop_char(Window *w, const char *prop, const char *type, const char data[], int len) {
	changeproperty(w, prop, type, 8, (const uchar*)data, len);
}
