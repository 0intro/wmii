/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <string.h>
#include <cext.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>

#include "blitz.h"

void
blitz_selrequest(Blitz *blitz, XSelectionRequestEvent *rq, char *text)
{
	/* 
	 * PRECONDITION:
	 * 1. XSetSelectionOwner(blitz->dpy, XA_PRIMARY, w, CurrentTime);
	 * 2. Received SelectionRequest rq
	 */
	XEvent ev;
	Atom target;
	XTextProperty ct;
	XICCEncodingStyle style;
	char *cl[1];

	ev.xselection.type = SelectionNotify;
	ev.xselection.property = None;
	ev.xselection.display = rq->display;
	ev.xselection.requestor = rq->requestor;
	ev.xselection.selection = rq->selection;
	ev.xselection.target = rq->target;
	ev.xselection.time = rq->time;

	if (rq->target == XA_STRING) {
		style = XStringStyle;
		target = XA_STRING;
	}
	cl[0] = text;
	XmbTextListToTextProperty(blitz->dpy, cl, 1, style, &ct);
	XChangeProperty(blitz->dpy, rq->requestor, rq->property,
			target, 8, PropModeReplace, ct.value, ct.nitems);
	ev.xselection.property = rq->property;
	XSendEvent(blitz->dpy, rq->requestor, False, 0, &ev);
}

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
		}
		XDeleteProperty(dpy, w, ev.xselection.property);
	}
	XDestroyWindow(dpy, w);
	XCloseDisplay(dpy);
	return result;
}
