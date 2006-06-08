/* Public Domain */
#include <stdio.h>
#include <stdlib.h>

void
cext_failed_assert(char *a, char *file, int line)
{
	fprintf(stderr, "Assertion \"%s\" failed at %s:%d\n", a, file, line);
	exit(1);
}
