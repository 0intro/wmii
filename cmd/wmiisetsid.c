/*
 * (C)opyright MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int
main(int argc, char *argv[])
{
	if(argc < 2) {
		fprintf(stderr, "usage: wmiisetsid cmd [arg ...]\n", argv[0]);
		exit(1);
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

