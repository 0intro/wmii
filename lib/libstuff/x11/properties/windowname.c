/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"

char*
windowname(Window *w) {
	char *str;

	str = getprop_string(w, "_NET_WM_NAME");
	if(str == nil)
		str = getprop_string(w, "WM_NAME");
	if(str == nil)
		str = estrdup("");
	return str;
}

