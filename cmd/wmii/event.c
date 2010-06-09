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

void
print_focus(const char *fn, Client *c, const char *to) {
	Dprint(DFocus, "%s() disp.focus:\n", fn);
	Dprint(DFocus, "\t%#C => %#C\n", disp.focus, c);
	Dprint(DFocus, "\t%C => %s\n", disp.focus, to);
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
		event_handle(w, focusin, ev);
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
		event_handle(w, focusout, ev);
}

