/* (C)opyright MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */
#include "wm.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

/* extern */

void *
emallocz(unsigned int size) {
	void *res = calloc(1, size);

	if(!res)
		eprint("fatal: could not malloc() %u bytes\n", size);
	return res;
}

void
eprint(const char *errstr, ...) {
	va_list ap;

	va_start(ap, errstr);
	vfprintf(stderr, errstr, ap);
	va_end(ap);
	exit(EXIT_FAILURE);
}

void *
erealloc(void *ptr, unsigned int size) {
	void *res = realloc(ptr, size);

	if(!res)
		eprint("fatal: could not malloc() %u bytes\n", size);
	return res;
}
