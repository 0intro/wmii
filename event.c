/* (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */
#include "wmii.h"
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <X11/keysym.h>

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
						quadofcoord(&f->client->rect, ev->x, ev->y));
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
				if(ispointinrect(ev->x, ev->y, &f->grabbox.rect))
					do_mouse_resize(f->client, True, CENTER);
				else if(!ispointinrect(ev->x, ev->y, &f->titlebar.rect))
					do_mouse_resize(f->client, False,
						quadofcoord(&f->client->rect, ev->x, ev->y));
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

	c = client_of_win(ev->window);
	wc.x = ev->x;
	wc.y = ev->y;
	wc.width = ev->width;
	wc.height = ev->height;
	wc.border_width = 0;
	wc.sibling = None;
	wc.stack_mode = Above;

	ev->value_mask &= ~CWSibling;
	if(c) {
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
		if(c->frame) {
			if(c->sel->area->floating)
				frect=&c->sel->rect;
			else
				frect=&c->sel->revert;

			frect->y = -labelh(&def.font);
			frect->x = -def.border;
			frect->width = c->rect.width + 2 * def.border;
			frect->height = c->rect.height + def.border + labelh(&def.font);

			wc.border_width = 1;
			wc.stack_mode = ev->detail;
			if(c->sel->area->floating)
				resize_client(c, frect);
			wc.x = frect->x + labelh(&def.font);
			wc.y = frect->y + def.border;
			wc.width = frect->width - 2 * def.border;
			wc.height = frect->height - def.border - labelh(&def.font);
		}
	}

	ev->value_mask &= ~CWStackMode;
	ev->value_mask |= CWBorderWidth;
	XConfigureWindow(blz.dpy, ev->window, ev->value_mask, &wc);
	XSync(blz.dpy, False);
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

void (*handler[LASTEvent]) (XEvent *) = {
	[ButtonPress]	= buttonpress,
	[ButtonRelease]	= buttonrelease,
	[ConfigureRequest]= configurerequest,
	[DestroyNotify]	= destroynotify,
	[EnterNotify]	= enternotify,
	[LeaveNotify]	= leavenotify,
	[Expose]	= expose,
	[KeyPress]	= keypress,
	[MappingNotify]	= mappingnotify,
	[MapRequest]	= maprequest,
	[PropertyNotify]= propertynotify,
	[UnmapNotify]	= unmapnotify
};

void
check_x_event(IXPConn *c) {
	XEvent ev;
	while(XPending(blz.dpy)) { /* main event loop */
		XNextEvent(blz.dpy, &ev);
		if(handler[ev.type])
			(handler[ev.type]) (&ev); /* call handler */
	}
}
