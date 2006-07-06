/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <blitz.h>

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
	unsigned char *data;
	unsigned long i, offset, len, remain;

	/* command line args */
	if(argc > 1) {
		if(!strncmp(argv[1], "-v", 3)) {
			fprintf(stdout, "%s", version);
			exit(0);
		} else
			usage();
	}
	len = offset = 0;
	do {
		data = blitz_getselection(offset, &len, &remain);
		for(i = 0; i < len; i++)
			putchar(data[i]);
		offset += len;
		free(data);
	}
	while(remain);
	putchar('\n');
	return 0;
}
