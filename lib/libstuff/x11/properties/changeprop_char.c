/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"

void
changeprop_char(Window *w, char *prop, char *type, char data[], int len) {
	changeproperty(w, prop, type, 8, (uchar*)data, len);
}
