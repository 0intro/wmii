/* Copyright ©2008-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include "util.h"

int
doublefork(void) {
	pid_t pid;
	int status;

	switch(pid=fork()) {
	case -1:
		fatal("Can't fork(): %r");
	case 0:
		switch(pid=fork()) {
		case -1:
			fatal("Can't fork(): %r");
		case 0:
			return 0;
		default:
			exit(0);
		}
	default:
		waitpid(pid, &status, 0);
		return pid;
	}
	/* NOTREACHED */
}
