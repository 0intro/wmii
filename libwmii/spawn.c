/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "wmii.h"

void
wmii_spawn(void *dpy, char *cmd)
{
	pid_t pid = fork();

	switch (pid)
	{
		case 0:
			setsid();
			close(ConnectionNumber(dpy));
			execlp("rc", "rc", "-c", cmd, (char *) 0);
			exit(1);
			perror("failed");
			exit(1);
		case -1:
			perror("can't fork");
	}
}
