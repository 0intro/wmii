/* Copyright ©2006-2007 Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <util.h>
#include "dat.h"
#include "fns.h"

enum {
	ButtonMask =
		ButtonPressMask | ButtonReleaseMask,
	MouseMask =
		ButtonMask | PointerMotionMask
};

static Window *
gethsep(Rectangle r) {
	Window *w;
	WinAttr wa;
	
	wa.background_pixel = def.normcolor.border;
	w = createwindow(&scr.root, r, scr.depth, InputOutput, &wa, CWBackPixel);
	mapwin(w);
	XRaiseWindow(display, w->w);
	return w;
}

static void
rect_morph_xy(Rectangle *r, Point d, Align *mask) {
	int n;

	if(*mask & NORTH)
		r->min.y += d.y;
	if(*mask & WEST)
		r->min.x += d.x;
	if(*mask & SOUTH)
		r->max.y += d.y;
	if(*mask & EAST)
		r->max.x += d.x;
	
	if(r->min.x > r->max.x) {
		n = r->min.x;
		r->min.x = r->max.x;
		r->max.x = n;
		*mask ^= EAST|WEST;
	}
	if(r->min.y > r->max.y) {
		n = r->min.y;
		r->min.y = r->max.y;
		r->max.y = n;
		*mask ^= NORTH|SOUTH;
	}
}

typedef struct {
	Rectangle *rects;
	int num;
	Rectangle r;
	int x, y;
	int dx, dy;
	Align mask;
} SnapArgs;

static void
snap_line(SnapArgs *a) {
	Rectangle *r;
	int i, x, y;

	if(a->mask & (NORTH|SOUTH)) {
		for(i=0; i < a->num; i++) {
			r = &a->rects[i];
			if((r->min.x <= a->r.max.x) && (r->max.x >= a->r.min.x)) {
				y = r->min.y;
				if(abs(y - a->y) <= abs(a->dy))
					a->dy = y - a->y;

				y = r->max.y;
				if(abs(y - a->y) <= abs(a->dy))
					a->dy = y - a->y;
			}
		}
	}else {
		for(i=0; i < a->num; i++) {
			r = &a->rects[i];
			if((r->min.y <= a->r.max.y) && (r->max.y >= a->r.min.y)) {
				x = r->min.x;
				if(abs(x - a->x) <= abs(a->dx))
					a->dx = x - a->x;

				x = r->max.x;
				if(abs(x - a->x) <= abs(a->dx))
					a->dx = x - a->x;
			}
		}
	}
}

/* Returns a gravity for increment handling. It's normally the opposite of the mask
 * (the directions that we're resizing in), unless a snap occurs, in which case, it's the
 * direction of the snap.
 */
Align
snap_rect(Rectangle *rects, int num, Rectangle *r, Align *mask, int snap) {
	SnapArgs a = { 0, };
	Align ret;

	a.rects = rects;
	a.num = num;
	a.dx = snap + 1;
	a.dy = snap + 1;
	a.r = *r;

	a.mask = NORTH|SOUTH;
	if(*mask & NORTH) {
		a.y = r->min.y;
		snap_line(&a);
	}
	if(*mask & SOUTH) {
		a.y = r->max.y;
		snap_line(&a);
	}

	a.mask = EAST|WEST;
	if(*mask & EAST) {
		a.x = r->max.x;
		snap_line(&a);
	}
	if(*mask & WEST) {
		a.x = r->min.x;
		snap_line(&a);
	}

	ret = CENTER;
	if(abs(a.dx) <= snap)
		ret ^= EAST|WEST;
	else
		a.dx = 0;

	if(abs(a.dy) <= snap)
		ret ^= NORTH|SOUTH;
	else
		a.dy = 0;

	rect_morph_xy(r, Pt(a.dx, a.dy), mask);
	return ret ^ *mask;
}

static void
xorborder(Rectangle r) {
	Rectangle r2;
	ulong col;
	
	col = def.focuscolor.bg;

	r2 = insetrect(r, 4);

	if(Dy(r) > 4 && Dx(r) > 2)
		drawline(&xor,
			Pt(r2.min.x, r2.min.y + Dy(r2)/2),
			Pt(r2.max.x, r2.min.y + Dy(r2)/2),
			CapNotLast, 1, col);
	if(Dx(r) > 4 && Dy(r) > 2)
		drawline(&xor,
			Pt(r2.min.x + Dx(r2)/2, r.min.y),
			Pt(r2.min.x + Dx(r2)/2, r.max.y),
			CapNotLast, 1, col);
	border(&xor, r, 4, col);
}

