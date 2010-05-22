/* Written by Kris Maglione <maglione.k at Gmail> */
/* Public domain */
#include <sys/types.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <fmt.h>
#include "util.h"

void
_die(char *file, int line, char *msg, ...) {
	va_list ap;

	va_start(ap, msg);
	fprint(2, "%s: dieing at %s:%d: %s\n",
		argv0, file, line,
		vsxprint(msg, ap));
	va_end(ap);

	kill(getpid(), SIGABRT);
	abort(); /* Adds too many frames:
		  *  _die()
		  *  abort()
		  *  raise(SIGABRT)
		  *  kill(getpid(), SIGABRT)
		  */
}
