/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <X11/keysym.h>

#include "wm.h"

/* local functions */
static void     handle_buttonpress(XEvent * e);
static void     handle_configurerequest(XEvent * e);
static void     handle_destroynotify(XEvent * e);
static void     handle_expose(XEvent * e);
static void     handle_maprequest(XEvent * e);
static void     handle_motionnotify(XEvent * e);
static void     handle_propertynotify(XEvent * e);
static void     handle_unmapnotify(XEvent * e);
static void     handle_enternotify(XEvent * e);
static void     update_ignore_enternotify_hack(XEvent * e);

static unsigned int ignore_enternotify_hack = 0;

void            (*handler[LASTEvent]) (XEvent *);

void 
init_event_hander()
{
	int             i;
	/* init event handler */
	for (i = 0; i < LASTEvent; i++) {
		handler[i] = 0;
	}
	handler[ButtonPress] = handle_buttonpress;
	handler[CirculateNotify] = update_ignore_enternotify_hack;
	handler[ConfigureRequest] = handle_configurerequest;
	handler[DestroyNotify] = handle_destroynotify;
	handler[EnterNotify] = handle_enternotify;
	handler[Expose] = handle_expose;
	handler[GravityNotify] = update_ignore_enternotify_hack;
	handler[MapRequest] = handle_maprequest;
	handler[MapNotify] = update_ignore_enternotify_hack;
	handler[MotionNotify] = handle_motionnotify;
	handler[PropertyNotify] = handle_propertynotify;
	handler[UnmapNotify] = handle_unmapnotify;
}

void 
check_event(Connection * c)
{
	XEvent          ev;
	while (XPending(dpy)) {
		XNextEvent(dpy, &ev);
		/* main evet loop */
		if (handler[ev.type]) {
			/* call handler */
			(handler[ev.type]) (&ev);
		}
	}
}

static void 
handle_buttonpress(XEvent * e)
{
	Client         *c;
	XButtonPressedEvent *ev = &e->xbutton;
	Frame          *f = win_to_frame(ev->window);
	if (f) {
		handle_frame_buttonpress(ev, f);
		return;
	}
	if (ev->window == root) {
		XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
		XSync(dpy, False);
	}
	if ((c = win_to_client(ev->window))) {
		if (c->frame) {
			ev->state &= valid_mask;
			if (ev->state & Mod1Mask) {
				if (!c->frame->area->page->sel)
					XRaiseWindow(dpy, c->frame->win);
				switch (ev->button) {
				case Button1:
					mouse_move(c->frame);
					break;
				case Button3:
					{
						Align           align = xy_to_align(&c->rect, ev->x, ev->y);
						if (align == CENTER)
							mouse_move(c->frame);
						else
							mouse_resize(c->frame, align);
					}
					break;
				default:
					break;
				}
			}
		}
	}
}

static void 
handle_configurerequest(XEvent * e)
{
	XConfigureRequestEvent *ev = &e->xconfigurerequest;
	XWindowChanges  wc;
	Client         *c;
	unsigned int    bw = 0, tabh = 0;
	Frame          *f = 0;

	update_ignore_enternotify_hack(e);
	/* fprintf(stderr, "%s",  "configure request\n"); */
	c = win_to_client(ev->window);
	ev->value_mask &= ~CWSibling;

	if (c) {
		/* fprintf(stderr, "%s",  "configure request client\n"); */
		f = c->frame;

		if (f) {
			bw = border_width(f);
			tabh = tab_height(f);
		}
		if (ev->value_mask & CWStackMode) {
			if (wc.stack_mode == Above)
				XRaiseWindow(dpy, c->win);
			else
				ev->value_mask &= ~CWStackMode;
		}
		gravitate(c, tabh ? tabh : bw, bw, 1);

		if (ev->value_mask & CWX)
			c->rect.x = ev->x;
		if (ev->value_mask & CWY)
			c->rect.y = ev->y;
		if (ev->value_mask & CWWidth)
			c->rect.width = ev->width;
		if (ev->value_mask & CWHeight)
			c->rect.height = ev->height;
		if (ev->value_mask & CWBorderWidth)
			c->border = ev->border_width;

		gravitate(c, tabh ? tabh : bw, bw, 0);

		if (f) {
			XRectangle     *frect = rect_of_frame(f);
			frect->x = wc.x = c->rect.x - bw;
			frect->y = wc.y = c->rect.y - (tabh ? tabh : bw);
			frect->width = wc.width = c->rect.width + 2 * bw;
			frect->height = wc.height =
				c->rect.height + bw + (tabh ? tabh : bw);
			wc.border_width = 1;
			wc.sibling = None;
			wc.stack_mode = ev->detail;
			XConfigureWindow(dpy, f->win, ev->value_mask, &wc);
			configure_client(c);
		}
	}
	wc.x = ev->x;
	wc.y = ev->y;

	if (f) {
		/* if so, then bw and tabh are already initialized */
		wc.x = bw;
		wc.y = tabh ? tabh : bw;
	}
	wc.width = ev->width;
	wc.height = ev->height;
	wc.border_width = 0;
	wc.sibling = None;
	wc.stack_mode = Above;
	ev->value_mask &= ~CWStackMode;
	ev->value_mask |= CWBorderWidth;
	XConfigureWindow(dpy, e->xconfigurerequest.window, ev->value_mask,
			 &wc);
	XSync(dpy, False);

	/*
	   fprintf(stderr, "%d,%d,%d,%d\n", wc.x, wc.y, wc.width, wc.height);
	 */
}

