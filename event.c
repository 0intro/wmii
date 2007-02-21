/* (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */
#include "wmii.h"
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <X11/keysym.h>
#include "printevent.h"

uint
flush_masked_events(long even_mask) {
	XEvent ev;
	uint n = 0;
	while(XCheckMaskEvent(blz.dpy, even_mask, &ev)) n++;
	return n;
}

static void
buttonrelease(XEvent *e) {
	Frame *f;
	Bar *b;
	XButtonPressedEvent *ev = &e->xbutton;
	if(ev->window == screen->barwin) {
		for(b=screen->lbar; b; b=b->next)
			if(ispointinrect(ev->x, ev->y, &b->brush.rect)) {
				write_event("LeftBarClick %d %s\n",
						ev->button, b->name);
				return;
			}
		for(b=screen->rbar; b; b=b->next)
			if(ispointinrect(ev->x, ev->y, &b->brush.rect)) {
				write_event("RightBarClick %d %s\n",
						ev->button, b->name);
				return;
			}
	}
	else if((f = frame_of_win(ev->window)))
		write_event("ClientClick 0x%x %d\n", f->client->win, ev->button);
}

static void
buttonpress(XEvent *e) {
	Frame *f;
	Bool inclient;
	XButtonPressedEvent *ev;
	
	ev = &e->xbutton;
	if((f = frame_of_win(ev->window))) {
		inclient = (ev->subwindow == f->client->win);
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
						quadofcoord(&f->rect, ev->x_root, ev->y_root));
				frame_to_top(f);
				focus(f->client, True);
				break;
			default: break;
				XAllowEvents(blz.dpy, ReplayPointer, CurrentTime);
			}
		}else{
			if(ev->button == Button1) {
				if(frame_to_top(f))
					restack_view(f->view);
				if(ispointinrect(ev->x, ev->y, &f->grabbox))
					do_mouse_resize(f->client, True, CENTER);
				else if(!ev->subwindow
				&& !ispointinrect(ev->x, ev->y, &f->titlebar))
					do_mouse_resize(f->client, False,
						quadofcoord(&f->rect, ev->x_root, ev->y_root));
				if(f->client != sel_client())
					focus(f->client, True);
			}
			XAllowEvents(blz.dpy, ReplayPointer, CurrentTime);
		}
	}else
		XAllowEvents(blz.dpy, ReplayPointer, CurrentTime);
}

