/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <X11/Xlib.h>

#include "cext.h"

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
	const char *err;
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
	x = cext_strtonum(argv[1], 0, DisplayWidth(dpy, DefaultScreen(dpy)), &err);
	if(err)
		usage();
	y = cext_strtonum(argv[2], 0, DisplayWidth(dpy, DefaultScreen(dpy)), &err);
	if(err)
		usage();
	XWarpPointer(dpy, None, RootWindow(dpy, DefaultScreen(dpy)), 0, 0, 0, 0, x, y);
	XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
	XCloseDisplay(dpy);
	return 0;
}
