/* Copyright ©2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "dat.h"
#include "fns.h"

static Handlers selection_handlers;

Selection*
selection_create(char *selection, ulong time,
		 void (*request)(Selection*, XSelectionRequestEvent*),
		 void (*cleanup)(Selection*)) {
	Selection *s;

	if(time == 0)
		time = event_xtime;

	s = emallocz(sizeof *s);
	s->owner = createwindow(&scr.root, Rect(0, 0, 1, 1), 0,
				InputOnly, nil, 0);
	s->owner->aux = s;
	s->request = request;
	s->cleanup = cleanup;
	s->time_start = time;

	sethandler(s->owner, &selection_handlers);

	XSetSelectionOwner(display, xatom(selection), s->owner->xid, time);

	/*
	 * There is a race here that ICCCM doesn't mention. It's
	 * possible that we've gained and lost the selection in this
	 * time, and a client's sent us a selection request. We're
	 * required to reply to it, but since we're destroying the
	 * window, we'll never hear about it. Since ICCCM doesn't
	 * mention it, we assume that other clients behave likewise,
	 * and therefore clients must be prepared to deal with such
	 * behavior regardless.
	 */
	if(XGetSelectionOwner(display, xatom(selection)) != s->owner->xid) {
		destroywindow(s->owner);
		free(s);
		return nil;
	}

	s->selection = estrdup(selection);
	return s;
}

#include <X11/Xlib-xcb.h>
#include <xcb/xproto.h>

Selection*
selection_manage(char *selection, ulong time,
		 void (*message)(Selection*, XClientMessageEvent*),
		 void (*cleanup)(Selection*)) {
	Selection *s;

	if(XGetSelectionOwner(display, xatom(selection)) != None)
		return nil;

	s = selection_create(selection, time, nil, cleanup);
	if(s) {
		s->message = message;
		clientmessage(&scr.root, "MANAGER", SubstructureNotifyMask|StructureNotifyMask, 32,
			      (ClientMessageData){ .l = {time, xatom(selection), s->owner->xid} });
	}

	return s;
}

void
selection_release(Selection *s) {
	if(!s->time_end)
		XSetSelectionOwner(display, xatom(s->selection), None, s->time_start);
	destroywindow(s->owner);
	if(s->cleanup)
		s->cleanup(s);
	free(s->selection);
	free(s);
}

static void
selection_notify(Selection *s, XSelectionRequestEvent *ev, bool success) {
	XSelectionEvent notify;

	notify.type = SelectionNotify;
	notify.requestor = ev->requestor;
	notify.selection = ev->selection;
	notify.target = ev->target;
	notify.property = success ? ev->property : None;
	notify.time = ev->time;

	sendevent(window(ev->requestor), false, 0L, &notify);
}

static bool
message_event(Window *w, void *aux, XClientMessageEvent *ev) {
	Selection *s;

	s = aux;
	if(s->message)
		s->message(s, ev);
	return false;
}

static bool
selectionclear_event(Window *w, void *aux, XSelectionClearEvent *ev) {
	Selection *s;

	USED(w, ev);
	s = aux;
	s->time_end = ev->time;
	selection_release(s);
	return false;
}

static bool
selectionrequest_event(Window *w, void *aux, XSelectionRequestEvent *ev) {
	Selection *s;

	s = aux;
	if(ev->property == None)
		ev->property = ev->target; /* Per ICCCM §2.2. */

	if(ev->target == xatom("TIMESTAMP")) {
		/* Per ICCCM §2.6.2. */
		changeprop_ulong(window(ev->requestor),
				 XGetAtomName(display, ev->property), "TIMESTAMP",
				 &s->time_start, 1);
		selection_notify(s, ev, true);
		return false;
	}

	if(s->request)
		s->request(s, ev);
	else
		selection_notify(s, ev, false);
	return false;
}

static Handlers selection_handlers = {
	.message = message_event,
	.selectionclear = selectionclear_event,
	.selectionrequest = selectionrequest_event,
};

