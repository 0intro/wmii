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
static void handle_configurerequest(XEvent * e);
static void handle_destroynotify(XEvent * e);
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
		handler[i] = 0;
	handler[ButtonPress] = handle_buttonpress;
	handler[ConfigureRequest] = handle_configurerequest;
	handler[DestroyNotify] = handle_destroynotify;
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
	while(XPending(dpy)) { /* main evet loop */
		XNextEvent(dpy, &ev);
		if(handler[ev.type])
			(handler[ev.type]) (&ev); /* call handler */
	}
}

static void
handle_buttonpress(XEvent *e)
{
	Client *c;
	XButtonPressedEvent *ev = &e->xbutton;
	static char buf[32];
	if(ev->window == barwin) {
		unsigned int i;
		for(i = 0; i < nlabel; i++)
			if(blitz_ispointinrect(ev->x, ev->y, &label[i]->rect)) {
				snprintf(buf, sizeof(buf), "LabelClick %s %d\n",
						label[i]->name, ev->button);
				write_event(buf, True);
				return;
			}
	}
	else if((c = win2clientframe(ev->window))) {
		if(ev->button == Button1) {
			if(sel_client() != c) {
				focus(c);
				return;
			}
			else
				mouse_move(c);
		}
		if(c->nframe) {
			snprintf(buf, sizeof(buf), "ClientClick %d %d\n",
					frame2index(c->frame[c->sel]) + 1, ev->button);
			write_event(buf, True);
		}
	}
	else if((c = win2client(ev->window))) {
		ev->state &= valid_mask;
		if(ev->state & Mod1Mask) {
			focus(c);
			switch (ev->button) {
			case Button1:
				mouse_move(c);
				break;
			case Button3:
				{
					Align align = xy2align(&c->rect, ev->x, ev->y);
					if(align == CENTER)
						mouse_move(c);
					else
						mouse_resize(c, align);
				}
				break;
			}
		}
		else if(ev->button == Button1)
			focus(c);

		if(c->nframe) {
			snprintf(buf, sizeof(buf), "ClientClick %d %d\n",
					frame2index(c->frame[c->sel]) + 1, ev->button);
			write_event(buf, True);
		}
	}
	
}

static void
handle_configurerequest(XEvent *e)
{
	XConfigureRequestEvent *ev = &e->xconfigurerequest;
	XWindowChanges wc;
	Client *c;

	c = win2client(ev->window);
	ev->value_mask &= ~CWSibling;

	if(c) {
		gravitate(c, True);

		if(c->frame && (area2index(c->frame[c->sel]->area) == 0)) {
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

		gravitate(c, False);

		if(c->nframe) {
			Frame *f = c->frame[c->sel];
			f->rect.x = wc.x = c->rect.x - def.border;
			f->rect.y = wc.y = c->rect.y - bar_height();
			f->rect.width = wc.width = c->rect.width + 2 * def.border;
			f->rect.height = wc.height = c->rect.height + def.border
				+ bar_height();
			wc.border_width = 1;
			wc.sibling = None;
			wc.stack_mode = ev->detail;
			if(f->area->view != view[sel])
				f->rect.x += 2 * rect.width;
			XConfigureWindow(dpy, c->framewin, ev->value_mask, &wc);
			configure_client(c);
		}
	}

	wc.x = ev->x;
	wc.y = ev->y;
	wc.width = ev->width;
	wc.height = ev->height;

	if(c && c->frame) {
		wc.x = def.border;
		wc.y = bar_height();
		if(area2index(c->frame[c->sel]->area) > 0) {
			wc.width = c->rect.width;
			wc.height = c->rect.height;
		}
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

	if((c = win2client(ev->window)))
		destroy_client(c);
}

static void
handle_expose(XEvent *e)
{
	XExposeEvent *ev = &e->xexpose;
	static Client *c;
	if(ev->count == 0) {
		if(ev->window == barwin)
			draw_bar();
		else if((c = win2clientframe(ev->window)))
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

	/* there're client which send map requests twice */
	if(!win2client(ev->window))
		manage_client(alloc_client(ev->window, &wa));
}

static void
handle_propertynotify(XEvent *e)
{
	XPropertyEvent *ev = &e->xproperty;
	Client *c;

	if(ev->state == PropertyDelete)
		return; /* ignore */

	if((c = win2client(ev->window)))
		update_client_property(c, ev);
}

static void
handle_unmapnotify(XEvent *e)
{
	Client *c;
	XUnmapEvent *ev = &e->xunmap;

	if((c = win2client(ev->window)))
		destroy_client(c);
}
