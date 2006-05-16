/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <X11/keysym.h>

#include "wm.h"

/* local functions */
static void handle_buttonpress(XEvent * e);
static void handle_buttonrelease(XEvent * e);
static void handle_configurerequest(XEvent * e);
static void handle_destroynotify(XEvent * e);
static void handle_enternotify(XEvent * e);
static void handle_expose(XEvent * e);
static void handle_keypress(XEvent * e);
static void handle_keymapnotify(XEvent * e);
static void handle_maprequest(XEvent * e);
static void handle_propertynotify(XEvent * e);
static void handle_unmapnotify(XEvent * e);

void (*handler[LASTEvent]) (XEvent *);

void
init_x_event_handler()
{
	int i;
	/* init event handler */
	for(i = 0; i < LASTEvent; i++)
		handler[i] = nil;
	handler[ButtonPress] = handle_buttonpress;
	handler[ButtonRelease] = handle_buttonrelease;
	handler[ConfigureRequest] = handle_configurerequest;
	handler[DestroyNotify] = handle_destroynotify;
	handler[EnterNotify] = handle_enternotify;
	handler[Expose] = handle_expose;
	handler[KeyPress] = handle_keypress;
	handler[KeymapNotify] = handle_keymapnotify;
	handler[MapRequest] = handle_maprequest;
	handler[PropertyNotify] = handle_propertynotify;
	handler[UnmapNotify] = handle_unmapnotify;
}

void
check_x_event(IXPConn *c)
{
	XEvent ev;
	while(XPending(dpy)) { /* main event loop */
		XNextEvent(dpy, &ev);
		if(handler[ev.type])
			(handler[ev.type]) (&ev); /* call handler */
	}
}

unsigned int
flush_masked_events(long even_mask)
{
	XEvent ev;
	unsigned int n = 0;
	while(XCheckMaskEvent(dpy, even_mask, &ev)) n++;
	return n;
}

static void
handle_buttonrelease(XEvent *e)
{
	Client *c;
	XButtonPressedEvent *ev = &e->xbutton;
	static char buf[32];
	if(ev->window == barwin) {
		unsigned int i;
		for(i = 0; i < bar.size; i++)
			if(blitz_ispointinrect(ev->x, ev->y, &bar.data[i]->rect)) {
				snprintf(buf, sizeof(buf), "BarClick %s %d\n",
						bar.data[i]->name, ev->button);
				write_event(buf);
				return;
			}
	}
	else if((c = frame_of_win(ev->window)) && c->frame.size) {
		snprintf(buf, sizeof(buf), "ClientClick %d %d\n",
				idx_of_client_id(c->id), ev->button);
		write_event(buf);
	}
}

static void
handle_buttonpress(XEvent *e)
{
	Client *c;
	XButtonPressedEvent *ev = &e->xbutton;

	if((c = frame_of_win(ev->window))) {
		ev->state &= valid_mask;
		if(ev->state & def.mod) {
			focus(c, True);
			switch(ev->button) {
			case Button1:
				do_mouse_move(c, False);
				break;
			case Button2:
				if(idx_of_area(c->frame.data[c->sel]->area))
					do_mouse_move(c, True);
				break;
			case Button3:
				{
					BlitzAlign align = blitz_align_of_rect(&c->rect, ev->x, ev->y);
					if(align != CENTER)
						do_mouse_resize(c, align);
					else
						do_mouse_move(c, False);
				}
			default:
			break;
			}
		}
		else if(ev->button == Button1)
			focus(c, True);
	}
}

