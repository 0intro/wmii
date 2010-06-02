/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"

char*
getprop_string(Window *w, const char *name) {
	char **list, *str;
	int n;

	str = nil;

	n = getprop_textlist(w, name, &list);
	if(n > 0)
		str = estrdup(*list);
	freestringlist(list);

	return str;
}
