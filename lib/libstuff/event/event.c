/* Copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "event.h"

typedef bool (*Handler)(Window*, void*, XEvent*);
void	(*event_debug)(XEvent*);
long	event_lastconfigure;
long	event_xtime;
bool	event_looprunning;

EventHandler event_handler[LASTEvent] = {
	[ButtonPress] =		(EventHandler)event_buttonpress,
	[ButtonRelease] =	(EventHandler)event_buttonrelease,
	[ClientMessage] =	(EventHandler)event_clientmessage,
	[ConfigureNotify] =	(EventHandler)event_configurenotify,
	[ConfigureRequest] =	(EventHandler)event_configurerequest,
	[DestroyNotify] =	(EventHandler)event_destroynotify,
	[EnterNotify] =		(EventHandler)event_enternotify,
	[Expose] =		(EventHandler)event_expose,
	[FocusIn] =		(EventHandler)event_focusin,
	[FocusOut] =		(EventHandler)event_focusout,
	[KeyPress] =		(EventHandler)event_keypress,
	[KeyRelease] =		(EventHandler)event_keyrelease,
	[LeaveNotify] =		(EventHandler)event_leavenotify,
	[MapNotify] =		(EventHandler)event_mapnotify,
	[MapRequest] =		(EventHandler)event_maprequest,
	[MappingNotify] =	(EventHandler)event_mappingnotify,
	[MotionNotify] =	(EventHandler)event_motionnotify,
	[PropertyNotify] =	(EventHandler)event_propertynotify,
	[ReparentNotify] =	(EventHandler)event_reparentnotify,
	[SelectionClear] =	(EventHandler)event_selectionclear,
	[SelectionNotify] =	(EventHandler)event_selection,
	[UnmapNotify] =		(EventHandler)event_unmapnotify,
};

void
_event_handle(Window *w, ulong offset, XEvent *event) {
	Handler f;
	HandlersLink *l;

	if(w->handler && (f = structmember(w->handler, Handler, offset)))
		if(!f(w, w->aux, event))
			return;

	for(l=w->handler_link; l; l=l->next)
		if((f = structmember(l->handler, Handler, offset)))
			if(!f(w, l->aux, event))
				return;
}

void
event_dispatch(XEvent *e) {
	if(event_debug)
		event_debug(e);

	if(e->type < nelem(event_handler)) {
		if(event_handler[e->type])
			event_handler[e->type](e);
	}else
		xext_event(e);
}

void
event_check(void) {
	XEvent ev;

	while(XPending(display)) {
		XNextEvent(display, &ev);
		event_dispatch(&ev);
	}
}

void
event_loop(void) {
	XEvent ev;

	event_looprunning = true;
	while(event_looprunning) {
		XNextEvent(display, &ev);
		event_dispatch(&ev);
	}
}

uint
event_flush(long event_mask, bool dispatch) {
	XEvent ev;
	uint n = 0;

	while(XCheckMaskEvent(display, event_mask, &ev)) {
		if(dispatch)
			event_dispatch(&ev);
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
event_flushenter(void) {
	XEvent e;
	long l;
	int n;

	l = 0;
	n = 0;
	while(XCheckIfEvent(display, &e, findenter, (void*)&l))
		n++;
	return n;
}

