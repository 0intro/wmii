/* Copyright ©2006 Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */
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

static void
rect_morph_xy(XRectangle *rect, int dx, int dy, BlitzAlign *mask) {
	BlitzAlign new_mask = 0;
	if(*mask & NORTH) {
		if(rect->height - dy >= 0 || *mask & SOUTH) {
			rect->y += dy;
			rect->height -= dy;
		}
		else {
			rect->y += rect->height;
			rect->height = dy - rect->height;
			new_mask ^= NORTH|SOUTH;
		}
	}
	if(*mask & SOUTH) {
		if(rect->height + dy >= 0 || *mask & NORTH)
			rect->height += dy;
		else {
			rect->height = -dy - rect->height;
			rect->y -= rect->height;
			new_mask ^= NORTH|SOUTH;
		}
	}
	if(*mask & EAST) {
		if(rect->width + dx >= 0 || *mask & WEST)
			rect->width += dx;
		else {
			rect->width = -dx - rect->width;
			rect->x -= rect->width;
			new_mask ^= EAST|WEST;
		}
	}
	if(*mask & WEST) {
		if(rect->width - dx >= 0 || *mask & EAST) {
			rect->x += dx;
			rect->width -= dx;
		}
		else {
			rect->x += rect->width;
			rect->width = dx - rect->width;
			new_mask ^= EAST|WEST;
		}
	}
	*mask ^= new_mask;
}

typedef struct {
	XRectangle *rects;
	int num;
	int x1, y1;
	int x2, y2;
	int dx, dy;
	BlitzAlign mask;
} SnapArgs;

static void
snap_line(SnapArgs *a) {
	int i, t_xy;

	/* horizontal */
	if(a->y1 == a->y2 && (a->mask & (NORTH|SOUTH))) {
		for(i=0; i < a->num; i++) {
			if(!(r_east(&a->rects[i]) < a->x1) ||
				(a->rects[i].x > a->x2)) {

				if(abs(a->rects[i].y - a->y1) <= abs(a->dy))
					a->dy = a->rects[i].y - a->y1;

				t_xy = r_south(&a->rects[i]);
				if(abs(t_xy - a->y1) < abs(a->dy))
					a->dy = t_xy - a->y1;
			}
		}
	}
	else if (a->mask & (EAST|WEST)) {
		for(i=0; i < a->num; i++) {
			if(!(r_south(&a->rects[i]) < a->y1) ||
				(a->rects[i].y > a->y2)) {

				if(abs(a->rects[i].x - a->x1) <= abs(a->dx))
					a->dx = a->rects[i].x - a->x1;

				t_xy = r_east(&a->rects[i]);
				if(abs(t_xy - a->x1) < abs(a->dx))
					a->dx = t_xy - a->x1;
			}
		}
	}
}

/* Returns a gravity for increment handling. It's normally the opposite of the mask
 * (the directions that we're resizing in), unless a snap occurs, in which case, it's the
 * direction of the snap.
 */
BlitzAlign
snap_rect(XRectangle *rects, int num, XRectangle *current, BlitzAlign *mask, int snap) {
	SnapArgs a = { 0, };
	BlitzAlign ret;

	a.rects = rects;
	a.num = num;
	a.mask = *mask;
	a.dx = snap + 1;
	a.dy = snap + 1;

	a.x1 = current->x;
	a.x2 = r_east(current);
	if(*mask & NORTH) {
		a.y2 = a.y1 = current->y;
		snap_line(&a);
	}
	if(*mask & SOUTH) {
		a.y2 = a.y1 = r_south(current);
		snap_line(&a);
	}

	a.y1 = current->y;
	a.y2 = r_south(current);
	if(*mask & EAST) {
		a.x1 = a.x2 = r_east(current);
		snap_line(&a);
	}
	if(*mask & WEST) {
		a.x1 = a.x2 = current->x;
		snap_line(&a);
	}

	ret = CENTER;
	if(abs(a.dx) > snap)
		a.dx = 0;
	else
		ret ^= EAST|WEST;

	if(abs(a.dy) > snap)
		a.dy = 0;
	else
		ret ^= NORTH|SOUTH;

	rect_morph_xy(current, a.dx, a.dy, mask);

	return ret ^ *mask;
}

static void
draw_xor_border(XRectangle *r) {
	XRectangle xor;

	xor = *r;
	xor.x += 2;
	xor.y += 2;
	xor.width = xor.width > 4 ? xor.width - 4 : 0;
	xor.height = xor.height > 4 ? xor.height - 4 : 0;

	XSetLineAttributes(blz.dpy, xorgc, 1, LineSolid, CapNotLast, JoinMiter);
	XSetForeground(blz.dpy, xorgc, def.focuscolor.bg);
	if(xor.height > 4 && xor.width > 2)
		XDrawLine(blz.dpy, blz.root, xorgc,
			xor.x + 2,
			xor.y +  xor.height / 2,
			r_east(&xor) - 2,
			xor.y + xor.height / 2);
	if(xor.width > 4 && xor.height > 2)
		XDrawLine(blz.dpy, blz.root, xorgc,
			xor.x + xor.width / 2,
			xor.y + 2,
			xor.x + xor.width / 2,
			r_south(&xor) - 2);
	XSetLineAttributes(blz.dpy, xorgc, 4, LineSolid, CapNotLast, JoinMiter);
	XDrawRectangles(blz.dpy, blz.root, xorgc, &xor, 1);
}

