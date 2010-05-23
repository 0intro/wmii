/* Written by Kris Maglione <maglione.k at Gmail> */
/* Public domain */
#include "util.h"

uint
strlcat(char *dst, const char *src, uint size) {
	const char *s;
	char *d;
	int n, len;

	d = dst;
	s = src;
	n = size;
	while(n-- > 0 && *d != '\0')
		d++;
	len = n;

	while(*s != '\0') {
		if(n-- > 0)
			*d++ = *s;
		s++;
	}
	if(len > 0)
		*d = '\0';
	return size - n - 1;
}
