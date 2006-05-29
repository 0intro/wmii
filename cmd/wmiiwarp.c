/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <X11/Xlib.h>

#include <cext.h>

static char version[] = "wmiiwarp - " VERSION ", (C)opyright MMIV-MMV Anselm R. Garbe\n";

static void
usage()
{
	fprintf(stderr, "%s", "usage: wmiiwarp <x> <y> [-v]\n");
	exit(1);
}

int
main(int argc, char **argv)
{
	Display *dpy;
	int x, y;

    /* command line args */
	if((argc == 2) && !strncmp(argv[1], "-v", 2)) {
		fprintf(stdout, "%s", version);
		exit(0);
	}
	else if(argc != 3)
		usage();
	if(!(dpy = XOpenDisplay(0))) {
		fprintf(stderr, "%s", "wmiiwarp: cannot open display\n");
		exit(1);
	}
	if(sscanf(argv[1], "%d", &x) != 1)
		usage();
	if(sscanf(argv[1], "%d", &y) != 1)
		usage();
	XWarpPointer(dpy, None, RootWindow(dpy, DefaultScreen(dpy)), 0, 0, 0, 0, x, y);
	XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
	XCloseDisplay(dpy);
	return 0;
}