static void
handle_configurerequest(XEvent *e)
{
	XConfigureRequestEvent *ev = &e->xconfigurerequest;
	XWindowChanges wc;
	Client *c;

	c = client_of_win(ev->window);
	ev->value_mask &= ~CWSibling;
	if(c) {
		if(c->frame.size && !idx_of_area(c->frame.data[c->sel]->area)) {
			gravitate_client(c, True);

			if(c->frame.size) {
				if(ev->value_mask & CWX)
					c->rect.x = ev->x;
				if(ev->value_mask & CWY)
					c->rect.y = ev->y;
				if(ev->value_mask & CWWidth)
					c->rect.width = ev->width;
				if(ev->value_mask & CWHeight)
					c->rect.height = ev->height;
			}
			if(ev->value_mask & CWBorderWidth)
				c->border = ev->border_width;

			gravitate_client(c, False);

			if(c->frame.size) {
				Frame *f = c->frame.data[c->sel];
				if(c->rect.width >= rect.width && c->rect.height >= rect.height) {
					f->rect.y = wc.y = -height_of_bar();
					f->rect.x = wc.x = -def.border;
				}
				else {
					f->rect.x = wc.x = c->rect.x - def.border;
					f->rect.y = wc.y = c->rect.y - height_of_bar();
				}
				f->rect.width = wc.width = c->rect.width + 2 * def.border;
				f->rect.height = wc.height = c->rect.height + def.border
					+ height_of_bar();
				wc.border_width = 1;
				wc.sibling = None;
				wc.stack_mode = ev->detail;
				if(f->area->view != view.data[sel])
					wc.x += 2 * rect.width;
				XConfigureWindow(dpy, c->framewin, ev->value_mask, &wc);
				configure_client(c);
			}
		}
	}

	wc.x = ev->x;
	wc.y = ev->y;
	wc.width = ev->width;
	wc.height = ev->height;

	if(c && c->frame.size) {
		wc.x = def.border;
		wc.y = height_of_bar();
		wc.width = c->rect.width;
		wc.height = c->rect.height;
	}

	wc.border_width = 0;
	wc.sibling = None;
	wc.stack_mode = Above;
	ev->value_mask &= ~CWStackMode;
	ev->value_mask |= CWBorderWidth;
	XConfigureWindow(dpy, ev->window, ev->value_mask, &wc);
	XSync(dpy, False);
}

static void
handle_destroynotify(XEvent *e)
{
	Client *c;
	XDestroyWindowEvent *ev = &e->xdestroywindow;

	if((c = client_of_win(ev->window)))
		destroy_client(c);
}

static void
handle_enternotify(XEvent *e)
{
	XCrossingEvent *ev = &e->xcrossing;
	Client *c;

	if(ev->mode != NotifyNormal || ev->detail == NotifyInferior)
		return;

	if((c = client_of_win(ev->window))) {
		Client *old = sel_client_of_view(view.data[sel]);
		Frame *f = c->frame.data[c->sel];
		Area *a = f->area;
		if(a->mode == Colmax)
			c = a->frame.data[a->sel]->client;
		if(c != old)
			focus(c, False);
	}
}

static void
handle_expose(XEvent *e)
{
	XExposeEvent *ev = &e->xexpose;
	static Client *c;
	if(ev->count == 0) {
		if(ev->window == barwin)
			draw_bar();
		else if((c = frame_of_win(ev->window)))
			draw_client(c);
	}
}

static void
handle_keypress(XEvent *e)
{
	XKeyEvent *ev = &e->xkey;
	ev->state &= valid_mask;
	handle_key(root, ev->state, (KeyCode) ev->keycode);
}

static void
handle_keymapnotify(XEvent *e)
{
	update_keys();
}

static void
handle_maprequest(XEvent *e)
{
	XMapRequestEvent *ev = &e->xmaprequest;
	static XWindowAttributes wa;

	if(!XGetWindowAttributes(dpy, ev->window, &wa))
		return;

	if(wa.override_redirect) {
		XSelectInput(dpy, ev->window,
				(StructureNotifyMask | PropertyChangeMask));
		return;
	}

	if(!client_of_win(ev->window))
		manage_client(create_client(ev->window, &wa));
}

static void
handle_propertynotify(XEvent *e)
{
	XPropertyEvent *ev = &e->xproperty;
	Client *c;

	if(ev->state == PropertyDelete)
		return; /* ignore */

	if((c = client_of_win(ev->window)))
		prop_client(c, ev);
}

static void
handle_unmapnotify(XEvent *e)
{
	Client *c;
	XUnmapEvent *ev = &e->xunmap;

	if((c = client_of_win(ev->window)))
		destroy_client(c);
}
