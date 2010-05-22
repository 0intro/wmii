/* Written by Kris Maglione <maglione.k at Gmail> */
/* Public domain */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "util.h"

/* Can't malloc */
void
mfatal(char *name, uint size) {
	const char
		couldnot[] = ": fatal: Could not ",
		paren[] = "() ",
		bytes[] = " bytes\n";
	char buf[1024];
	char sizestr[8];
	int i;

	i = sizeof sizestr;
	do {
		sizestr[--i] = '0' + (size%10);
		size /= 10;
	} while(size > 0);

	strlcat(buf, argv0, sizeof buf);
	strlcat(buf, couldnot, sizeof buf);
	strlcat(buf, name, sizeof buf);
	strlcat(buf, paren, sizeof buf);
	strlcat(buf, sizestr+i, sizeof buf);
	strlcat(buf, bytes, sizeof buf);
	write(2, buf, strlen(buf));

	exit(1);
}
