/* (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * (C)opyright MMVI Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */
#include "wmii.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ButtonMask	(ButtonPressMask | ButtonReleaseMask)
#define MouseMask       (ButtonMask | PointerMotionMask)

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
	int x1, y1, x2, y2;
	BlitzAlign mask;
	int *delta;
} SnapArgs;

static void
snap_line(SnapArgs *a) {
	int i, t_xy;
	
	/* horizontal */
	if(a->y1 == a->y2 && (a->mask & (NORTH|SOUTH))) {
		for(i=0; i < a->num; i++) {
			if(!((a->rects[i].x + a->rects[i].width < a->x1) ||
				(a->rects[i].x > a->x2))) {
				
				if(abs(a->rects[i].y - a->y1) <= abs(*a->delta))
					*a->delta = a->rects[i].y - a->y1;
				
				t_xy = a->rects[i].y + a->rects[i].height;
				if(abs(t_xy - a->y1) < abs(*a->delta))
					*a->delta = t_xy - a->y1;
			}
		}
	}
	else if (a->mask & (EAST|WEST)) {
		/* This is the same as above, tr/xy/yx/, 
		 *                            s/width/height/, s/height/width/ */
		for(i=0; i < a->num; i++) {
			if(!((a->rects[i].y + a->rects[i].height < a->y1) ||
				(a->rects[i].y > a->y2))) {
				
				if(abs(a->rects[i].x - a->x1) <= abs(*a->delta))
					*a->delta = a->rects[i].x - a->x1;
				
				t_xy = a->rects[i].x + a->rects[i].width;
				if(abs(t_xy - a->x1) < abs(*a->delta))
					*a->delta = t_xy - a->x1;
			}
		}
	}
}

BlitzAlign
snap_rect(XRectangle *rects, int num, XRectangle *current,
          BlitzAlign *mask, int snap)
{
	SnapArgs a = { rects, num, 0, 0, 0, 0, *mask, nil };
	int dx = snap + 1, dy = snap + 1;
	BlitzAlign ret;

	a.x1 = current->x;
	a.x2 = current->x + current->width;
	a.delta = &dy;
	if(*mask & NORTH) {
		a.y2 = a.y1 = current->y;
		snap_line(&a);
	}
	if(*mask & SOUTH) {
		a.y2 = a.y1 = current->y + current->height;
		snap_line(&a);
	}
	a.y1 = current->y;
	a.y2 = current->y + current->height;
	a.delta = &dx;
	if(*mask & EAST) {
		a.x1 = a.x2 = current->x + current->width;
		snap_line(&a);
	}
	if(*mask & WEST) {
		a.x1 = a.x2 = current->x;
		snap_line(&a);
	}
	rect_morph_xy(current, abs(dx) <= snap ? dx : 0,
			abs(dy) <= snap ? dy : 0, mask);
	ret = *mask;
	if(abs(dx) <= snap)
		ret ^= EAST|WEST;
	if(abs(dy) <= snap)
		ret ^= NORTH|SOUTH;
	return ret ^ CENTER;
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
	if(xor.height > 4 && xor.width > 2)
		XDrawLine(blz.dpy, blz.root, xorgc, xor.x + 2, xor.y +  xor.height / 2,
			xor.x + xor.width - 2, xor.y + xor.height / 2);
	if(xor.width > 4 && xor.height > 2)
		XDrawLine(blz.dpy, blz.root, xorgc, xor.x + xor.width / 2, xor.y + 2,
			xor.x + xor.width / 2, xor.y + xor.height - 2);
	XSetLineAttributes(blz.dpy, xorgc, 4, LineSolid, CapNotLast, JoinMiter);
	XDrawRectangles(blz.dpy, blz.root, xorgc, &xor, 1);
	XSync(blz.dpy, False);
}

static void
find_droppoint(Frame *frame, int x, int y, XRectangle *rect, Bool do_move) {
	View *v;
	Area *a, *a_prev;
	Frame *f, *f_close;

	v = frame->view;
	rect->y = 0;
	rect->height = screen->rect.height - screen->brect.height;

	a_prev = v->area;
	for(a = a_prev->next; a && a->next; a = a->next) {
		if(x < (a->rect.x + a->rect.width))
			break;
		a_prev = a;
	}
	if(x < (a->rect.x + labelh(&def.font))) {
		rect->x = a->rect.x - 4;
		rect->width = 8;

		if(do_move) {
			a = new_column(v, a_prev, 0);
			send_to_area(a, frame->area, frame);
			focus(frame->client, False);
		}
		return;
	}
	if(x > (a->rect.x + a->rect.width - labelh(&def.font))) {
		rect->x = a->rect.x + a->rect.width - 4;
		rect->width = 8;

		if(do_move) {
			a = new_column(v, a, 0);
			send_to_area(a, frame->area, frame);
			focus(frame->client, False);
		}
		return;
	}

	rect->x = a->rect.x;
	rect->width = a->rect.width;

	f_close = nil;
	for(f = a->frame; f; f = f->anext) {
		if(y < f->rect.y)
			break;
		if(y < (f->rect.y + f->rect.height))
			break;
		f_close = f;
	}
	if(f == nil)
		f = f_close;
	if(y < (f->rect.y + labelh(&def.font))) {
		rect->y = f->rect.y;
		rect->height = 2;
		if(f_close) {
			rect->y = (f_close->rect.y + f_close->rect.height);
			rect->height = f->rect.y - rect->y;
		}
		if(do_move) {
			if(frame == f)
				return;
			if(a != frame->area)
				send_to_area(a, frame->area, frame);
			remove_frame(frame);
			insert_frame(f, frame, True);
			focus(frame->client, True);
		}
		return;
	}
	if(y > (f->rect.y + f->rect.height - labelh(&def.font))) {
		rect->y = f->rect.y + f->rect.height;
		rect->height = (screen->rect.height - labelh(&def.font) - rect->y);
		if(f->anext)
			rect->height = (f->anext->rect.y - rect->y);
		if(do_move) {
			if(frame == f)
				return;
			if(a != frame->area)
				send_to_area(a, frame->area, frame);
			remove_frame(frame);
			insert_frame(f, frame, False);
			focus(frame->client, True);
		}
		return;
	}
	*rect = f->rect;
	if(do_move) {
		swap_frames(frame, f);
		focus(frame->client, False);
	}
}

