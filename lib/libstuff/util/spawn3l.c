/* Copyright ©2008-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include <fmt.h>
#include "util.h"

int
spawn3l(int fd[3], const char *file, ...) {
	va_list ap;
	char **argv;
	int i, n;

	va_start(ap, file);
	for(n=0; va_arg(ap, char*); n++)
		;
	va_end(ap);

	argv = emalloc((n+1) * sizeof *argv);
	va_start(ap, file);
	quotefmtinstall();
	for(i=0; i <= n; i++)
		argv[i] = va_arg(ap, char*);
	va_end(ap);

	i = spawn3(fd, file, argv);
	free(argv);
	return i;
}
