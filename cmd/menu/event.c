/* Copyright ©2006-2008 Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */
#include "dat.h"
#include "fns.h"

typedef void (*EvHandler)(XEvent*);
static EvHandler handler[LASTEvent];

void
dispatch_event(XEvent *e) {
	if(e->type < nelem(handler)) {
		if(handler[e->type])
			handler[e->type](e);
	}else
		xext_event(e);
}

#define handle(w, fn, ev) \
	BLOCK(if((w)->handler->fn) (w)->handler->fn((w), ev))

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
	/* Round trip. */
        static Window *w;
        WinAttr wa;
        XEvent e;
        long l;

	if(w == nil) {
		w = createwindow(&scr.root, Rect(0, 0, 1, 1), 0, InputOnly, &wa, 0);
		selectinput(w, PropertyChangeMask);
	}
        changeprop_long(w, "ATOM", "ATOM", &l, 0);
	sync();
        XIfEvent(display, &e, findtime, (void*)w);
}

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

static void
buttonrelease(XButtonPressedEvent *ev) {
	Window *w;

	if((w = findwin(ev->window)))
		handle(w, bup, ev);
}

static void
buttonpress(XButtonPressedEvent *ev) {
	Window *w;

	if((w = findwin(ev->window)))
		handle(w, bdown, ev);
	else
		XAllowEvents(display, ReplayPointer, ev->time);
}

static void
configurerequest(XConfigureRequestEvent *ev) {
	XWindowChanges wc;
	Window *w;

	if((w = findwin(ev->window)))
		handle(w, configreq, ev);
	else{
		wc.x = ev->x;
		wc.y = ev->y;
		wc.width = ev->width;
		wc.height = ev->height;
		wc.border_width = ev->border_width;
		wc.sibling = ev->above;
		wc.stack_mode = ev->detail;
		XConfigureWindow(display, ev->window, ev->value_mask, &wc);
	}
}

static void
configurenotify(XConfigureEvent *ev) {
	Window *w;

	USED(ev);
	if((w = findwin(ev->window)))
		handle(w, config, ev);
}

static void
clientmessage(XClientMessageEvent *ev) {

	USED(ev);
}

static void
destroynotify(XDestroyWindowEvent *ev) {
	Window *w;

	if((w = findwin(ev->window))) 
		handle(w, destroy, ev);
}

static void
enternotify(XCrossingEvent *ev) {
	Window *w;
	static int sel_screen;

	xtime = ev->time;
	if(ev->mode != NotifyNormal)
		return;

	if((w = findwin(ev->window))) 
		handle(w, enter, ev);
	else if(ev->window == scr.root.w)
		sel_screen = true;
}

static void
leavenotify(XCrossingEvent *ev) {

	xtime = ev->time;
#if 0
	if((ev->window == scr.root.w) && !ev->same_screen)
		sel_screen = true;
#endif
}

static void
focusin(XFocusChangeEvent *ev) {
	Window *w;

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
	if((ev->mode == NotifyWhileGrabbed)) /* && (screen->hasgrab != &c_root)) */
		return;

	if((w = findwin(ev->window))) 
		handle(w, focusin, ev);
#if 0
	else if(ev->mode == NotifyGrab) {
		if(ev->window == scr.root.w)
			screen->hasgrab = &c_root;
		/* Some unmanaged window has grabbed focus */
		else if((c = screen->focus)) {
			print_focus("focusin", &c_magic, "<magic>");
			screen->focus = &c_magic;
			if(c->sel)
				frame_draw(c->sel);
		}
	}
#endif
}

static void
focusout(XFocusChangeEvent *ev) {
	Window *w;

	if(!((ev->detail == NotifyNonlinear)
	   ||(ev->detail == NotifyNonlinearVirtual)
	   ||(ev->detail == NotifyVirtual)
	   ||(ev->detail == NotifyInferior)
	   ||(ev->detail == NotifyAncestor)))
		return;
#if 0
	if(ev->mode == NotifyUngrab)
		screen->hasgrab = nil;
#endif

	if((w = findwin(ev->window))) 
		handle(w, focusout, ev);
}

static void
expose(XExposeEvent *ev) {
	Window *w;

	if(ev->count == 0) {
		if((w = findwin(ev->window))) 
			handle(w, expose, ev);
	}
}

static void
keypress(XKeyEvent *ev) {
	Window *w;

	xtime = ev->time;
	if((w = findwin(ev->window))) 
		handle(w, kdown, ev);
}

static void
mappingnotify(XMappingEvent *ev) {

	/* Why do you need me to tell you this? */
	XRefreshKeyboardMapping(ev);
}

static void
maprequest(XMapRequestEvent *ev) {

	USED(ev);
}

static void
motionnotify(XMotionEvent *ev) {
	Window *w;

	xtime = ev->time;
	if((w = findwin(ev->window)))
		handle(w, motion, ev);
}

static void
propertynotify(XPropertyEvent *ev) {
	Window *w;

	xtime = ev->time;
	if((w = findwin(ev->window))) 
		handle(w, property, ev);
}

static void
mapnotify(XMapEvent *ev) {
	Window *w;

	if((w = findwin(ev->window))) 
		handle(w, map, ev);
}

static void
unmapnotify(XUnmapEvent *ev) {
	Window *w;

	if((w = findwin(ev->window)) && (ev->event == w->parent->w)) {
		w->mapped = false;
		if(ev->send_event || w->unmapped-- == 0)
			handle(w, unmap, ev);
	}
}

static EvHandler handler[LASTEvent] = {
	[ButtonPress] =		(EvHandler)buttonpress,
	[ButtonRelease] =	(EvHandler)buttonrelease,
	[ConfigureRequest] =	(EvHandler)configurerequest,
	[ConfigureNotify] =	(EvHandler)configurenotify,
	[ClientMessage] =	(EvHandler)clientmessage,
	[DestroyNotify] =	(EvHandler)destroynotify,
	[EnterNotify] =		(EvHandler)enternotify,
	[Expose] =		(EvHandler)expose,
	[FocusIn] =		(EvHandler)focusin,
	[FocusOut] =		(EvHandler)focusout,
	[KeyPress] =		(EvHandler)keypress,
	[LeaveNotify] =		(EvHandler)leavenotify,
	[MapNotify] =		(EvHandler)mapnotify,
	[MapRequest] =		(EvHandler)maprequest,
	[MappingNotify] =	(EvHandler)mappingnotify,
	[MotionNotify] =	(EvHandler)motionnotify,
	[PropertyNotify] =	(EvHandler)propertynotify,
	[UnmapNotify] =		(EvHandler)unmapnotify,
};

void
check_x_event(IxpConn *c) {
	XEvent ev;

	USED(c);
	while(XCheckMaskEvent(display, ~0, &ev))
		dispatch_event(&ev);
}