static void
xorrect(Rectangle r) {
	fill(&xor, r, 0x00888888L);
}

static void
find_droppoint(Frame *frame, int x, int y, Rectangle *r, Bool do_move) {
	enum { Delta = 5 };
	View *v;
	Area *a, *a_prev;
	Frame *f;
	Bool before;

	v = frame->view;

	/* New column? */
	a_prev = v->area;
	for(a = a_prev->next; a && a->next; a = a->next) {
		if(x < a->rect.max.x)
			break;
		a_prev = a;
	}

	r->min.y = screen->rect.min.y;
	r->max.y = screen->brect.min.y;
	if(x < (a->rect.min.x + labelh(def.font))) {
		r->min.x = a->rect.min.x - Delta;
		r->max.x = a->rect.min.x + Delta;
		if(do_move) {
			a = new_column(v, a_prev, 0);
			send_to_area(a, frame);
			focus(frame->client, False);
		}
		return;
	}
	if(x > (a->rect.max.x - labelh(def.font))) {
		r->min.x = a->rect.max.x - Delta;
		r->max.x = a->rect.max.x + Delta;
		if(do_move) {
			a = new_column(v, a, 0);
			send_to_area(a, frame);
			focus(frame->client, False);
		}
		return;
	}

	/* Over/under frame? */
	for(f = a->frame; f; f = f->anext)
		if(y < f->rect.max.y || f->anext == nil)
			break;

	*r = a->rect;
	if(y < (f->rect.min.y + labelh(def.font))) {
		before = True;
		r->min.y = f->rect.min.y - Delta;
		r->max.y = f->rect.min.y + Delta;
		if(do_move)
			goto do_move;
		return;
	}
	if(y > f->rect.max.y - labelh(def.font)) {
		before = False;
		r->min.y = f->rect.max.y - Delta;
		r->max.y = f->rect.max.y + Delta;
		if(do_move)
			goto do_move;
		return;
	}

	/* No? Swap. */
	*r = f->rect;
	if(do_move) {
		swap_frames(frame, f);
		focus(frame->client, False);
		focus_view(screen, f->view);
	}
	return;

do_move:
	if(frame == f)
		return;
	if(a != frame->area)
		send_to_area(a, frame);
	remove_frame(frame);
	insert_frame(f, frame, before);
	arrange_column(f->area, False);
	focus(frame->client, True);
}

void
querypointer(Window *w, int *x, int *y) {
	XWindow dummy;
	uint ui;
	int i;
	
	XQueryPointer(display, w->w, &dummy, &dummy, &i, &i, x, y, &ui);
}

void
warppointer(int x, int y) {
	XWarpPointer(display,
		/* src, dest w */ None, scr.root.w,
		/* src_rect */	0, 0, 0, 0,
		/* target */	x, y);
}

void
translate(Window *src, Window *dst, int sx, int sy, int *dx, int *dy) {
	XWindow w;

	XTranslateCoordinates(display, src->w, dst->w, sx, sy, dx, dy, &w);
}

static void
do_managed_move(Client *c) {
	Rectangle frect, ofrect;
	XEvent ev;
	Frame *f;
	int x, y;

	focus(c, False);
	f = c->sel;

	XSync(display, False);
	if(!grabpointer(c->framewin, nil, cursor[CurMove], MouseMask))
		return;
	XGrabServer(display);

	querypointer(&scr.root, &x, &y);

	find_droppoint(f, x, y, &frect, False);
	xorrect(frect);
	for(;;) {
		XMaskEvent(display, MouseMask | ExposureMask, &ev);
		switch (ev.type) {
		default:
			break;
		case Expose:
			dispatch_event(&ev);
			break;
		case MotionNotify:
			ofrect = frect;
			x = ev.xmotion.x_root;
			y = ev.xmotion.y_root;

			find_droppoint(f, x, y, &frect, False);

			if(!eqrect(frect, ofrect)) {
				xorrect(ofrect);
				xorrect(frect);
			}
			break;
		case ButtonRelease:
			xorrect(frect);

			find_droppoint(f, x, y, &frect, True);

			XUngrabServer(display);
			XUngrabPointer(display, CurrentTime);
			XSync(display, False);
			return;
		}
	}
}

