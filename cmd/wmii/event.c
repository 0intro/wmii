/* Copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "dat.h"
#include <X11/keysym.h>
#include "fns.h"

void
debug_event(XEvent *e) {
	Dprint(DEvent, "%E\n", e);
}

#define handle(w, fn, ev) \
	BLOCK(if((w)->handler->fn) (w)->handler->fn((w), ev))

void
event_buttonpress(XButtonPressedEvent *ev) {
	Window *w;

	if((w = findwin(ev->window)))
		handle(w, bdown, ev);
	else
		XAllowEvents(display, ReplayPointer, ev->time);
}

void
event_configurenotify(XConfigureEvent *ev) {
	Window *w;

	ignoreenter = ev->serial;
	if((w = findwin(ev->window)))
		handle(w, config, ev);
}

void
event_clientmessage(XClientMessageEvent *ev) {

	if(ewmh_clientmessage(ev))
		return;
	if(xdnd_clientmessage(ev))
		return;
}

void
event_destroynotify(XDestroyWindowEvent *ev) {
	Window *w;
	Client *c;

	if((w = findwin(ev->window)))
		handle(w, destroy, ev);
	else if((c = win2client(ev->window)))
		fprint(2, "Badness: Unhandled DestroyNotify: Client: %p, Window: %W, Name: %s\n",
		       c, &c->w, c->name);
}

void
print_focus(const char *fn, Client *c, const char *to) {
	Dprint(DFocus, "%s() disp.focus:\n", fn);
	Dprint(DFocus, "\t%C => %C\n", disp.focus, c);
	Dprint(DFocus, "\t%s => %s\n", clientname(disp.focus), to);
}

void
event_focusin(XFocusChangeEvent *ev) {
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

void
event_focusout(XFocusChangeEvent *ev) {
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
		event_dispatch(&me);
	else if((w = findwin(ev->window)))
		handle(w, focusout, ev);
}

void
event_mapnotify(XMapEvent *ev) {
       Window *w;

       ignoreenter = ev->serial;
       if((w = findwin(ev->window)))
               handle(w, map, ev);
}

void
event_unmapnotify(XUnmapEvent *ev) {
	Window *w;

	ignoreenter = ev->serial;
	if((w = findwin(ev->window)) && (ev->event == w->parent->xid)) {
		w->mapped = false;
		if(ev->send_event || w->unmapped-- == 0)
			handle(w, unmap, ev);
	}
}

