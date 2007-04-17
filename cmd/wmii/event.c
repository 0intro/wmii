/* Copyright ©2004-2006 Anselm R. Garbe <garbeam at gmail dot com>
 * Copyright ©2006-2007 Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */
#include <stdio.h>
#include <X11/keysym.h>
#include <util.h>
#include "dat.h"
#include "fns.h"
#include "printevent.h"

void
dispatch_event(XEvent *e) {
	if(handler[e->type])
		handler[e->type](e);
}

uint
flushevents(long event_mask, Bool dispatch) {
	XEvent ev;
	uint n = 0;

	while(XCheckMaskEvent(display, event_mask, &ev)) {
		if(dispatch)
			dispatch_event(&ev);
		n++;
	}
	return n;
}

static void
buttonrelease(XEvent *e) {
	XButtonPressedEvent *ev;
	Window *w;

	ev = &e->xbutton;
	if((w = findwin(e->xany.window)))
		if(w->handler->bup)
			w->handler->bup(w, ev);
}

static void
buttonpress(XEvent *e) {
	XButtonPressedEvent *ev;
	Window *w;

	ev = &e->xbutton;
	if((w = findwin(e->xany.window))) {
		if(w->handler->bdown)
			w->handler->bdown(w, ev);
	}
	else
		XAllowEvents(display, ReplayPointer, ev->time);
}

static void
configurerequest(XEvent *e) {
	XConfigureRequestEvent *ev;
	Window *w;
	XWindowChanges wc;

	ev = &e->xconfigurerequest;
	if((w = findwin(e->xany.window))) {
		if(w->handler->configreq)
			w->handler->configreq(w, ev);
	}else{
		wc.x = ev->x;
		wc.y = ev->y;
		wc.width = ev->width;
		wc.height = ev->height;
		wc.border_width = ev->border_width;
		wc.sibling = ev->above;
		wc.stack_mode = ev->detail;
		ev->value_mask &= ~(CWStackMode|CWSibling);
		XConfigureWindow(display, ev->window, ev->value_mask, &wc);
		XSync(display, False);
	}
}

static void
destroynotify(XEvent *e) {
	XDestroyWindowEvent *ev;
	Window *w;

	ev = &e->xdestroywindow;
	if((w = findwin(e->xany.window))) {
		if(w->handler->destroy)
			w->handler->destroy(w, ev);
	}
}

static void
enternotify(XEvent *e) {
	XCrossingEvent *ev;
	Window *w;

	ev = &e->xcrossing;
	if(ev->mode != NotifyNormal)
		return;

	if((w = findwin(e->xany.window))) {
		if(w->handler->enter)
			w->handler->enter(w, ev);
	}
	else if(ev->window == scr.root.w) {
		sel_screen = True;
		draw_frames();
	}
}

static void
leavenotify(XEvent *e) {
	XCrossingEvent *ev;
	Window *w;

	ev = &e->xcrossing;
	w = findwin(e->xany.window);
	if((ev->window == scr.root.w) && !ev->same_screen) {
		sel_screen = True;
		draw_frames();
	}
}

void
print_focus(Client *c, char *to) {
		if(verbose) {
			fprintf(stderr, "screen->focus: %p => %p\n",
				screen->focus, c);
			fprintf(stderr, "\t%s => %s\n",
				screen->focus ? screen->focus->name : "<nil>",
				to);
		}
}

static void
focusin(XEvent *e) {
	XFocusChangeEvent *ev;
	Window *w;
	Client *c;
	XEvent me;

	ev = &e->xfocus;
	/* Yes, we're focusing in on nothing, here. */
	if(ev->detail == NotifyDetailNone) {
		XSetInputFocus(display, screen->barwin->w, RevertToParent, CurrentTime);
		return;
	}

	if(!((ev->detail == NotifyNonlinear)
	   ||(ev->detail == NotifyNonlinearVirtual)
	   ||(ev->detail == NotifyVirtual)
	   ||(ev->detail == NotifyInferior)
	   ||(ev->detail == NotifyAncestor)))
		return;
	if((ev->mode == NotifyWhileGrabbed)
	&& (screen->hasgrab != &c_root))
		return;

	if((w = findwin(e->xany.window))) {
		if(w->handler->focusin)
			w->handler->focusin(w, ev);
	}
	else if(ev->window == screen->barwin->w) {
		print_focus(nil, "<nil>");
		screen->focus = nil;
	}
	else if(ev->mode == NotifyGrab) {
		if(ev->window == scr.root.w)
			if(XCheckMaskEvent(display, KeyPressMask, &me)) {
				/* wmii has grabbed focus */
				screen->hasgrab = &c_root;
				dispatch_event(&me);
				return;
			}
		/* Some unmanaged window has grabbed focus */
		if((c = screen->focus)) {
			print_focus(&c_magic, "<magic>");
			screen->focus = &c_magic;
			if(c->sel)
				draw_frame(c->sel);
		}
	}
}

