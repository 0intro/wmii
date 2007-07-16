#include	"plan9.h"
#include	<bio.h>

int
Bprint(Biobuf *bp, const char *fmt, ...)
{
	int n;
	va_list arg;

	va_start(arg, fmt);
	n = Bvprint(bp, fmt, arg);
	va_end(arg);
	return n;
}