static void
do_managed_move(Client *c) {
	XRectangle frect, ofrect;
	Window dummy;
	XEvent ev;
	Frame *f;
	unsigned int di;
	int x, y, i;

	focus(c, False);
	f = c->frame;

	XSync(blz.dpy, False);
	if(XGrabPointer(blz.dpy, c->framewin, False, MouseMask, GrabModeAsync, GrabModeAsync,
			None, cursor[CurMove], CurrentTime) != GrabSuccess)
		return;
	XGrabServer(blz.dpy);

	XQueryPointer(blz.dpy, blz.root, &dummy, &dummy, &i, &i, &x, &y, &di);

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
			(handler[Expose])(&ev);
			break;
		default: break;
		}
	}
}

void
do_mouse_resize(Client *c, BlitzAlign align) {
	BlitzAlign grav;
	Window dummy;
	Cursor cur;
	XEvent ev;
	XRectangle *rects, ofrect, frect, origin;
	int snap, dx, dy, pt_x, pt_y, hr_x, hr_y, i;
	unsigned int num, di;
	Bool floating;
	float rx, ry;
	Frame *f;

	f = c->sel;
	floating = f->area->floating;
	origin = frect = f->rect;
	rects = floating ? rects_of_view(f->area->view, &num) : nil;
	snap = floating ? screen->rect.height / 66 : 0;
	cur = cursor[CurResize];
	if(align == CENTER)
		cur = cursor[CurInvisible];
	
	if(!floating && (align == CENTER)) {
		do_managed_move(c);
		return;
	}

	XQueryPointer(blz.dpy, c->framewin, &dummy, &dummy, &i, &i, &pt_x, &pt_y, &di);
	rx = (float)pt_x / frect.width;
	ry = (float)pt_y / frect.height;

	if(XGrabPointer(blz.dpy, c->framewin, False, MouseMask, GrabModeAsync, GrabModeAsync,
			None, cur, CurrentTime) != GrabSuccess)
		return;

	if (align != CENTER) {
		pt_x = dx = frect.width / 2;
		pt_y = dy = frect.height / 2;
		if(align&NORTH) dy -= pt_y;
		if(align&SOUTH) dy += pt_y;
		if(align&EAST) dx += pt_x;
		if(align&WEST) dx -= pt_x;
		XWarpPointer(blz.dpy, None, c->framewin, 0, 0, 0, 0, dx, dy);
	}
	else {
		hr_x = screen->rect.width / 2;
		hr_y = screen->rect.height / 2;
		XWarpPointer(blz.dpy, None, blz.root, 0, 0, 0, 0, hr_x, hr_y);
	}

	XQueryPointer(blz.dpy, blz.root, &dummy, &dummy, &i, &i, &pt_x, &pt_y, &di);

	XSync(blz.dpy, False);
	XGrabServer(blz.dpy);

	draw_xor_border(&frect);
	for(;;) {
		XMaskEvent(blz.dpy, MouseMask | ExposureMask, &ev);
		switch (ev.type) {
		case ButtonRelease:
			draw_xor_border(&frect);

			if(!floating)
				resize_column(c, &frect);
			else
				resize_client(c, &frect);

			if(rects)
				free(rects);

			XTranslateCoordinates(blz.dpy, c->framewin, blz.root,
					frect.width * rx, frect.height * ry,
					&dx, &dy, &dummy);
			if(dy > screen->brect.y)
				dy = screen->brect.y - 1;
			XWarpPointer(blz.dpy, None, blz.root, 0, 0, 0, 0, dx, dy);

			XUngrabServer(blz.dpy);
			XUngrabPointer(blz.dpy, CurrentTime);
			XSync(blz.dpy, False);
			return;
		case MotionNotify:
			ofrect = frect;
			dx = ev.xmotion.x_root;
			dy = ev.xmotion.y_root;

			if(align == CENTER) {
				if(dx == hr_x && dy == hr_y)
					continue;
				XWarpPointer(blz.dpy, None, blz.root, 0, 0, 0, 0, hr_x, hr_y);
				dx -= hr_x;
				dy -= hr_y;
			}else{
				dx -= pt_x;
				dy -= pt_y;
			}
			pt_x += dx;
			pt_y += dy;

			rect_morph_xy(&origin, dx, dy, &align);
			if(align != CENTER) check_frame_constraints(&origin);
			frect = origin;

			if(floating)
				grav = snap_rect(rects, num, &frect, &align, snap);
			else
				grav = align ^ CENTER;

			match_sizehints(c, &frect, floating, grav);

			draw_xor_border(&ofrect);
			draw_xor_border(&frect);
			break;
		case Expose:
			(handler[Expose])(&ev);
			break;
		default: break;
		}
	}
}
