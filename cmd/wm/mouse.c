/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "wm.h"

#define ButtonMask      (ButtonPressMask | ButtonReleaseMask)
#define MouseMask       (ButtonMask | PointerMotionMask)

static int
check_vert_match(XRectangle *r, XRectangle *neighbor)
{
	/* check if neighbor matches edge */
	return (((neighbor->y <= r->y) && (neighbor->y + neighbor->height >= r->y))
			|| ((neighbor->y >= r->y) && (r->y + r->height >= neighbor->y)));
}

static int
check_horiz_match(XRectangle * r, XRectangle * neighbor)
{
	/* check if neighbor matches edge */
	return (((neighbor->x <= r->x) && (neighbor->x + neighbor->width >= r->x))
			|| ((neighbor->x >= r->x)
				&& (r->x + r->width >= neighbor->x)));
}

static void
snap_move(XRectangle *r, XRectangle *rects,
		unsigned int num, int snapw, int snaph)
{
	int i, j, w = 0, n = 0, e = 0, s = 0;

	/* snap to other windows */
	for(i = 0; i <= snapw && !(w && e); i++) {
		for(j = 0; j < num && !(w && e); j++) {
			/* check west neighbors leftwards */
			if(!w) {
				if(r->x - i == (rects[j].x + rects[j].width)) {
					/* west edge of neighbor found, check vert match */
					w = check_vert_match(r, &rects[j]);
					if(w)
						r->x = rects[j].x + rects[j].width;
				}
			}
			/* check west neighbors rightwards */
			if(!w) {
				if(r->x + i == (rects[j].x + rects[j].width)) {
					/* west edge of neighbor found, check vert match */
					w = check_vert_match(r, &rects[j]);
					if(w)
						r->x = rects[j].x + rects[j].width;
				}
			}
			/* check east neighbors leftwards */
			if(!e) {
				if(r->x + r->width - i == rects[j].x) {
					/* east edge of neighbor found, check vert match */
					e = check_vert_match(r, &rects[j]);
					if(e)
						r->x = rects[j].x - r->width;
				}
			}
			/* check east neighbors rightwards */
			if(!e) {
				if(r->x + r->width + i == rects[j].x) {
					/* east edge of neighbor found, check vert match */
					e = check_vert_match(r, &rects[j]);
					if(e)
						r->x = rects[j].x - r->width;
				}
			}
		}

		/* snap to west screen border */
		if(!w && (r->x - i == rect.x)) {
			w = 1;
			r->x = rect.x;
		}
		/* snap to west screen border */
		if(!w && (r->x + i == rect.x)) {
			w = 1;
			r->x = rect.x;
		}
		/* snap to east screen border */
		if(!e && (r->x + r->width - i == rect.width)) {
			e = 1;
			r->x = rect.x + rect.width - r->width;
		}
		if(!e && (r->x + r->width + i == rect.width)) {
			e = 1;
			r->x = rect.x + rect.width - r->width;
		}
	}

	for(i = 0; i <= snaph && !(n && s); i++) {
		for(j = 0; j < num && !(n && s); j++) {
			/* check north neighbors upwards */
			if(!n) {
				if(r->y - i == (rects[j].y + rects[j].height)) {
					/* north edge of neighbor found, check horiz match */
					n = check_horiz_match(r, &rects[j]);
					if(n)
						r->y = rects[j].y + rects[j].height;
				}
			}
			/* check north neighbors downwards */
			if(!n) {
				if(r->y + i == (rects[j].y + rects[j].height)) {
					/* north edge of neighbor found, check horiz match */
					n = check_horiz_match(r, &rects[j]);
					if(n)
						r->y = rects[j].y + rects[j].height;
				}
			}
			/* check south neighbors upwards */
			if(!s) {
				if(r->y + r->height - i == rects[j].y) {
					/* south edge of neighbor found, check horiz match */
					s = check_horiz_match(r, &rects[j]);
					if(s)
						r->y = rects[j].y - r->height;
				}
			}
			/* check south neighbors downwards */
			if(!s) {
				if(r->y + r->height + i == rects[j].y) {
					/* south edge of neighbor found, check horiz match */
					s = check_horiz_match(r, &rects[j]);
					if(s)
						r->y = rects[j].y - r->height;
				}
			}
		}

		/* snap to north screen border */
		if(!n && (r->y - i == rect.y)) {
			n = 1;
			r->y = rect.y;
		}
		if(!n && (r->y + i == rect.y)) {
			n = 1;
			r->y = rect.y;
		}
		/* snap to south screen border */
		if(!s && (r->y + r->height - i == rect.height)) {
			s = 1;
			r->y = rect.y + rect.height - r->height;
		}
		if(!s && (r->y + r->height + i == rect.height)) {
			s = 1;
			r->y = rect.y + rect.height - r->height;
		}
	}
}

