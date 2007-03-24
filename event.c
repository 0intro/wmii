/* Copyright ©2004-2006 Anselm R. Garbe <garbeam at gmail dot com>
 * Copyright ©2006-2007 Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */
#include <X11/keysym.h>
#include "wmii.h"
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

	while(XCheckMaskEvent(blz.dpy, event_mask, &ev)) {
		if(dispatch)
			dispatch_event(&ev);
		n++;
	}
	return n;
}

static void
buttonrelease(XEvent *e) {
	XButtonPressedEvent *ev;
	Frame *f;
	Bar *b;

	ev = &e->xbutton;
	if(ev->window == screen->barwin) {
		for(b=screen->bar[BarLeft]; b; b=b->next)
			if(ptinrect(ev->x, ev->y, &b->brush.rect)) {
				write_event("LeftBarClick %d %s\n", ev->button, b->name);
				return;
			}
		for(b=screen->bar[BarRight]; b; b=b->next)
			if(ptinrect(ev->x, ev->y, &b->brush.rect)) {
				write_event("RightBarClick %d %s\n", ev->button, b->name);
				return;
			}
	}
	else if((f = frame_of_win(ev->window)))
		write_event("ClientClick 0x%x %d\n", f->client->win, ev->button);
}

static void
buttonpress(XEvent *e) {
	XButtonPressedEvent *ev;
	Frame *f;

	ev = &e->xbutton;
	if((f = frame_of_win(ev->window))) {
		if((ev->state & def.mod) == def.mod) {
			switch(ev->button) {
			case Button1:
				do_mouse_resize(f->client, False, CENTER);
				focus(f->client, True);
				frame_to_top(f);
				focus(f->client, True);
				break;
			case Button3:
				do_mouse_resize(f->client, False,
						quadrant(&f->rect, ev->x_root, ev->y_root));
				frame_to_top(f);
				focus(f->client, True);
				break;
			default: break;
				XAllowEvents(blz.dpy, ReplayPointer, ev->time);
			}
		}else{
			if(ev->button == Button1) {
				if(frame_to_top(f))
					restack_view(f->view);

				if(ptinrect(ev->x, ev->y, &f->grabbox))
					do_mouse_resize(f->client, True, CENTER);
				else if(!ev->subwindow && !ptinrect(ev->x, ev->y, &f->titlebar))
					do_mouse_resize(f->client, False, quadrant(&f->rect, ev->x_root, ev->y_root));

				if(f->client != sel_client())
					focus(f->client, True);
			}
			if(ev->subwindow)
				XAllowEvents(blz.dpy, ReplayPointer, ev->time);
			else {
				/* Ungrab so a menu can receive events before the button is released */
				XUngrabPointer(blz.dpy, ev->time);
				XSync(blz.dpy, False);

				write_event("ClientMouseDown 0x%x %d\n", f->client->win, ev->button);
			}
		}
	}else
		XAllowEvents(blz.dpy, ReplayPointer, ev->time);
}

static void
configurerequest(XEvent *e) {
	XConfigureRequestEvent *ev;
	XWindowChanges wc;
	XRectangle *frect;
	Client *c;
	Frame *f;

	ev = &e->xconfigurerequest;
	c = client_of_win(ev->window);
	if(c) {
		f = c->sel;
		gravitate_client(c, True);
		if(ev->value_mask & CWX)
			c->rect.x = ev->x;
		if(ev->value_mask & CWY)
			c->rect.y = ev->y;
		if(ev->value_mask & CWWidth)
			c->rect.width = ev->width;
		if(ev->value_mask & CWHeight)
			c->rect.height = ev->height;
		if(ev->value_mask & CWBorderWidth)
			c->border = ev->border_width;
		gravitate_client(c, False);

		if((c->rect.height == screen->rect.height)
		&& (c->rect.width == screen->rect.width)) {
			c->fullscreen = True;
			if(c->sel) {
				if(!c->sel->area->floating)
					send_to_area(c->sel->view->area, c->sel);
				focus_client(c);
				restack_view(c->sel->view);
			}
		}

		if(c->sel->area->floating)
			frect=&c->sel->rect;
		else
			frect=&c->sel->revert;

		*frect = c->rect;
		frect->y -= labelh(&def.font);
		frect->x -= def.border;
		frect->width += 2 * def.border;
		frect->height += frame_delta_h();

		if(c->sel->area->floating || c->fullscreen)
			resize_client(c, frect);
		else
			configure_client(c);
	}else{
		wc.x = ev->x;
		wc.y = ev->y;
		wc.width = ev->width;
		wc.height = ev->height;
		wc.border_width = ev->border_width;
		wc.sibling = ev->above;
		wc.stack_mode = ev->detail;
		ev->value_mask &= ~(CWStackMode|CWSibling);
		XConfigureWindow(blz.dpy, ev->window, ev->value_mask, &wc);
		XSync(blz.dpy, False);
	}
}

static void
destroynotify(XEvent *e) {
	XDestroyWindowEvent *ev;
	Client *c;

	ev = &e->xdestroywindow;
	if((c = client_of_win(ev->window)))
		destroy_client(c);
}

