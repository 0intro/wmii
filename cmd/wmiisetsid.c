/*
 * (C)opyright MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

static char version[] = "wmiisetsid - " VERSION ", (C)opyright MMVI Anselm R. Garbe\n";

static void
usage()
{
	fprintf(stderr, "%s", "usage: wmiisetsid [-v] cmd [arg ...]\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	/* command line args */
	if(argc < 2)
		usage();
	else if(!strncmp(argv[1], "-v", 3)) {
		fprintf(stdout, "%s", version);
		exit(0);
	}
	if(getpgrp() == getpid()) {
		switch(fork()){
			case -1:
				perror("wmiisetsid: fork");
				exit(1);
			case 0:
				break;
			default:
				exit(0);
		}
	}
	if(setsid() < 0) {
		perror("wmiisetsid: setsid");
		exit(1);
	}
	execvp(argv[1], argv + 1);
	perror("wmiisetsid: execvp");
	exit(1);
	return 0;
}

