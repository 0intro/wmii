/* Copyright ©2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "dat.h"
#include "fns.h"

static Handlers selection_handlers;
static Handlers steal_handlers;

static Selection*
_selection_create(char *selection, ulong time,
		 void (*request)(Selection*, XSelectionRequestEvent*),
		 void (*cleanup)(Selection*),
		 bool lazy) {
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

	if (!lazy) {
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
	}

	s->selection = estrdup(selection);
	return s;
}

Selection*
selection_create(char *selection, ulong time,
		 void (*request)(Selection*, XSelectionRequestEvent*),
		 void (*cleanup)(Selection*)) {
	return _selection_create(selection, time, request, cleanup, false);
}

static void
_selection_manage(Selection *s) {

	if (s->oldowner) {
		Dprint("[selection] Grabbing.\n");
		XSetSelectionOwner(display, xatom(s->selection), s->owner->xid, s->time_start);
		if(XGetSelectionOwner(display, xatom(s->selection)) != s->owner->xid) {
			selection_release(s);
			return;
		}
	}

	Dprint("[selection] Notifying.\n");
	clientmessage(&scr.root, "MANAGER", SubstructureNotifyMask|StructureNotifyMask, 32,
		      (ClientMessageData){ .l = {s->time_start, xatom(s->selection), s->owner->xid} });
}

static void
timeout(long timer, void *v) {
	Selection *s;

	s = v;
	Dprint("[selection] Done waiting. Killing 0x%ulx.\n", s->oldowner);
	s->timer = 0;
	XKillClient(display, s->oldowner);
	sync();
}

Selection*
selection_manage(char *selection, ulong time,
		 void (*message)(Selection*, XClientMessageEvent*),
		 void (*cleanup)(Selection*),
		 bool steal) {
	Selection *s;
	Window *w;
	XWindow old;

	if((old = XGetSelectionOwner(display, xatom(selection)))) {
		if (!steal)
			return nil;

		w = emallocz(sizeof *w);
		w->type = WWindow;
		w->xid = old;
		selectinput(w, StructureNotifyMask);

		/* Hack for broken Qt systray implementation. If it
		 * finds a new system tray running when the old one
		 * dies, it never selects the StructureNotify mask
		 * on it, and therefore never disassociates from it,
		 * and completely ignores any future MANAGER
		 * messages it receives.
		 */
		XSetSelectionOwner(display, xatom(selection), 0, time);
	}

	s = _selection_create(selection, time, nil, cleanup, old);
	if(s) {
		s->message = message;
		s->oldowner = old;
		if(!old)
			_selection_manage(s);
		else {
			Dprint("[selection] Waiting for old owner %W to die...\n", w);
			pushhandler(w, &steal_handlers, s);
			s->timer = ixp_settimer(&srv, 2000, timeout, s);
		}
	}

	return s;
}

void
selection_release(Selection *s) {
	if(s->cleanup)
		s->cleanup(s);
	if(!s->time_end)
		XSetSelectionOwner(display, xatom(s->selection), None, s->time_start);
	destroywindow(s->owner);
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
	Dprint("[selection] Lost selection\n");
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

	Dprint("[selection] Request: %A\n", ev->target);
	if(ev->target == xatom("TIMESTAMP")) {
		/* Per ICCCM §2.6.2. */
		changeprop_ulong(window(ev->requestor),
				 atomname(ev->property), "TIMESTAMP",
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

static bool
destroy_event(Window *w, void *aux, XDestroyWindowEvent *e) {
	Selection *s;

	Dprint("[selection] Old owner is dead.\n");
	s = aux;
	if(s->timer)
		ixp_unsettimer(&srv, s->timer);
	s->timer = 0;

	_selection_manage(s);
	s->oldowner = 0;
	return false;
}

static Handlers steal_handlers = {
	.destroy = destroy_event,
};