static void
enternotify(XEvent *e) {
	XCrossingEvent *ev;
	Client *c;
	Frame *f;

	ev = &e->xcrossing;
	if(ev->mode != NotifyNormal)
		return;

	if((c = client_of_win(ev->window))) {
		if(ev->detail != NotifyInferior) {
			if(screen->focus != c) {
				if(verbose) fprintf(stderr, "enter_notify(c) => %s\n", c->name);
				focus(c, False);
			}
			set_cursor(c, cursor[CurNormal]);
		}else if(verbose) fprintf(stderr, "enter_notify(c[NotifyInferior]) => %s\n", c->name);
	}
	else if((f = frame_of_win(ev->window))) {
		if(screen->focus != c) {
			if(verbose) fprintf(stderr, "enter_notify(f) => %s\n", f->client->name);
			if(f->area->floating || !f->collapsed)
				focus(f->client, False);
		}
		set_frame_cursor(f, ev->x, ev->y);
	}
	else if(ev->window == blz.root) {
		sel_screen = True;
		draw_frames();
	}
}

static void
leavenotify(XEvent *e) {
	XCrossingEvent *ev;

	ev = &e->xcrossing;
	if((ev->window == blz.root) && !ev->same_screen) {
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
	Client *c, *old;
	XEvent me;

	ev = &e->xfocus;
	/* Yes, we're focusing in on nothing, here. */
	if(ev->detail == NotifyDetailNone) {
		XSetInputFocus(blz.dpy, screen->barwin, RevertToParent, CurrentTime);
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

	old = screen->focus;
	c = client_of_win(ev->window);
	if(c) {
		print_focus(c, c->name);
		if(ev->mode == NotifyGrab)
			screen->hasgrab = c;
		screen->focus = c;
		if(c != old) {
			update_client_grab(c);
			if(c->sel)
				draw_frame(c->sel);
			if(old && old->sel)
				draw_frame(old->sel);
		}
	}else if(ev->window == screen->barwin) {
		print_focus(nil, "<nil>");
		screen->focus = nil;
	}else if(ev->mode == NotifyGrab) {
		if(ev->window == blz.root)
			if(XCheckMaskEvent(blz.dpy, KeyPressMask, &me)) {
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
	Client *c;

	ev = &e->xfocus;
	if(!((ev->detail == NotifyNonlinear)
	   ||(ev->detail == NotifyNonlinearVirtual)))
		return;
	if(ev->mode == NotifyUngrab)
		screen->hasgrab = nil;

	c = client_of_win(ev->window);
	if(c) {
		if((ev->mode == NotifyWhileGrabbed)
		&& (screen->hasgrab != &c_root)) {
			if((screen->focus)
			&& (screen->hasgrab != screen->focus))
				screen->hasgrab = screen->focus;
			if(screen->hasgrab == c)
				return;
		}else if(ev->mode != NotifyGrab) {
			if(screen->focus == c) {
				print_focus(&c_magic, "<magic>");
				screen->focus = &c_magic;
			}
			update_client_grab(c);
			if(c->sel)
				draw_frame(c->sel);
		}
	}
}

static void
expose(XEvent *e) {
	XExposeEvent *ev;
	static Frame *f;

	ev = &e->xexpose;
	if(ev->count == 0) {
		if(ev->window == screen->barwin)
			draw_bar(screen);
		else if((f = frame_of_win(ev->window)))
			draw_frame(f);
	}
}

static void
keypress(XEvent *e) {
	XKeyEvent *ev;

	ev = &e->xkey;
	ev->state &= valid_mask;
	if(ev->window == blz.root)
		kpress(blz.root, ev->state, (KeyCode) ev->keycode);
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
	static XWindowAttributes wa;

	ev = &e->xmaprequest;
	if(!XGetWindowAttributes(blz.dpy, ev->window, &wa))
		return;
	if(wa.override_redirect) {
		XSelectInput(blz.dpy, ev->window,
				(StructureNotifyMask | PropertyChangeMask));
		return;
	}
	if(!client_of_win(ev->window))
		manage_client(create_client(ev->window, &wa));
}

static void
motionnotify(XEvent *e) {
	XMotionEvent *ev;
	Frame *f;

	ev = &e->xmotion;
	if((f = frame_of_win(ev->window)))
		set_frame_cursor(f, ev->x, ev->y);
}

static void
propertynotify(XEvent *e) {
	XPropertyEvent *ev;
	Client *c;

	ev = &e->xproperty;
	if(ev->state == PropertyDelete)
		return; /* ignore */
	if((c = client_of_win(ev->window)))
		prop_client(c, ev->atom);
}

static void
mapnotify(XEvent *e) {
	XMapEvent *ev;
	Client *c;

	ev = &e->xmap;
	if((c = client_of_win(ev->window)))
		if(c == sel_client())
			focus_client(c);
}

static void
unmapnotify(XEvent *e) {
	XUnmapEvent *ev;
	Client *c;

	ev = &e->xunmap;
	if((c = client_of_win(ev->window)))
		if(ev->send_event || (c->unmapped-- == 0))
			destroy_client(c);
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
check_x_event(IXPConn *c) {
	XEvent ev;
	while(XPending(blz.dpy)) {
		XNextEvent(blz.dpy, &ev);
		if(verbose)
			printevent(&ev);
		dispatch_event(&ev);
		XPending(blz.dpy);
	}
}