static void
find_droppoint(Frame *frame, int x, int y, XRectangle *rect, Bool do_move) {
	View *v;
	Area *a, *a_prev;
	Frame *f, *f_prev;
	Bool before;

	v = frame->view;
	rect->y = 0;
	rect->height = screen->rect.height - screen->brect.height;

	a_prev = v->area;
	for(a = a_prev->next; a && a->next; a = a->next) {
		if(x < r_east(&a->rect))
			break;
		a_prev = a;
	}
	if(x < (a->rect.x + labelh(&def.font))) {
		rect->x = a->rect.x - 4;
		rect->width = 8;

		if(do_move) {
			a = new_column(v, a_prev, 0);
			send_to_area(a, frame);
			focus(frame->client, False);
		}
		return;
	}
	if(x > (r_east(&a->rect) - labelh(&def.font))) {
		rect->x = r_east(&a->rect) - 4;
		rect->width = 8;

		if(do_move) {
			a = new_column(v, a, 0);
			send_to_area(a, frame);
			focus(frame->client, False);
		}
		return;
	}

	rect->x = a->rect.x;
	rect->width = a->rect.width;

	f_prev = nil;
	for(f = a->frame; f; f = f->anext) {
		if(y < f->rect.y)
			break;
		if(y < r_south(&f->rect))
			break;
		f_prev = f;
	}
	if(f == nil)
		f = f_prev;
	if(y < (f->rect.y + labelh(&def.font))) {
		before = True;
		rect->y = f->rect.y;
		rect->height = 2;
		if(f_prev) {
			rect->y = r_south(&f_prev->rect);
			rect->height = f->rect.y - rect->y;
		}
		if(do_move)
			goto do_move;
		return;
	}
	if(y > r_south(&f->rect) - labelh(&def.font)) {
		before = False;
		rect->y = r_south(&f->rect);
		rect->height = (screen->rect.height - labelh(&def.font) - rect->y);
		if(f->anext)
			rect->height = (f->anext->rect.y - rect->y);
		if(do_move)
			goto do_move;
		return;
	}

	*rect = f->rect;
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
querypointer(Window w, int *x, int *y) {
	Window dummy;
	uint ui;
	int i;
	
	XQueryPointer(blz.dpy, w, &dummy, &dummy, &i, &i, x, y, &ui);
}

void
warppointer(int x, int y) {
	XWarpPointer(blz.dpy,
		/* src_w */	None,
		/* dest_w */	blz.root,
		/* src_rect */	0, 0, 0, 0,
		/* target */	x, y
		);
}

static void
do_managed_move(Client *c) {
	XRectangle frect, ofrect;
	XEvent ev;
	Frame *f;
	int x, y;

	focus(c, False);
	f = c->sel;

	XSync(blz.dpy, False);
	if(XGrabPointer(blz.dpy, c->framewin, False, MouseMask, GrabModeAsync, GrabModeAsync,
			None, cursor[CurMove], CurrentTime) != GrabSuccess)
		return;
	XGrabServer(blz.dpy);

	querypointer(blz.root, &x, &y);

	find_droppoint(f, x, y, &frect, False);
	draw_xor_border(&frect);
	for(;;) {
		XMaskEvent(blz.dpy, MouseMask | ExposureMask, &ev);
		switch (ev.type) {
		case ButtonRelease:
			draw_xor_border(&frect);

			find_droppoint(f, x, y, &frect, True);

			XUngrabServer(blz.dpy);
			XUngrabPointer(blz.dpy, CurrentTime);
			XSync(blz.dpy, False);
			return;
		case MotionNotify:
			ofrect = frect;
			x = ev.xmotion.x_root;
			y = ev.xmotion.y_root;

			find_droppoint(f, x, y, &frect, False);

			if(memcmp(&frect, &ofrect, sizeof(frect))) {
				draw_xor_border(&ofrect);
				draw_xor_border(&frect);
			}
			break;
		case Expose:
			dispatch_event(&ev);
			break;
		default: break;
		}
	}
}

void
do_mouse_resize(Client *c, Bool opaque, BlitzAlign align) {
	BlitzAlign grav;
	Window dummy;
	Cursor cur;
	XEvent ev;
	XRectangle *rects, ofrect, frect, origin;
	int snap, dx, dy, pt_x, pt_y, hr_x, hr_y;
	uint num;
	Bool floating;
	float rx, ry, hrx, hry;
	Frame *f;

	f = c->sel;
	floating = f->area->floating;
	origin = frect = f->rect;
	cur = cursor_of_quad(align);
	if(floating) {
		rects = rects_of_view(f->area->view, &num, (opaque ? c->frame : nil));
		snap = screen->rect.height / 66;
	}else{
		rects = nil;
		snap = 0;
	}

	if(align == CENTER) {
		if(!opaque)
			cur = cursor[CurInvisible];
		if(!floating) {
			do_managed_move(c);
			return;
		}
	}

	querypointer(c->framewin, &pt_x, &pt_y);
	rx = (float)pt_x / frect.width;
	ry = (float)pt_y / frect.height;

	if(XGrabPointer(
		/* display */		blz.dpy,
		/* window */		c->framewin,
		/* owner_events */	False,
		/* event_mask */	MouseMask,
		/* pointer_mode */	GrabModeAsync,
		/* keyboard_mode */	GrabModeAsync,
		/* confine_to */	None,
		/* cursor */		cur,
		/* time */		CurrentTime
		) != GrabSuccess)
		return;

	querypointer(blz.root, &pt_x, &pt_y);

	if(align != CENTER) {
		hr_x = dx = frect.width / 2;
		hr_y = dy = frect.height / 2;
		if(align&NORTH) dy -= hr_y;
		if(align&SOUTH) dy += hr_y;
		if(align&EAST) dx += hr_x;
		if(align&WEST) dx -= hr_x;

		XTranslateCoordinates(blz.dpy,
			/* src, dst */	c->framewin, blz.root,
			/* src x,y */	dx, dy,
			/* dest x,y */	&pt_x, &pt_y,
			/* child */	&dummy
			);
		warppointer(pt_x, pt_y);
	}
	else if(f->client->fullscreen)
		return;
	else if(!opaque) {
		hrx = (double)(screen->rect.width + frect.width - 2 * labelh(&def.font)) / screen->rect.width;
		hry = (double)(screen->rect.height  + frect.height - 3 * labelh(&def.font)) / screen->rect.height;
		pt_x = r_east(&frect) - labelh(&def.font);
		pt_y = r_south(&frect) - labelh(&def.font);
		warppointer(pt_x / hrx, pt_y / hry);
		flushevents(PointerMotionMask, False);
	}

	XSync(blz.dpy, False);
	if(!opaque) {
		XGrabServer(blz.dpy);
		draw_xor_border(&frect);
	}else
		unmap_client(c, IconicState);

	for(;;) {
		XMaskEvent(blz.dpy, MouseMask | ExposureMask, &ev);
		switch (ev.type) {
		case ButtonRelease:
			if(!opaque)
				draw_xor_border(&frect);

			if(!floating)
				resize_column(c, &frect);
			else
				resize_client(c, &frect);

			if(!opaque) {
				XTranslateCoordinates(blz.dpy,
					/* src, dst */	c->framewin, blz.root,
					/* src_x */	(frect.width * rx),
					/* src_y */	(frect.height * ry),
					/* dest x,y */	&pt_x, &pt_y,
					/* child */	&dummy
					);
				if(pt_y > screen->brect.y)
					pt_y = screen->brect.y - 1;
				warppointer(pt_x, pt_y);
				XUngrabServer(blz.dpy);
			}else
				map_client(c);

			free(rects);

			XUngrabPointer(blz.dpy, CurrentTime);
			XSync(blz.dpy, False);
			return;
		case MotionNotify:
			ofrect = frect;
			dx = ev.xmotion.x_root;
			dy = ev.xmotion.y_root;

			if(align == CENTER && !opaque) {
				dx = (dx * hrx) - pt_x;
				dy = (dy * hry) - pt_y;
			}else{
				dx -= pt_x;
				dy -= pt_y;
			}
			pt_x += dx;
			pt_y += dy;

			rect_morph_xy(&origin, dx, dy, &align);
			check_frame_constraints(&origin);
			frect = origin;

			if(floating)
				grav = snap_rect(rects, num, &frect, &align, snap);
			else
				grav = align ^ CENTER;

			apply_sizehints(c, &frect, floating, True, grav);
			check_frame_constraints(&frect);

			if(opaque) {
				XMoveWindow(blz.dpy, c->framewin, frect.x, frect.y);
				XSync(blz.dpy, False);
			} else {
				draw_xor_border(&ofrect);
				draw_xor_border(&frect);
			}
			break;
		case Expose:
			dispatch_event(&ev);
			break;
		default:
			break;
		}
	}
}

void
grab_button(Window w, uint button, ulong mod) {
	XGrabButton(blz.dpy, button, mod, w, False, ButtonMask,
			GrabModeSync, GrabModeSync, None, None);
	if((mod != AnyModifier) && (num_lock_mask != 0)) {
		XGrabButton(blz.dpy, button, mod | num_lock_mask, w, False, ButtonMask,
			GrabModeSync, GrabModeAsync, None, None);
		XGrabButton(blz.dpy, button, mod | num_lock_mask | LockMask, w, False,
			ButtonMask, GrabModeSync, GrabModeAsync, None, None);
	}
}
