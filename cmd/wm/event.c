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
static void handle_buttonpress(XEvent *e);
static void handle_buttonrelease(XEvent *e);
static void handle_configurerequest(XEvent *e);
static void handle_destroynotify(XEvent *e);
static void handle_enternotify(XEvent *e);
static void handle_focusin(XEvent *e);
static void handle_focusout(XEvent *e);
static void handle_leavenotify(XEvent *e);
static void handle_expose(XEvent *e);
static void handle_keypress(XEvent *e);
static void handle_keymapnotify(XEvent *e);
static void handle_maprequest(XEvent *e);
static void handle_motionnotify(XEvent *e);
static void handle_propertynotify(XEvent *e);
static void handle_unmapnotify(XEvent *e);

void (*handler[LASTEvent]) (XEvent *) = {
	[ButtonPress]	= handle_buttonpress,
	[ButtonRelease]	= handle_buttonrelease,
	[ConfigureRequest]= handle_configurerequest,
	[DestroyNotify]	= handle_destroynotify,
	[EnterNotify]	= handle_enternotify,
	[FocusIn]		= handle_focusin,
	[FocusOut]		= handle_focusout,
	[LeaveNotify]	= handle_leavenotify,
	[Expose]	= handle_expose,
	[KeyPress]	= handle_keypress,
	[KeymapNotify]	= handle_keymapnotify,
	[MotionNotify]	= handle_motionnotify,
	[MapRequest]	= handle_maprequest,
	[PropertyNotify]= handle_propertynotify,
	[UnmapNotify]	= handle_unmapnotify
};

void
check_x_event(IXPConn *c)
{
	XEvent ev;
	while(XPending(blz.display)) { /* main event loop */
		XNextEvent(blz.display, &ev);
		if(handler[ev.type])
			(handler[ev.type]) (&ev); /* call handler */
	}
}

unsigned int
flush_masked_events(long even_mask)
{
	XEvent ev;
	unsigned int n = 0;
	while(XCheckMaskEvent(blz.display, even_mask, &ev)) n++;
	return n;
}

static void
handle_buttonrelease(XEvent *e)
{
	Frame *f;
	Bar *b;
	XButtonPressedEvent *ev = &e->xbutton;
	if(ev->window == screen->barwin) {
		for(b=screen->lbar; b; b=b->next)
			if(blitz_ispointinrect(ev->x, ev->y, &b->brush.rect))
				return write_event("LeftBarClick %d %s\n",
						ev->button, b->name);
		for(b=screen->rbar; b; b=b->next)
			if(blitz_ispointinrect(ev->x, ev->y, &b->brush.rect))
				return write_event("RightBarClick %d %s\n",
						ev->button, b->name);
	}
	else if((f = frame_of_win(ev->window))) {
		if(blitz_brelease_input(&f->tagbar, ev->x, ev->y))
			draw_frame(f);
		write_event("ClientClick %d %d\n", idx_of_client(f->client), ev->button);
	}
}

static void
handle_focusin(XEvent *e)
{
	Frame *f;
	XFocusChangeEvent *ev = &e->xfocus;
	if((f = frame_of_win(ev->window)))
		blitz_focusin_input(&f->tagbar);
}

static void
handle_focusout(XEvent *e)
{
	Frame *f;
	XFocusChangeEvent *ev = &e->xfocus;
	if((f = frame_of_win(ev->window)))
		blitz_focusout_input(&f->tagbar);
}

static void
handle_motionnotify(XEvent *e)
{
	Frame *f;
	XMotionEvent *ev = &e->xmotion;
	if((f = frame_of_win(ev->window))) {
		if(blitz_bmotion_input(&f->tagbar, ev->x, ev->y))
			draw_frame(f);
	}
}

static void
handle_buttonpress(XEvent *e)
{
	Frame *f;
	XButtonPressedEvent *ev = &e->xbutton;

	if((f = frame_of_win(ev->window))) {
		ev->state &= valid_mask;
		if(blitz_bpress_input(&f->tagbar, ev->x, ev->y))
			draw_frame(f);
		if((ev->state & def.mod) == def.mod) {
			focus(f->client, True);
			switch(ev->button) {
			case Button1:
				do_mouse_resize(f->client, CENTER);
				break;
			case Button3:
				do_mouse_resize(f->client, quadofcoord(&f->client->rect, ev->x, ev->y));
			default:
			break;
			}
		}
		else if(ev->button == Button1)
			focus(f->client, True);
	}
}

static void
handle_configurerequest(XEvent *e)
{
	XConfigureRequestEvent *ev = &e->xconfigurerequest;
	XWindowChanges wc;
	XRectangle *frect;
	Client *c;

	c = client_of_win(ev->window);
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

			if(c->rect.width >= screen->rect.width && c->rect.height >= screen->rect.height) {
				frect->y = wc.y = -height_of_bar();
				frect->x = wc.x = -def.border;
			}
			else {
				frect->y = wc.y = c->rect.y - height_of_bar();
				frect->x = wc.x = c->rect.x - def.border;
			}
			frect->width = wc.width = c->rect.width + 2 * def.border;
			frect->height = wc.height = c->rect.height + def.border
				+ height_of_bar();
			wc.border_width = 1;
			wc.sibling = None;
			wc.stack_mode = ev->detail;
			if(c->sel->area->view != screen->sel)
				wc.x += 2 * screen->rect.width;
			if(c->sel->area->floating) {
				XConfigureWindow(blz.display, c->framewin, ev->value_mask, &wc);
				configure_client(c);
			}
		}
	}

	wc.x = ev->x;
	wc.y = ev->y;
	wc.width = ev->width;
	wc.height = ev->height;

	if(c && c->frame) {
		wc.x = def.border;
		wc.y = height_of_bar();
		wc.width = c->sel->rect.width - 2 * def.border;
		wc.height = c->sel->rect.height - def.border - height_of_bar();
	}

	wc.border_width = 0;
	wc.sibling = None;
	wc.stack_mode = Above;
	ev->value_mask &= ~CWStackMode;
	ev->value_mask |= CWBorderWidth;
	XConfigureWindow(blz.display, ev->window, ev->value_mask, &wc);

	XSync(blz.display, False);
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
		Frame *f = c->sel;
		Area *a = f->area;
		if(a->mode == Colmax)
			c = a->sel->client;
		focus(c, False);
	}
	else if(ev->window == blz.root) {
		sel_screen = True;
		draw_frames();
	}
}

static void
handle_leavenotify(XEvent *e)
{
	XCrossingEvent *ev = &e->xcrossing;

	if((ev->window == blz.root) && !ev->same_screen) {
		sel_screen = True;
		draw_frames();
	}
}

static void
handle_expose(XEvent *e)
{
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
handle_keypress(XEvent *e)
{
	XKeyEvent *ev = &e->xkey;
	ev->state &= valid_mask;
	handle_key(blz.root, ev->state, (KeyCode) ev->keycode);
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

	if(!XGetWindowAttributes(blz.display, ev->window, &wa))
		return;

	if(wa.override_redirect) {
		XSelectInput(blz.display, ev->window,
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
