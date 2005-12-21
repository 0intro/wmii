/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <X11/Xutil.h>
#include <X11/Xatom.h>

static char *version[] = {
    "wmiiwarp - window manager improved warp - " VERSION "\n"
        " (C)opyright MMIV-MMV Anselm R. Garbe\n", 0
};

static void
usage()
{
    fprintf(stderr,
            "usage: wmiiwarp [-v] <x>,<y>\n"
            "      -v     version info\n");
    exit(1);
}

int
main(int argc, char **argv)
{
    Display *dpy;
    int x, y;

    /* command line args */
    if(argc != 2)
        usage();
    if(!strncmp(argv[1], "-v", 2)) {
        fprintf(stdout, "%s", version[0]);
        exit(0);
    }
    dpy = XOpenDisplay(0);
    if(!dpy) {
        fprintf(stderr, "%s", "wmiiwarp: cannot open display\n");
        exit(1);
    }
    if(!strncmp(argv[1], "center", 7)) {
        x = DisplayWidth(dpy, DefaultScreen(dpy)) / 2;
        y = DisplayHeight(dpy, DefaultScreen(dpy)) / 2;
    } else if(sscanf(argv[1], "%d,%d", &x, &y) != 2) {
        XCloseDisplay(dpy);
        usage();
    }
    XWarpPointer(dpy, None, RootWindow(dpy, DefaultScreen(dpy)),
                 0, 0, 0, 0, x, y);
    XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
    XCloseDisplay(dpy);
    return 0;
}
