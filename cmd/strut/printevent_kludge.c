#include "dat.h"

void	dprint(const char *fmt, ...);
void
dprint(const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	vfprint(2, fmt, ap);
	va_end(ap);
}