static Window
init_opaque_win(XRectangle r)
{
	XSetWindowAttributes wa;
	wa.override_redirect = 1;
	wa.border_pixel = def.sel.border;
	wa.background_pixel = def.sel.bg;
	return XCreateWindow(dpy, root, r.x, r.y,
			r.width, r.height, 1,
			DefaultDepth(dpy, screen), CopyFromParent,
			DefaultVisual(dpy, screen),
			CWOverrideRedirect | CWBackPixel | CWBorderPixel, &wa);
}

void
do_mouse_move(Client *c)
{
	int px = 0, py = 0, wex, wey, ex, ey, i;
	Window dummy, opaque;
	XEvent ev;
	unsigned int num = 0;
	unsigned int dmask;
	Frame *f = c->frame.data[c->sel];
	int aidx = idx_of_area(f->area);
	int snapw = aidx ? 0 : (rect.width * def.snap) / 1000;
	int snaph = aidx ? 0 : (rect.height * def.snap) / 1000;
	XRectangle *rects = aidx ? nil : rects_of_view(f->area->view, &num);
	XRectangle frect = f->rect;
	XPoint pt;

	XQueryPointer(dpy, c->framewin, &dummy, &dummy, &i, &i, &wex, &wey, &dmask);
	XTranslateCoordinates(dpy, c->framewin, root, wex, wey, &ex, &ey, &dummy);
	pt.x = ex;
	pt.y = ey;
	XSync(dpy, False);

	if(XGrabPointer(dpy, root, False, MouseMask, GrabModeAsync, GrabModeAsync,
					None, cursor[CurMove], CurrentTime) != GrabSuccess)
		return;

	opaque = init_opaque_win(frect);
	XClearWindow(dpy, opaque);
	XMapRaised(dpy, opaque);
	for(;;) {
		XMaskEvent(dpy, MouseMask | ExposureMask, &ev);
		switch (ev.type) {
		case ButtonRelease:
			XDestroyWindow(dpy, opaque);
			if(aidx)
				resize_column(c, &frect, &pt);
			else
				resize_client(c, &frect, False);
			if(rects)
				free(rects);
			XUngrabPointer(dpy, CurrentTime);
			XSync(dpy, False);
			return;
			break;
		case MotionNotify:
			pt.x = ev.xmotion.x;
			pt.y = ev.xmotion.y;
			XTranslateCoordinates(dpy, c->framewin, root, ev.xmotion.x,
					ev.xmotion.y, &px, &py, &dummy);
			frect.x = px - ex;
			frect.y = py - ey;
			if(!aidx)
				snap_move(&frect, rects, num, snapw, snaph);
			XMoveResizeWindow(dpy, opaque, frect.x, frect.y, frect.width, frect.height);
			XClearWindow(dpy, opaque);
			break;
		case Expose:
			(handler[Expose])(&ev);
			break;
		default: break;
		}
	}
}

static void
snap_resize(XRectangle *r, XRectangle *o, BlitzAlign align,
		XRectangle *rects, unsigned int num, int px, int ox, int py,
		int oy, int snapw, int snaph)
{
	int i, j, pend = 0;
	int w, h;

	/* x */
	switch (align) {
	case NEAST:
	case SEAST:
		w = px - r->x + (o->width - ox);
		if(w < 10)
			break;
		r->width = w;
		if(w <= snapw)
			break;
		/* snap to border */
		for(i = 0; !pend && (i < snapw); i++) {
			if(r->x + r->width - i == rect.x + rect.width) {
				r->width -= i;
				break;
			}
			if(r->x + r->width + i == rect.x + rect.width) {
				r->width += i;
				break;
			}
			for(j = 0; j < num; j++) {
				if(r->x + r->width - i == rects[j].x) {
					pend = check_vert_match(r, &rects[j]);
					if(pend) {
						r->width -= i;
						break;
					}
				}
				if(r->x + r->width + i == rects[j].x) {
					pend = check_vert_match(r, &rects[j]);
					if(pend) {
						r->width += i;
						break;
					}
				}
			}
		}
		break;
	case NWEST:
	case SWEST:
		w = r->width + r->x - px + ox;
		if(w < 10)
			break;
		r->width = w;
		r->x = px - ox;
		if(w <= snapw)
			break;
		/* snap to border */
		for(i = 0; !pend && (i < snapw); i++) {
			if(r->x - i == rect.x) {
				r->x -= i;
				r->width += i;
				break;
			}
			if(r->x + i == rect.x) {
				r->x += i;
				r->width -= i;
				break;
			}
			for(j = 0; j < num; j++) {
				if(r->x - i == rects[j].x + rects[j].width) {
					pend = check_vert_match(r, &rects[j]);
					if(pend) {
						r->x -= i;
						r->width += i;
						break;
					}
				}
				if(r->x + i == rects[j].x + rects[j].width) {
					pend = check_vert_match(r, &rects[j]);
					if(pend) {
						r->x += i;
						r->width -= i;
						break;
					}
				}
			}
		}
		break;
	default:
		break;
	}

	/* y */
	pend = 0;
	switch (align) {
	case SWEST:
	case SEAST:
		h = py - r->y + (o->height - oy);
		if(h < 10)
			break;
		r->height = h;
		if(h <= snaph)
			break;
		/* snap to border */
		for(i = 0; !pend && (i < snaph); i++) {
			if(r->y + r->height - i == rect.y + rect.height) {
				r->height -= i;
				break;
			}
			if(r->y + r->height + i == rect.y + rect.height) {
				r->height += i;
				break;
			}
			for(j = 0; j < num; j++) {
				if(r->y + r->height - i == rects[j].y) {
					pend = check_horiz_match(r, &rects[j]);
					if(pend) {
						r->height -= i;
						break;
					}
				}
				if(r->y + r->height + i == rects[j].y) {
					pend = check_horiz_match(r, &rects[j]);
					if(pend) {
						r->height += i;
						break;
					}
				}
			}
		}
		break;
	case NWEST:
	case NEAST:
		h = r->height + r->y - py + oy;
		if(h < 10)
			break;
		r->height = h;
		r->y = py - oy;
		if(h <= snaph)
			break;
		/* snap to border */
		for(i = 0; !pend && (i < snaph); i++) {
			if(r->y - i == rect.y) {
				r->y -= i;
				r->height += i;
				break;
			}
			if(r->y + i == rect.y) {
				r->y += i;
				r->height -= i;
				break;
			}
			for(j = 0; j < num; j++) {
				if(r->y - i == rects[j].y + rects[j].height) {
					pend = check_horiz_match(r, &rects[j]);
					if(pend) {
						r->y -= i;
						r->height += i;
						break;
					}
				}
				if(r->y + i == rects[j].y + rects[j].height) {
					pend = check_horiz_match(r, &rects[j]);
					if(pend) {
						r->y += i;
						r->height -= i;
						break;
					}
				}
			}
		}
		break;
	default:
		break;
	}
}

