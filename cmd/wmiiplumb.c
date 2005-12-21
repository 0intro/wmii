/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>

static char *version[] = {
    "wmiiplumb - window manager improved plumb - " VERSION "\n"
        " (C)opyright MMIV-MMV Anselm R. Garbe\n", 0
};

static void
usage()
{
    fprintf(stderr, "%s\n",
            "usage: wmiiplumb [-v]\n" "     -v      version info\n");
    exit(1);
}

static void
print_sel(Display * dpy, Window w, XSelectionEvent * e)
{
    Atom typeret;
    int format;
    unsigned long nitems, bytesleft;
    unsigned char *data;

    XGetWindowProperty(dpy, w, e->property, 0L, 4096L, False,
                       AnyPropertyType, &typeret, &format,
                       &nitems, &bytesleft, &data);
    if(format == 8) {
        int i;
        for(i = 0; i < nitems; i++)
            putchar(data[i]);
        putchar('\n');
    }
    XDeleteProperty(dpy, w, e->property);
}

int
main(int argc, char **argv)
{
    Display *dpy;
    Atom xa_clip_string;
    Window w;
    XEvent ev;
    int pdone = 0;


    /* command line args */
    if(argc > 1) {
        if(!strncmp(argv[1], "-v", 3)) {
            fprintf(stdout, "%s", version[0]);
            exit(0);
        } else
            usage();
    }
    dpy = XOpenDisplay(0);
    if(!dpy) {
        fprintf(stderr, "%s", "wmiiplumb: cannot open display\n");
        exit(1);
    }
    xa_clip_string = XInternAtom(dpy, "PLUMB_STRING", False);
    w = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy), 10, 10, 200, 200,
                            1, CopyFromParent, CopyFromParent);
    while(1 && !pdone) {
        XConvertSelection(dpy, XA_PRIMARY, XA_STRING, xa_clip_string,
                          w, CurrentTime);
        XFlush(dpy);
        XNextEvent(dpy, &ev);
        switch (ev.type) {
        case SelectionNotify:
            if(ev.xselection.property != None)
                print_sel(dpy, w, &ev.xselection);
            else
                putchar('\n');
            XDestroyWindow(dpy, w);
            XCloseDisplay(dpy);
            pdone = 1;
            break;
        default:
            break;
        }
    }
    return 0;
    /*
     * XDestroyWindow(dpy, w); XCloseDisplay(dpy); return 1;
     */
}
