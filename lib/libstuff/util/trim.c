/* Written by Kris Maglione <maglione.k at Gmail> */
/* Public domain */
#include "util.h"

void
trim(char *str, const char *chars) {
	const char *r;
	char *p, *q;

	for(p=str, q=str; *p; p++) {
		for(r=chars; *r; r++)
			if(*p == *r)
				break;
		if(!*r)
			*q++ = *p;
	}
	*q = '\0';
}

