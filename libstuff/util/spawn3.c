/* Copyright ©2008-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include <errno.h>
#include <unistd.h>
#include "util.h"

int
spawn3(int fd[3], const char *file, char *argv[]) {
	/* Some ideas from Russ Cox's libthread port. */
	int p[2];
	int pid;

	if(pipe(p) < 0)
		return -1;
	closeexec(p[1]);

	switch(pid = doublefork()) {
	case 0:
		dup2(fd[0], 0);
		dup2(fd[1], 1);
		dup2(fd[2], 2);

		execvp(file, argv);
		write(p[1], &errno, sizeof errno);
		exit(1);
		break;
	default:
		close(p[1]);
		if(read(p[0], &errno, sizeof errno) == sizeof errno)
			pid = -1;
		close(p[0]);
		break;
	case -1: /* can't happen */
		break;
	}

	close(fd[0]);
	/* These could fail if any of these was also a previous fd. */
	close(fd[1]);
	close(fd[2]);
	return pid;
}
