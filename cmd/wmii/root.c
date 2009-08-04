/* Copyright ©2008-2009 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "dat.h"
#include "fns.h"

static Handlers handlers;

void
root_init(void) {
	WinAttr wa;

	wa.event_mask = EnterWindowMask
		      | FocusChangeMask
		      | LeaveWindowMask
		      | PointerMotionMask
		      | SubstructureNotifyMask
		      | SubstructureRedirectMask;
	wa.cursor = cursor[CurNormal];
	setwinattr(&scr.root, &wa,
			  CWEventMask
			| CWCursor);
	sethandler(&scr.root, &handlers);
}

static void
enter_event(Window *w, XCrossingEvent *e) {
	disp.sel = true;
	frame_draw_all();
}

static void
leave_event(Window *w, XCrossingEvent *e) {
	if(!e->same_screen) {
		disp.sel = false;
		frame_draw_all();
	}
}

static void
focusin_event(Window *w, XFocusChangeEvent *e) {
	if(e->mode == NotifyGrab)
		disp.hasgrab = &c_root;
}

static void
mapreq_event(Window *w, XMapRequestEvent *e) {
	XWindowAttributes wa;

	if(!XGetWindowAttributes(display, e->window, &wa))
		return;
	if(wa.override_redirect) {
		/* Do I really want these? */
		/* Probably not.
		XSelectInput(display, e->window,
			 PropertyChangeMask | StructureNotifyMask);
		*/
		return;
	}
	if(!win2client(e->window))
		client_create(e->window, &wa);
}

static void
motion_event(Window *w, XMotionEvent *e) {
	Rectangle r, r2;

	r = rectsetorigin(Rect(0, 0, 1, 1), Pt(e->x_root, e->y_root));
	r2 = constrain(r, 0);
	if(!eqrect(r, r2))
		warppointer(r2.min);
}

static void
kdown_event(Window *w, XKeyEvent *e) {

	e->state &= valid_mask;
	kpress(w->xid, e->state, (KeyCode)e->keycode);
}

static Handlers handlers = {
	.enter = enter_event,
	.focusin = focusin_event,
	.kdown = kdown_event,
	.leave = leave_event,
	.mapreq = mapreq_event,
	.motion = motion_event,
};

