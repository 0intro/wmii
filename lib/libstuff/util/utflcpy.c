/* Written by Kris Maglione <maglione.k at Gmail> */
/* Public domain */
#include "util.h"

int
utflcpy(char *to, const char *from, int l) {
	char *p;

	p = utfecpy(to, to+l, from);
	return p-to;
}
