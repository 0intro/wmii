/* Written by Kris Maglione <maglione.k at Gmail> */
/* Public domain */
#include <fmt.h>
#include "util.h"

char*
vsxprint(const char *fmt, va_list ap) {
	char *s;

	s = vsmprint(fmt, ap);
	freelater(s);
	return s;
}