void
mouse_resizecolframe(Frame *f, Align align) {
	WinAttr wa;
	XEvent ev;
	Window *cwin, *hwin;
	Divide *d;
	Frame *fp;
	View *v;
	Area *a, *ap;
	Rectangle r;
	uint minw, minh;
	int x, y;

	assert((align&(EAST|WEST)) != (EAST|WEST));
	assert((align&(NORTH|SOUTH)) != (NORTH|SOUTH));

	v = screen->sel;
	for(a = v->area->next, d = divs; a; a = a->next, d = d->next) {
		if(a->next == f->area)
			ap = a;
		if(a == f->area)
			break;
	}
	for(fp = a->frame; fp; fp = fp->anext)
		if(fp->anext == f) break;
	if(align&EAST)
		d = d->next;

	if(!grabpointer(&scr.root, cwin, cursor[CurSizing], MouseMask))
		return;

	if(align&NORTH) {
		r.min.y = (fp ? fp->rect.min.y : screen->rect.min.y);
		r.max.y = f->rect.max.y;
	}else {
		r.min.y = f->rect.min.y;
		r.max.y = (f->anext ? f->anext->rect.max.y : a->rect.max.y);
	}
	if(align&WEST) {
		r.min.x = (ap ? ap->rect.min.x : screen->rect.min.x);
		r.max.x = a->rect.max.x;
	}else {
		r.min.x = a->rect.min.x;
		r.max.x = (a->next ? a->next->rect.max.x : screen->rect.max.x);
	}
	minw = Dx(screen->rect)/NCOL;
	minh = frame_delta_h() + labelh(def.font);
	r.min.x += minw;
	r.max.x -= minw;
	r.min.y += minh;
	r.max.y -= minh;

	cwin = createwindow(&scr.root, r, 0, InputOnly, &wa, 0);
	mapwin(cwin);

	r = f->rect;
	if(align&NORTH)
		r.min.y--;
	else
		r.min.y = r.max.y - 1;
	r.max.y = r.min.y + 2;

	hwin = gethsep(r);
	
	x = ((align&WEST) ? f->rect.min.x : f->rect.max.x);
	y = ((align&NORTH) ? f->rect.min.y : f->rect.max.y);
	warppointer(x, y);

	for(;;) {
		XMaskEvent(display, MouseMask | ExposureMask, &ev);
		switch (ev.type) {
		default:
			break;
		case Expose:
			dispatch_event(&ev);
			break;
		case MotionNotify:
			x = ev.xmotion.x_root;
			y = ev.xmotion.y_root;

			if(align&WEST)
				r.min.x = x;
			else
				r.max.x = x;
			r.min.y = ((align&SOUTH) ? y : y-1);
			r.max.y = r.min.y+2;

			setdiv(d, x);
			reshapewin(hwin, r);
			break;
		case ButtonRelease:
			if(align&WEST)
				r.min.x = x;
			else
				r.max.x = x;
			if(align&NORTH) {
				r.min.y = y;
				r.max.y = f->rect.max.y;
			}else {
				r.min.y = f->rect.min.y;
				r.max.y = y;
			}
			resize_colframe(f, &r);
			
			if(align&WEST)
				x = f->rect.min.x + 1;
			else
				x = f->rect.max.x - 2;
			if(align&NORTH)
				y = f->rect.min.y + 1;
			else
				y = f->rect.max.y - 2;
			warppointer(x, y);
			goto done;
		}
	}
done:
	XUngrabPointer(display, CurrentTime);
	destroywindow(cwin);
	destroywindow(hwin);
}

void
mouse_resizecol(Divide *d) {
	WinAttr wa;
	XEvent ev;
	Window *cwin;
	Divide *dp;
	View *v;
	Area *a;
	Rectangle r;
	uint minw;
	int x, y;

	v = screen->sel;

	for(a = v->area->next, dp = divs; a; a = a->next, dp = dp->next)
		if(dp->next == d) break;

	/* Fix later */
	if(a == nil || a->next == nil)
		return;

	if(!grabpointer(&scr.root, cwin, cursor[CurInvisible], MouseMask))
		return;

	querypointer(&scr.root, &x, &y);

	minw = Dx(screen->rect)/NCOL;
	r.min.x = a->rect.min.x + minw;
	r.max.x = a->next->rect.max.x - minw;
	r.min.y = y;
	r.max.y = y+1;
	cwin = createwindow(&scr.root, r, 0, InputOnly, &wa, 0);
	mapwin(cwin);

	for(;;) {
		XMaskEvent(display, MouseMask | ExposureMask, &ev);
		switch (ev.type) {
		default:
			break;
		case Expose:
			dispatch_event(&ev);
			break;
		case MotionNotify:
			x = ev.xmotion.x_root;
			setdiv(d, x);
			break;
		case ButtonRelease:
			resize_column(a, x - a->rect.min.x);
			goto done;
		}
	}
done:
	XUngrabPointer(display, CurrentTime);
	destroywindow(cwin);
}

