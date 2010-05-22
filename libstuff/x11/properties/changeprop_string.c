/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"

void
changeprop_string(Window *w, char *prop, char *string) {
	changeprop_char(w, prop, "UTF8_STRING", string, strlen(string));
}
