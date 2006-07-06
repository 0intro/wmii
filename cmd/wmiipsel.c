/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>

static char version[] = "wmiipsel - " VERSION ", (C)opyright MMIV-MMVI Anselm R. Garbe\n";

static void
usage()
{
	fprintf(stderr, "%s\n", "usage: wmiipsel [-v]\n");
	exit(1);
}

int
main(int argc, char **argv)
{
	char buf[4096];
	unsigned int i, len;

	/* command line args */
	if(argc > 1) {
		if(!strncmp(argv[1], "-v", 3)) {
			fprintf(stdout, "%s", version);
			exit(0);
		} else
			usage();
	}
	if((len = blitz_getselection(buf, sizeof(buf)))) {
		for(i = 0; i < len; i++)
			putchar(buf[i]);
		putchar('\n');
	}
	return 0;
}
