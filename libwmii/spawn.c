/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

#include "wmii.h"

void
wmii_spawn(void *dpy, char *cmd)
{
	if(fork() == 0) {
        if(fork() == 0) {
			setsid();
			close(ConnectionNumber(dpy));
			execlp("rc", "rc", "-c", cmd, (char *) 0);
			perror("failed");
			exit(1);
        }
        exit(0);
    }
    wait(0);
}