void
do_mouse_resize(Client *c, Bool opaque, Align align) {
	XEvent ev;
	Rectangle *rects;
	Rectangle ofrect, frect, origin;
	Align grav;
	Cursor cur;
	int dx, dy, pt_x, pt_y, hr_x, hr_y;
	float rx, ry, hrx, hry;
	uint num;
	Bool floating;
	Frame *f;

	f = c->sel;

	if(!f->area->floating) {
		if(align==CENTER)
			do_managed_move(c);
		else
			mouse_resizecolframe(f, align);
		return;
	}

	origin = frect = f->rect;
	rects = rects_of_view(f->area->view, &num, (opaque ? c->frame : nil));

	cur = cursor_of_quad(align);
	if((align==CENTER) && !opaque)
		cur = cursor[CurInvisible];

	querypointer(c->framewin, &pt_x, &pt_y);
	rx = (float)pt_x / Dx(frect);
	ry = (float)pt_y /Dy(frect);

	if(!grabpointer(c->framewin, nil, cur, MouseMask))
		return;

	querypointer(&scr.root, &pt_x, &pt_y);

	if(align != CENTER) {
		hr_x = dx = Dx(frect) / 2;
		hr_y = dy = Dy(frect) / 2;
		if(align&NORTH) dy -= hr_y;
		if(align&SOUTH) dy += hr_y;
		if(align&EAST) dx += hr_x;
		if(align&WEST) dx -= hr_x;

		translate(c->framewin, &scr.root, dx, dy, &pt_x, &pt_y);
		warppointer(pt_x, pt_y);
	}
	else if(f->client->fullscreen) {
		XUngrabPointer(display, CurrentTime);
		return;
	}
	else if(!opaque) {
		hrx = (double)(Dx(screen->rect) + Dx(frect) - 2 * labelh(def.font))
				/ Dx(screen->rect);
		hry = (double)(Dy(screen->rect)  + Dy(frect) - 3 * labelh(def.font))
				/ Dy(screen->rect);
		pt_x = frect.max.x - labelh(def.font);
		pt_y = frect.max.y - labelh(def.font);
		warppointer(pt_x / hrx, pt_y / hry);
		flushevents(PointerMotionMask, False);
	}

	XSync(display, False);
	if(!opaque) {
		XGrabServer(display);
		xorborder(frect);
	}else
		unmap_client(c, IconicState);

	for(;;) {
		XMaskEvent(display, MouseMask | ExposureMask, &ev);
		switch (ev.type) {
		default:
			break;
		case Expose:
			dispatch_event(&ev);
			break;
		case MotionNotify:
			ofrect = frect;
			dx = ev.xmotion.x_root;
			dy = ev.xmotion.y_root;

			if(align == CENTER && !opaque) {
				dx = (dx * hrx) - pt_x;
				dy = (dy * hry) - pt_y;
			}else {
				dx -= pt_x;
				dy -= pt_y;
			}
			pt_x += dx;
			pt_y += dy;

			rect_morph_xy(&origin, Pt(dx, dy), &align);
			origin = constrain(origin);
			frect = origin;

			grav = snap_rect(rects, num, &frect, &align, def.snap);

			apply_sizehints(c, &frect, floating, True, grav);
			frect = constrain(frect);

			if(opaque) {
				movewin(c->framewin, frect.min);
				XSync(display, False);
			}else {
				xorborder(ofrect);
				xorborder(frect);
			}
			break;
		case ButtonRelease:
			if(!opaque)
				xorborder(frect);

			resize_client(c, &frect);

			if(!opaque) {
				translate(c->framewin, &scr.root,
					(Dx(frect)*rx), (Dy(frect)*ry),
					&pt_x, &pt_y);
				if(pt_y > screen->brect.min.y)
					pt_y = screen->brect.min.y - 1;
				warppointer(pt_x, pt_y);
				XUngrabServer(display);
			}else
				map_client(c);

			free(rects);
			XUngrabPointer(display, CurrentTime);
			return;
		}
	}
}

void
grab_button(XWindow w, uint button, ulong mod) {
	XGrabButton(display, button, mod, w, False, ButtonMask,
			GrabModeSync, GrabModeSync, None, None);
	if((mod != AnyModifier) && (num_lock_mask != 0)) {
		XGrabButton(display, button, mod | num_lock_mask, w, False, ButtonMask,
			GrabModeSync, GrabModeAsync, None, None);
		XGrabButton(display, button, mod | num_lock_mask | LockMask, w, False,
			ButtonMask, GrabModeSync, GrabModeAsync, None, None);
	}
}
