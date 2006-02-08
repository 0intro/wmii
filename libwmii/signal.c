/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "wmii.h"

static void (*sigchld)() = 0;

static void
sig_handler(int signal)
{
	switch (signal)
	{
		case SIGINT:
		case SIGTERM:
		case SIGHUP:
			if(sigchld)
				sigchld();
			break;
		case SIGCHLD:
			wait(0);
			break;
	}
}

void
wmii_signal(void (*sigchld_handler)())
{
	sigchld = sigchld_handler;
	signal(SIGTERM, sig_handler);
	signal(SIGINT, sig_handler);
	signal(SIGHUP, sig_handler);
	signal(SIGCHLD, sig_handler);
}
