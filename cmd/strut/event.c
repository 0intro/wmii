/* Copyright ©2006-2008 Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */
#include "dat.h"
#include "fns.h"

static void (*handler[LASTEvent])(XEvent*);

void
dispatch_event(XEvent *e) {
	/* printevent(e); */
	if(e->type < nelem(handler) && handler[e->type])
		handler[e->type](e);
}

#define handle(w, fn, ev) \
	BLOCK(if((w)->handler->fn) (w)->handler->fn((w), ev))

#ifdef notdef
uint
flushevents(long event_mask, bool dispatch) {
	XEvent ev;
	uint n = 0;

	while(XCheckMaskEvent(display, event_mask, &ev)) {
		if(dispatch)
			dispatch_event(&ev);
		n++;
	}
	return n;
}

static int
findenter(Display *d, XEvent *e, XPointer v) {
	long *l;

	USED(d);
	l = (long*)v;
	if(*l)
		return false;
	if(e->type == EnterNotify)
		return true;
	if(e->type == MotionNotify)
		(*l)++;
	return false;
}

/* This isn't perfect. If there were motion events in the queue
 * before this was called, then it flushes nothing. If we don't
 * check for them, we might lose a legitamate enter event.
 */
uint
flushenterevents(void) {
	XEvent e;
	long l;
	int n;

	l = 0;
	n = 0;
	while(XCheckIfEvent(display, &e, findenter, (void*)&l))
		n++;
	return n;
}
#endif

static int
findtime(Display *d, XEvent *e, XPointer v) {
	Window *w;

	w = (Window*)v;
	if(e->type == PropertyNotify && e->xproperty.window == w->w) {
		xtime = e->xproperty.time;
		return true;
	}
	return false;
}

void
xtime_kludge(void) {
	Window *w;
	WinAttr wa;
	XEvent e;
	long l;

	w = createwindow(&scr.root, Rect(0, 0, 1, 1), 0, InputOnly, &wa, 0);

	XSelectInput(display, w->w, PropertyChangeMask);
	changeprop_long(w, "ATOM", "ATOM", &l, 0);
	XIfEvent(display, &e, findtime, (void*)w);

	destroywindow(w);
}

static void
buttonrelease(XEvent *e) {
	XButtonPressedEvent *ev;
	Window *w;

	ev = &e->xbutton;
	if((w = findwin(ev->window)))
		handle(w, bup, ev);
}

static void
buttonpress(XEvent *e) {
	XButtonPressedEvent *ev;
	Window *w;

	ev = &e->xbutton;
	if((w = findwin(ev->window)))
		handle(w, bdown, ev);
	else
		XAllowEvents(display, ReplayPointer, ev->time);
}

static void
clientmessage(XEvent *e) {
	XClientMessageEvent *ev;

	ev = &e->xclient;
	USED(ev);
}

static void
configurenotify(XEvent *e) {
	XConfigureEvent *ev;
	Window *w;

	ev = &e->xconfigure;
	if((w = findwin(ev->window)))
		handle(w, config, ev);
}

static void
destroynotify(XEvent *e) {
	XDestroyWindowEvent *ev;
	Window *w;

	ev = &e->xdestroywindow;
	if((w = findwin(ev->window))) 
		handle(w, destroy, ev);
}

static void
enternotify(XEvent *e) {
	XCrossingEvent *ev;
	Window *w;

	ev = &e->xcrossing;
	xtime = ev->time;
	if(ev->mode != NotifyNormal)
		return;

	if((w = findwin(ev->window))) 
		handle(w, enter, ev);
}

static void
leavenotify(XEvent *e) {
	XCrossingEvent *ev;

	ev = &e->xcrossing;
	xtime = ev->time;
}

static void
focusin(XEvent *e) {
	XFocusChangeEvent *ev;
	Window *w;

	ev = &e->xfocus;
	/* Yes, we're focusing in on nothing, here. */
	if(ev->detail == NotifyDetailNone) {
		/* FIXME: Do something. */
		return;
	}

	if(!((ev->detail == NotifyNonlinear)
	   ||(ev->detail == NotifyNonlinearVirtual)
	   ||(ev->detail == NotifyVirtual)
	   ||(ev->detail == NotifyInferior)
	   ||(ev->detail == NotifyAncestor)))
		return;
	if((ev->mode == NotifyWhileGrabbed))
		return;

	if((w = findwin(ev->window))) 
		handle(w, focusin, ev);
}

static void
focusout(XEvent *e) {
	XFocusChangeEvent *ev;
	Window *w;

	ev = &e->xfocus;
	if(!((ev->detail == NotifyNonlinear)
	   ||(ev->detail == NotifyNonlinearVirtual)
	   ||(ev->detail == NotifyVirtual)
	   ||(ev->detail == NotifyInferior)
	   ||(ev->detail == NotifyAncestor)))
		return;

	if((w = findwin(ev->window))) 
		handle(w, focusout, ev);
}

static void
expose(XEvent *e) {
	XExposeEvent *ev;
	Window *w;

	ev = &e->xexpose;
	if(ev->count == 0) {
		if((w = findwin(ev->window))) 
			handle(w, expose, ev);
	}
}

static void
keypress(XEvent *e) {
	XKeyEvent *ev;

	ev = &e->xkey;
	xtime = ev->time;
}

static void
mappingnotify(XEvent *e) {
	XMappingEvent *ev;

	ev = &e->xmapping;
	/* Why do you need me to tell you this? */
	XRefreshKeyboardMapping(ev);
}

static void
motionnotify(XEvent *e) {
	XMotionEvent *ev;
	Window *w;

	ev = &e->xmotion;
	xtime = ev->time;
	if((w = findwin(ev->window)))
		handle(w, motion, ev);
}

static void
propertynotify(XEvent *e) {
	XPropertyEvent *ev;
	Window *w;

	ev = &e->xproperty;
	xtime = ev->time;
	if((w = findwin(ev->window))) 
		handle(w, property, ev);
}

static void
mapnotify(XEvent *e) {
	XMapEvent *ev;
	Window *w;

	ev = &e->xmap;
	if((w = findwin(ev->window))) 
		handle(w, map, ev);
}

static void
unmapnotify(XEvent *e) {
	XUnmapEvent *ev;
	Window *w;

	ev = &e->xunmap;
	if((w = findwin(ev->window)) && w->parent && (ev->event == w->parent->w)) {
		if(ev->send_event || w->unmapped-- == 0)
			handle(w, unmap, ev);
	}
}

static void (*handler[LASTEvent])(XEvent*) = {
	[ButtonPress] = buttonpress,
	[ButtonRelease] = buttonrelease,
	[ClientMessage] = clientmessage,
	[ConfigureNotify] = configurenotify,
	[DestroyNotify] = destroynotify,
	[EnterNotify] = enternotify,
	[Expose] = expose,
	[FocusIn] = focusin,
	[FocusOut] = focusout,
	[KeyPress] = keypress,
	[LeaveNotify] = leavenotify,
	[MapNotify] = mapnotify,
	[MappingNotify] = mappingnotify,
	[MotionNotify] = motionnotify,
	[PropertyNotify] = propertynotify,
	[UnmapNotify] = unmapnotify,
};

void
xevent_loop(void) {
	XEvent ev;

	while(running) {
		XNextEvent(display, &ev);
		dispatch_event(&ev);
	}
}