static void
configurerequest(XEvent *e) {
	XConfigureRequestEvent *ev = &e->xconfigurerequest;
	XWindowChanges wc;
	XRectangle *frect;
	Client *c;
	Frame *f;

	c = client_of_win(ev->window);

	if(c) {
		f = c->sel;
		if(0 && verbose)
			fprintf(stderr, "Configure: %s\n\ta: x=%d y=%d w=%d h=%d\n",
				c->name, c->rect.x, c->rect.y, c->rect.width, c->rect.height);
		gravitate_client(c, True);
		if(0 && verbose)
			fprintf(stderr, "\tb: x=%d y=%d w=%d h=%d\n",
				c->rect.x, c->rect.y, c->rect.width, c->rect.height);
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
		if(0 && verbose)
			fprintf(stderr, "\tb: x=%d y=%d w=%d h=%d\n",
				c->rect.x, c->rect.y, c->rect.width, c->rect.height);
		gravitate_client(c, False);
		if(0 && verbose)
			fprintf(stderr, "\tb: x=%d y=%d w=%d h=%d\n",
				c->rect.x, c->rect.y, c->rect.width, c->rect.height);

		if(c->sel->area->floating)
			frect=&c->sel->rect;
		else
			frect=&c->sel->revert;

		*frect = c->rect;
		frect->y -= labelh(&def.font);
		frect->x -= def.border;
		frect->width += 2 * def.border;
		frect->height += frame_delta_h();
		if(0 && verbose)
			fprintf(stderr, "\tb: x=%d y=%d w=%d h=%d\n",
				frect->x, frect->y, frect->width, frect->height);

		if(c->sel->area->floating)
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
	Client *c;
	XDestroyWindowEvent *ev = &e->xdestroywindow;

	if((c = client_of_win(ev->window)))
		destroy_client(c);
}

static void
enternotify(XEvent *e) {
	XCrossingEvent *ev = &e->xcrossing;
	Client *c;

	if(ev->mode != NotifyNormal || ev->detail == NotifyInferior)
		return;
	if((c = client_of_win(ev->window))) {
		if(c->sel->area->mode == Colmax)
			c = c->sel->area->sel->client;
		focus(c, False);
	}
	else if(ev->window == blz.root) {
		sel_screen = True;
		draw_frames();
	}
}

static void
leavenotify(XEvent *e) {
	XCrossingEvent *ev = &e->xcrossing;

	if((ev->window == blz.root) && !ev->same_screen) {
		sel_screen = True;
		draw_frames();
	}
}

static void
expose(XEvent *e) {
	XExposeEvent *ev = &e->xexpose;
	static Frame *f;

	if(ev->count == 0) {
		if(ev->window == screen->barwin)
			draw_bar(screen);
		else if((f = frame_of_win(ev->window)) && f->view == screen->sel)
			draw_frame(f);
	}
}

static void
keypress(XEvent *e) {
	XKeyEvent *ev = &e->xkey;
	KeySym k = 0;
	char buf[32];
	int n;
	static Frame *f;


	ev->state &= valid_mask;
	if((f = frame_of_win(ev->window))) {
		buf[0] = 0;
		n = XLookupString(ev, buf, sizeof(buf), &k, 0);
		if(IsFunctionKey(k) || IsKeypadKey(k) || IsMiscFunctionKey(k)
				|| IsPFKey(k) || IsPrivateKeypadKey(k))
			return;
		buf[n] = 0;
	}
	else
		kpress(blz.root, ev->state, (KeyCode) ev->keycode);
}

static void
mappingnotify(XEvent *e) {
	XMappingEvent *ev = &e->xmapping;

	XRefreshKeyboardMapping(ev);
	if(ev->request == MappingKeyboard)
		update_keys();
}

static void
maprequest(XEvent *e) {
	XMapRequestEvent *ev = &e->xmaprequest;
	static XWindowAttributes wa;

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
	XMotionEvent *ev = &e->xmotion;
	Cursor cur;
	Frame *f;
	
	if((f = frame_of_win(ev->window))) {
		if(!ispointinrect(ev->x, ev->y, &f->titlebar)) {
			cur = cursor_of_quad(quadofcoord(&f->rect, ev->x_root, ev->y_root));
			set_cursor(f->client, cur);
		}else
			set_cursor(f->client, cursor[CurNormal]);
	}
}

static void
propertynotify(XEvent *e) {
	XPropertyEvent *ev = &e->xproperty;
	Client *c;

	if(ev->state == PropertyDelete)
		return; /* ignore */
	if((c = client_of_win(ev->window)))
		prop_client(c, ev);
}

static void
unmapnotify(XEvent *e) {
	Client *c;
	XUnmapEvent *ev = &e->xunmap;

	if((c = client_of_win(ev->window)))
		if(!c->unmapped--)
			destroy_client(c);
}

static void
focusin(XEvent *e) {
	Client *c, *old;
	XFocusChangeEvent *ev = &e->xfocus;

	/* Yes, we're focusing in on nothing, here. */
	if(ev->detail == NotifyDetailNone) {
		XSetInputFocus(blz.dpy, screen->barwin, RevertToParent, CurrentTime);
		return;
	}

	if(!((ev->detail == NotifyNonlinear)
	   ||(ev->detail == NotifyNonlinearVirtual)
	   ||(ev->detail == NotifyInferior)
	   ||(ev->detail == NotifyAncestor)))
		return;
	if(ev->mode == NotifyWhileGrabbed)
		return;

	c = client_of_win(ev->window);
	old = screen->focus;
	if(c) {
		if(verbose) {
			fprintf(stderr, "screen->focus: %p => %p\n", screen->focus, c);
			fprintf(stderr, "\t%s => %s\n", (screen->focus ? screen->focus->name : nil),
					c->name);
		}
		if(ev->mode == NotifyGrab)
			screen->hasgrab = c;
		screen->focus = c;
		update_client_grab(c);
		if(c->sel)
			draw_frame(c->sel);
		if(old && old->sel)
			draw_frame(old->sel);
	}else if(ev->window == screen->barwin) {
		if(verbose) {
			fprintf(stderr, "screen->focus: %p => %p\n", screen->focus, c);
			fprintf(stderr, "\t%s => %s\n", (screen->focus ? screen->focus->name : nil),
					"<nil>");
		}
		screen->focus = nil;
	}else if(ev->mode == NotifyGrab) {
		c = screen->focus;
		if(c) {
			screen->focus = nil;
			if(c->sel)
				draw_frame(c->sel);
		}
	}
}

static void
focusout(XEvent *e) {
	Client *c;
	XFocusChangeEvent *ev = &e->xfocus;

	if(!((ev->detail == NotifyNonlinear)
	   ||(ev->detail == NotifyNonlinearVirtual)))
		return;
	if(ev->mode == NotifyUngrab)
		screen->hasgrab = nil;

	c = client_of_win(ev->window);
	if(c) {
		if(ev->mode == NotifyWhileGrabbed) {
			if(screen->focus && screen->hasgrab != screen->focus)
				screen->hasgrab = screen->focus;
			if(screen->hasgrab == c)
				return;
		}
		if(screen->focus == c)
			screen->focus = nil;
		update_client_grab(c);
		if(c->sel)
			draw_frame(c->sel);
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
	[MapRequest]	= maprequest,
	[MappingNotify]	= mappingnotify,
	[MotionNotify]	= motionnotify,
	[PropertyNotify]= propertynotify,
	[UnmapNotify]	= unmapnotify,

};

void
check_x_event(IXPConn *c) {
	XEvent ev;
	while(XPending(blz.dpy)) { /* main event loop */
		XNextEvent(blz.dpy, &ev);
		if(verbose)
			printevent(&ev);
		if(handler[ev.type])
			(handler[ev.type]) (&ev); /* call handler */
	}
}