static void
focusout(XEvent *e) {
	XFocusChangeEvent *ev;
	Window *w;

	ev = &e->xfocus;
	if(!((ev->detail == NotifyNonlinear)
	   ||(ev->detail == NotifyNonlinearVirtual)))
		return;
	if(ev->mode == NotifyUngrab)
		screen->hasgrab = nil;

	if((w = findwin(e->xany.window))) {
		if(w->handler->focusout)
			w->handler->focusout(w, ev);
	}
}

static void
expose(XEvent *e) {
	XExposeEvent *ev;
	Window *w;

	ev = &e->xexpose;
	if(ev->count == 0) {
		if((w = findwin(e->xany.window))) {
			if(w->handler->expose)
				w->handler->expose(w, ev);
		}
	}
}

static void
keypress(XEvent *e) {
	XKeyEvent *ev;
	Window *w;

	ev = &e->xkey;
	w = findwin(e->xany.window);
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
	Window *w;
	XWindowAttributes wa;

	ev = &e->xmaprequest;
	w = findwin(e->xany.window);
	if(!XGetWindowAttributes(display, ev->window, &wa))
		return;
	if(wa.override_redirect) {
		XSelectInput(display, ev->window,
				(StructureNotifyMask | PropertyChangeMask));
		return;
	}
	if(!win2client(ev->window))
		manage_client(create_client(ev->window, &wa));
}

static void
motionnotify(XEvent *e) {
	XMotionEvent *ev;
	Window *w;

	ev = &e->xmotion;
	if((w = findwin(e->xany.window))) {
		if(w->handler->motion)
			w->handler->motion(w, ev);
	}
}

static void
propertynotify(XEvent *e) {
	XPropertyEvent *ev;
	Window *w;

	ev = &e->xproperty;
	if((w = findwin(e->xany.window))) {
		if(w->handler->property)
			w->handler->property(w, ev);
	}
}

static void
mapnotify(XEvent *e) {
	XMapEvent *ev;
	Window *w;

	ev = &e->xmap;
	if((w = findwin(e->xany.window))) {
		if(w->handler->map)
			w->handler->map(w, ev);
	}
}

static void
unmapnotify(XEvent *e) {
	XUnmapEvent *ev;
	Window *w;

	ev = &e->xunmap;
	if((w = findwin(e->xany.window))) {
		if(ev->send_event || w->unmapped-- == 0)
			if(w->handler->unmap)
				w->handler->unmap(w, ev);
	}
}

void (*handler[LASTEvent]) (XEvent *) = {
	[ButtonPress]	= buttonpress,
	[ButtonRelease]	= buttonrelease,
	[ConfigureRequest]=configurerequest,
	[DestroyNotify]	= destroynotify,
	[EnterNotify]	= enternotify,
	[Expose]	= expose,
	[FocusIn]	= focusin,
	[FocusOut]	= focusout,
	[KeyPress]	= keypress,
	[LeaveNotify]	= leavenotify,
	[MapNotify]	= mapnotify,
	[MapRequest]	= maprequest,
	[MappingNotify]	= mappingnotify,
	[MotionNotify]	= motionnotify,
	[PropertyNotify]= propertynotify,
	[UnmapNotify]	= unmapnotify,
};

void
check_x_event(IxpConn *c) {
	XEvent ev;
	while(XPending(display)) {
		XNextEvent(display, &ev);
		if(verbose)
			printevent(&ev);
		dispatch_event(&ev);
		/* Hack to alleviate an apparant Xlib bug */
		XPending(display);
	}
}