static void 
handle_destroynotify(XEvent * e)
{
	XDestroyWindowEvent *ev = &e->xdestroywindow;
	Client         *c = win_to_client(ev->window);
	/* fprintf(stderr, "destroy: client 0x%x\n", (int)ev->window); */
	if (!c)
		return;
	if (c->frame)
		detach_client_from_frame(c, 0, 1);
	else if (detached && (index_item((void **) detached, c) >= 0))
		detached = (Client **) detach_item((void **) detached, c,
						   sizeof(Client *));
	free_client(c);
}

static void 
handle_expose(XEvent * e)
{
	static Frame   *f;
	if (e->xexpose.count == 0) {
		f = win_to_frame(e->xbutton.window);
		if (f)
			draw_frame(f);
	}
}

static void 
handle_maprequest(XEvent * e)
{
	XMapRequestEvent *ev = &e->xmaprequest;
	static XWindowAttributes wa;
	static Client  *c;

	/* fprintf(stderr, "map: window 0x%x\n", (int)ev->window); */
	if (!XGetWindowAttributes(dpy, ev->window, &wa))
		return;
	if (wa.override_redirect)
		return;
	/* there're clients which send map requests twice */
	c = win_to_client(ev->window);
	if (!c)
		c = alloc_client(ev->window);
	if (!c->frame) {
		_init_client(c, &wa);
		attach_client(c);
	}
}

static void 
handle_motionnotify(XEvent * e)
{
	Frame          *f = win_to_frame(e->xmotion.window);
	Cursor          cursor;
	if (f) {
		Frame          *old = SELFRAME(pages[sel]);
		if (old != f) {
			focus_frame(f, 0, 0, 1);
			draw_frame(old);
			draw_frame(f);
		} else if (f->clients) {
			/* multihead assumption */
			XSetInputFocus(dpy, f->clients[f->sel]->win,
				       RevertToPointerRoot, CurrentTime);
			XSync(dpy, False);
		}
		cursor = cursor_for_motion(f, e->xmotion.x, e->xmotion.y);
		if (cursor != f->cursor) {
			f->cursor = cursor;
			XDefineCursor(dpy, f->win, cursor);
		}
	}
}

static void 
handle_propertynotify(XEvent * e)
{
	XPropertyEvent *ev = &e->xproperty;
	Client         *c = win_to_client(ev->window);

	if (c) {
		handle_client_property(c, ev);
		return;
	}
}

static void 
handle_unmapnotify(XEvent * e)
{
	XUnmapEvent    *ev = &e->xunmap;
	Client         *c;

	update_ignore_enternotify_hack(e);
	if (ev->event == root)
		return;
	if ((c = win_to_client(ev->window))) {
		if (c->frame) {
			detach_client_from_frame(c, 1, 0);
			if (pages)
				draw_page(pages[sel]);
			free_client(c);
		} else if (detached) {
			if (index_item((void **) detached, c) == -1)
				free_client(c);
		}
	}
}

static void 
handle_enternotify(XEvent * e)
{
	XCrossingEvent *ev = &e->xcrossing;
	Client         *c;

	if (ev->mode != NotifyNormal)
		return;

	/* mouse is not in the focus window */
	if (ev->detail == NotifyInferior)
		return;

	c = win_to_client(ev->window);
	if (c && c->frame && (ev->serial != ignore_enternotify_hack)) {
		Frame          *old = SELFRAME(pages[sel]);
		XUndefineCursor(dpy, c->frame->win);
		if (old != c->frame) {
			focus_frame(c->frame, 0, 0, 1);
			draw_frame(old);
			draw_frame(c->frame);
		} else {
			/* multihead assumption */
			XSetInputFocus(dpy, c->win, RevertToPointerRoot, CurrentTime);
			XSync(dpy, False);
		}
	}
}

static void 
update_ignore_enternotify_hack(XEvent * e)
{
	ignore_enternotify_hack = e->xany.serial;
	XSync(dpy, False);
}
