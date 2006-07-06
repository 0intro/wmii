/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <string.h>
#include <cext.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include "blitz.h"

unsigned char *
blitz_getselection(unsigned long offset, unsigned long *len, unsigned long *remain)
{
	Display *dpy;
	Atom xa_clip_string;
	Window w;
	XEvent ev;
	Atom typeret;
	int format;
	unsigned char *data;
	unsigned char *result = nil;

	dpy = XOpenDisplay(nil);
	if(!dpy)
		return nil;
	xa_clip_string = XInternAtom(dpy, "BLITZ_SEL_STRING", False);
	w = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy), 10, 10, 200, 200,
			1, CopyFromParent, CopyFromParent);
	XConvertSelection(dpy, XA_PRIMARY, XA_STRING, xa_clip_string,
			w, CurrentTime);
	XFlush(dpy);
	XNextEvent(dpy, &ev);
	if(ev.type == SelectionNotify && ev.xselection.property != None) {
		XGetWindowProperty(dpy, w, ev.xselection.property, offset, 4096L, False,
				AnyPropertyType, &typeret, &format, len, remain, &data);
		if(*len) {
			result = cext_emallocz(sizeof(unsigned char) * *len);
			memcpy(result, data, *len);
			result[*len - 1] = 0;
		}
		XDeleteProperty(dpy, w, ev.xselection.property);
	}
	XDestroyWindow(dpy, w);
	XCloseDisplay(dpy);
	return result;
}