void
do_mouse_resize(Client *c, BlitzAlign align)
{
	int px = 0, py = 0, i, ox, oy;
	Window dummy, opaque;
	XEvent ev;
	unsigned int dmask;
	unsigned int num = 0;
	Frame *f = c->frame.data[c->sel];
	int aidx = idx_of_area(f->area);
	int snapw = aidx ? 0 : (rect.width * def.snap) / 1000;
	int snaph = aidx ? 0 : (rect.height * def.snap) / 1000;
	XRectangle *rects = aidx ? nil : rects_of_view(f->area->view, &num);
	XRectangle frect = f->rect;
	XRectangle origin = frect;

	XQueryPointer(dpy, c->framewin, &dummy, &dummy, &i, &i, &ox, &oy, &dmask);
	XSync(dpy, False);

	if(XGrabPointer(dpy, c->framewin, False, MouseMask, GrabModeAsync, GrabModeAsync,
					None, cursor[CurResize], CurrentTime) != GrabSuccess)
		return;

	opaque = init_opaque_win(frect);
	XClearWindow(dpy, opaque);
	XMapRaised(dpy, opaque);
	for(;;) {
		XMaskEvent(dpy, MouseMask | ExposureMask, &ev);
		switch (ev.type) {
		case ButtonRelease:
			XDestroyWindow(dpy, opaque);
			if(aidx)
				resize_column(c, &frect, nil);
			else
				resize_client(c, &frect, False);
			if(rects)
				free(rects);
			XUngrabPointer(dpy, CurrentTime);
			XSync(dpy, False);
			return;
			break;
		case MotionNotify:
			XTranslateCoordinates(dpy, c->framewin, root, ev.xmotion.x,
					ev.xmotion.y, &px, &py, &dummy);
			snap_resize(&frect, &origin, align, rects, num, px,
						ox, py, oy, snapw, snaph);
			XMoveResizeWindow(dpy, opaque, frect.x, frect.y, frect.width, frect.height);
			XClearWindow(dpy, opaque);
			break;
		case Expose:
			(handler[Expose])(&ev);
			break;
		default: break;
		}
	}
}

void
grab_mouse(Window w, unsigned long mod, unsigned int button)
{
	XGrabButton(dpy, button, mod, w, False, ButtonMask,
			GrabModeAsync, GrabModeSync, None, None);
	if((mod != AnyModifier) && num_lock_mask) {
		XGrabButton(dpy, button, mod | num_lock_mask, w, False, ButtonMask,
				GrabModeAsync, GrabModeSync, None, None);
		XGrabButton(dpy, button, mod | num_lock_mask | LockMask, w, False,
				ButtonMask, GrabModeAsync, GrabModeSync, None, None);
	}
}

void
ungrab_mouse(Window w, unsigned long mod, unsigned int button)
{
	XUngrabButton(dpy, button, mod, w);
	if(mod != AnyModifier && num_lock_mask) {
		XUngrabButton(dpy, button, mod | num_lock_mask, w);
		XUngrabButton(dpy, button, mod | num_lock_mask | LockMask, w);
	}
}
