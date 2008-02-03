/* Copyright ©2006-2008 Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */
#include "dat.h"
#include <X11/keysym.h>
#include "fns.h"

void
dispatch_event(XEvent *e) {
	Debug(DEvent)
		printevent(e);
	if(e->type < nelem(handler)) {
		if(handler[e->type])
			handler[e->type](e);
	}else
		xext_event(e);
}

#define handle(w, fn, ev) \
	BLOCK(if((w)->handler->fn) (w)->handler->fn((w), ev))

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
configurerequest(XEvent *e) {
	XConfigureRequestEvent *ev;
	XWindowChanges wc;
	Window *w;

	ev = &e->xconfigurerequest;
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
clientmessage(XEvent *e) {
	XClientMessageEvent *ev;

	ev = &e->xclient;
	if(ewmh_clientmessage(ev))
		return;
	if(xdnd_clientmessage(ev))
		return;
}

static void
destroynotify(XEvent *e) {
	XDestroyWindowEvent *ev;
	Window *w;
	Client *c;

	ev = &e->xdestroywindow;
	if((w = findwin(ev->window))) 
		handle(w, destroy, ev);
	else {
		Dprint(DGeneric, "DestroyWindow(%ux) (no handler)\n", (uint)ev->window);
		if((c = win2client(ev->window)))
			fprint(2, "Badness: Unhandled DestroyNotify: "
				"Client: %p, Window: %W, Name: %s\n", c, &c->w, c->name);
	}
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
	else if(ev->window == scr.root.w) {
		sel_screen = true;
		frame_draw_all();
	}
}

static void
leavenotify(XEvent *e) {
	XCrossingEvent *ev;

	ev = &e->xcrossing;
	xtime = ev->time;
	if((ev->window == scr.root.w) && !ev->same_screen) {
		sel_screen = true;
		frame_draw_all();
	}
}

void
print_focus(const char *fn, Client *c, const char *to) {
	Dprint(DFocus, "%s() screen->focus:\n", fn);
	Dprint(DFocus, "\t%C => %C\n", screen->focus, c);
	Dprint(DFocus, "\t%s => %s\n", clientname(screen->focus), to);
}

static void
focusin(XEvent *e) {
	XFocusChangeEvent *ev;
	Window *w;
	Client *c;

	ev = &e->xfocus;
	/* Yes, we're focusing in on nothing, here. */
	if(ev->detail == NotifyDetailNone) {
		print_focus("focusin", &c_magic, "<magic[none]>");
		screen->focus = &c_magic;
		setfocus(screen->barwin, RevertToParent);
		return;
	}

	if(!((ev->detail == NotifyNonlinear)
	   ||(ev->detail == NotifyNonlinearVirtual)
	   ||(ev->detail == NotifyVirtual)
	   ||(ev->detail == NotifyInferior)
	   ||(ev->detail == NotifyAncestor)))
		return;
	if((ev->mode == NotifyWhileGrabbed) && (screen->hasgrab != &c_root))
		return;

	if(ev->window == screen->barwin->w) {
		print_focus("focusin", nil, "<nil>");
		screen->focus = nil;
	}
	else if((w = findwin(ev->window))) 
		handle(w, focusin, ev);
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
}

static void
focusout(XEvent *e) {
	XEvent me;
	XFocusChangeEvent *ev;
	Window *w;

	ev = &e->xfocus;
	if(!((ev->detail == NotifyNonlinear)
	   ||(ev->detail == NotifyNonlinearVirtual)
	   ||(ev->detail == NotifyVirtual)
	   ||(ev->detail == NotifyInferior)
	   ||(ev->detail == NotifyAncestor)))
		return;
	if(ev->mode == NotifyUngrab)
		screen->hasgrab = nil;

	if((ev->mode == NotifyGrab)
	&& XCheckMaskEvent(display, KeyPressMask, &me))
		dispatch_event(&me);
	else if((w = findwin(ev->window))) 
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
	ev->state &= valid_mask;
	if(ev->window == scr.root.w)
		kpress(scr.root.w, ev->state, (KeyCode) ev->keycode);
}

static void
mappingnotify(XEvent *e) {
	XMappingEvent *ev;

	ev = &e->xmapping;
	XRefreshKeyboardMapping(ev);
	if(ev->request == MappingKeyboard)
		update_keys();
}

static void
maprequest(XEvent *e) {
	XMapRequestEvent *ev;
	XWindowAttributes wa;

	ev = &e->xmaprequest;
	if(!XGetWindowAttributes(display, ev->window, &wa))
		return;
	if(wa.override_redirect) {
		/* Do I really want these? */
		XSelectInput(display, ev->window,
			(StructureNotifyMask | PropertyChangeMask));
		return;
	}
	if(!win2client(ev->window))
		client_create(ev->window, &wa);
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
	if((w = findwin(ev->window)) && (ev->event == w->parent->w)) {
		if(ev->send_event || w->unmapped-- == 0)
			handle(w, unmap, ev);
	}
}

void (*handler[LASTEvent]) (XEvent *) = {
	[ButtonPress] = buttonpress,
	[ButtonRelease] = buttonrelease,
	[ConfigureRequest] = configurerequest,
	[ClientMessage] = clientmessage,
	[DestroyNotify] = destroynotify,
	[EnterNotify] = enternotify,
	[Expose] = expose,
	[FocusIn] = focusin,
	[FocusOut] = focusout,
	[KeyPress] = keypress,
	[LeaveNotify] = leavenotify,
	[MapNotify] = mapnotify,
	[MapRequest] = maprequest,
	[MappingNotify] = mappingnotify,
	[MotionNotify] = motionnotify,
	[PropertyNotify] = propertynotify,
	[UnmapNotify] = unmapnotify,
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
