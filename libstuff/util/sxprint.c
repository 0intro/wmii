/* Written by Kris Maglione <maglione.k at Gmail> */
/* Public domain */
#include "util.h"

char*
sxprint(const char *fmt, ...) {
	va_list ap;
	char *ret;

	va_start(ap, fmt);
	ret = vsxprint(fmt, ap);
	va_end(ap);
	return ret;
}
