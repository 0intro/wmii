/* Written by Kris Maglione <maglione.k at Gmail> */
/* Public domain */
#include <string.h>
#include "util.h"

uint
stokenize(char *res[], uint reslen, char *str, char *delim) {
	char *s;
	uint i;

	i = 0;
	s = str;
	while(i < reslen && *s) {
		while(strchr(delim, *s))
			*(s++) = '\0';
		if(*s)
			res[i++] = s;
		while(*s && !strchr(delim, *s))
			s++;
	}
	return i;
}
