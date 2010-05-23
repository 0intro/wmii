/* Copyright ©2008-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include <unistd.h>
#include <fcntl.h>
#include "util.h"

void
closeexec(int fd) {
	if(fcntl(fd, F_SETFD, FD_CLOEXEC) == -1)
		fatal("can't set %d close on exec: %r", fd);
}
