/* Copyright ©2006-2009 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "dat.h"
#include <X11/keysym.h>
#include "fns.h"

typedef void (*EvHandler)(XEvent*);

void
dispatch_event(XEvent *e) {
	Dprint(DEvent, "%E\n", e);
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
	if(e->type == PropertyNotify && e->xproperty.window == w->xid) {
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

static Bool
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

	ignoreenter = ev->serial;
	if((w = findwin(ev->window)))
		handle(w, config, ev);
}

static void
clientmessage(XClientMessageEvent *ev) {

	if(ewmh_clientmessage(ev))
		return;
	if(xdnd_clientmessage(ev))
		return;
}

static void
destroynotify(XDestroyWindowEvent *ev) {
	Window *w;
	Client *c;

	if((w = findwin(ev->window)))
		handle(w, destroy, ev);
	else {
		if((c = win2client(ev->window)))
			fprint(2, "Badness: Unhandled DestroyNotify: "
				  "Client: %p, Window: %W, Name: %s\n",
				  c, &c->w, c->name);
	}
}

static void
enternotify(XCrossingEvent *ev) {
	Window *w;

	xtime = ev->time;
	if(ev->mode != NotifyNormal)
		return;

	if((w = findwin(ev->window)))
		handle(w, enter, ev);
}

static void
leavenotify(XCrossingEvent *ev) {
	Window *w;

	xtime = ev->time;
	if((w = findwin(ev->window)))
		handle(w, leave, ev);
}

void
print_focus(const char *fn, Client *c, const char *to) {
	Dprint(DFocus, "%s() disp.focus:\n", fn);
	Dprint(DFocus, "\t%C => %C\n", disp.focus, c);
	Dprint(DFocus, "\t%s => %s\n", clientname(disp.focus), to);
}

static void
focusin(XFocusChangeEvent *ev) {
	Window *w;
	Client *c;

	/* Yes, we're focusing in on nothing, here. */
	if(ev->detail == NotifyDetailNone) {
		print_focus("focusin", &c_magic, "<magic[none]>");
		disp.focus = &c_magic;
		setfocus(screen->barwin, RevertToParent);
		return;
	}

	if(!((ev->detail == NotifyNonlinear)
	   ||(ev->detail == NotifyNonlinearVirtual)
	   ||(ev->detail == NotifyVirtual)
	   ||(ev->detail == NotifyInferior)
	   ||(ev->detail == NotifyAncestor)))
		return;
	if((ev->mode == NotifyWhileGrabbed) && (disp.hasgrab != &c_root))
		return;

	if(ev->window == screen->barwin->xid) {
		print_focus("focusin", nil, "<nil>");
		disp.focus = nil;
	}
	else if((w = findwin(ev->window)))
		handle(w, focusin, ev);
	else if(ev->mode == NotifyGrab) {
		/* Some unmanaged window has grabbed focus */
		if((c = disp.focus)) {
			print_focus("focusin", &c_magic, "<magic>");
			disp.focus = &c_magic;
			if(c->sel)
				frame_draw(c->sel);
		}
	}
}

static void
focusout(XFocusChangeEvent *ev) {
	XEvent me;
	Window *w;

	if(!((ev->detail == NotifyNonlinear)
	   ||(ev->detail == NotifyNonlinearVirtual)
	   ||(ev->detail == NotifyVirtual)
	   ||(ev->detail == NotifyInferior)
	   ||(ev->detail == NotifyAncestor)))
		return;
	if(ev->mode == NotifyUngrab)
		disp.hasgrab = nil;

	if((ev->mode == NotifyGrab)
	&& XCheckMaskEvent(display, KeyPressMask, &me))
		dispatch_event(&me);
	else if((w = findwin(ev->window)))
		handle(w, focusout, ev);
}

static void
expose(XExposeEvent *ev) {
	Window *w;

	if(ev->count == 0)
		if((w = findwin(ev->window)))
			handle(w, expose, ev);
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

	XRefreshKeyboardMapping(ev);
	if(ev->request == MappingKeyboard)
		update_keys();
}

static void
maprequest(XMapRequestEvent *ev) {
	Window *w;

	if((w = findwin(ev->parent)))
		handle(w, mapreq, ev);
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

	ignoreenter = ev->serial;
	if((w = findwin(ev->window)))
		handle(w, map, ev);
}

static void
unmapnotify(XUnmapEvent *ev) {
	Window *w;

	ignoreenter = ev->serial;
	if((w = findwin(ev->window)) && (ev->event == w->parent->xid)) {
		w->mapped = false;
		if(ev->send_event || w->unmapped-- == 0)
			handle(w, unmap, ev);
	}
}

EvHandler handler[LASTEvent] = {
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
	while(XPending(display)) {
		XNextEvent(display, &ev);
		dispatch_event(&ev);
	}
}

