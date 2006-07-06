/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <cext.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include "blitz.h"

unsigned int
blitz_getselection(char *buf, unsigned int len)
{
	Display *dpy;
	Atom xa_clip_string;
	Window w;
	XEvent ev;
	Atom typeret;
	int format;
	unsigned long nitems, bytesleft;
	unsigned char *data;
	unsigned int ret;

	ret = 0;
	if(!buf || !len)
		return ret;
	buf[0] = 0;
	dpy = XOpenDisplay(nil);
	if(!dpy)
		return ret;
	xa_clip_string = XInternAtom(dpy, "BLITZ_SEL_STRING", False);
	w = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy), 10, 10, 200, 200,
			1, CopyFromParent, CopyFromParent);
	XConvertSelection(dpy, XA_PRIMARY, XA_STRING, xa_clip_string,
			w, CurrentTime);
	XFlush(dpy);
	XNextEvent(dpy, &ev);
	if(ev.type == SelectionNotify && ev.xselection.property != None) {
		XGetWindowProperty(dpy, w, ev.xselection.property, 0L, len, False,
				AnyPropertyType, &typeret, &format, &nitems, &bytesleft, &data);
		if(format == 8)
			cext_strlcpy(buf, (const char *)data, len);
		ret = nitems < len ? nitems : len - 1;
		buf[ret] = 0;
		XDeleteProperty(dpy, w, ev.xselection.property);
	}
	XDestroyWindow(dpy, w);
	XCloseDisplay(dpy);
	return ret;
}
