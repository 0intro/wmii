/* Copyright ©2010 Kris Maglione <maglione.k at Gmail>
 * Copyright ©2002 by Lucent Technologies.
 * See LICENSE file for license details.
 */
#include "fmtdef.h"

int
localelen(char *str, char *end) {
	mbstate_t state;
	size_t n, res;

	if(utf8locale()) {
		if(end)
			return utfnlen(str, end - str);
		return utflen(str);
	}

	state = (mbstate_t){0};
	n = 0;
	for(n=0;;)
		switch((res = mbrtowc(nil, str, end ? end - str : MB_LEN_MAX, &state))) {
		case -1:
			return -1;
		case 0:
		case -2:
			return n;
		default:
			n++;
			str += res;
		}
	return n; /* Not reached. */
}

