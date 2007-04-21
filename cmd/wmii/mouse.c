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

static Handlers handlers;

enum { OHoriz, OVert };
typedef struct Framewin Framewin;
struct Framewin {
	Window *w;
	Frame *f;
	Frame *fp;
	Rectangle fr;
	Area *ra;
	Rectangle gb;
	Point pt;
	int or;
	int n;
	int before;
};

static Rectangle
framerect(Framewin *f) {
	Rectangle r;
	Point p;

	r.min = ZP;
	if(f->or == OHoriz) {
		r.max.x = f->n;
		r.max.y = f->gb.max.y + f->gb.min.y;
		r = rectsubpt(r, Pt(0, Dy(r)/2));
	}else {
		r.max.x = f->gb.max.x + f->gb.min.x;
		r.max.y = f->n;
		r = rectsubpt(r, Pt(Dx(r)/2, 0));
	}
	r = rectaddpt(r, f->pt);
	
	/* Keep onscreen */
	p = ZP;
	p.x -= min(r.min.x, 0);
	p.x -= max(r.max.x - screen->r.max.x, 0);
	p.y -= max(r.max.y - screen->brect.min.y - Dy(r)/2, 0);
	return rectaddpt(r, p);
}

static void
frameadjust(Framewin *f, Point pt, int or, int n) {
	f->or = or;
	f->n = n;
	f->pt = pt;
	reshapewin(f->w, framerect(f));
}

static Framewin*
framewin(Frame *f, Point pt, int or, int n) {
	WinAttr wa;
	Framewin *fw;

	fw = emallocz(sizeof *fw);
	wa.override_redirect = True;	
	wa.event_mask = ExposureMask;
	fw->w = createwindow(&scr.root, Rect(0, 0, 1, 1), scr.depth, InputOutput, &wa, CWEventMask);
	fw->w->aux = fw;
	sethandler(fw->w, &handlers);

	fw->f = f;
	fw->gb = f->grabbox;
	frameadjust(fw, pt, or, n);

	mapwin(fw->w);
	raisewin(fw->w);

	return fw;	
}

static void
framedestroy(Framewin *f) {
	destroywindow(f->w);
	free(f);
}

static void
expose_event(Window *w, XExposeEvent *e) {
	Rectangle r;
	Framewin *f;
	Image *buf;
	CTuple *c;

	f = w->aux;
	c = &def.focuscolor;
	buf = screen->ibuf;
	
	r = rectsubpt(w->r, w->r.min);
	fill(buf, r, c->bg);
	border(buf, r, 1, c->border);
	border(buf, f->gb, 1, c->border);
	border(buf, insetrect(f->gb, -f->gb.min.x), 1, c->border);

	copyimage(w, r, buf, ZP);	
}

static Handlers handlers = {
	.expose = expose_event,
};

static int
_vsnap(Framewin *f, int y) {
	if(abs(f->n - y) < Dy(f->w->r)) {
		f->n = y;
		return 1;
	}
	return 0;
}

static void
vplace(Framewin *fw, Point pt) {
	Rectangle r;
	Frame *f;
	Area *a;
	View *v;
	int hr;

	v = screen->sel;
	r = fw->w->r;
	hr = Dy(r)/2;

	fw->n = pt.y;
	fw->before = 1;

	for(a = v->area->next; a->next; a = a->next)
		if(pt.x < a->r.max.x)
			break;
	fw->ra = a;

	for(f = a->frame; f->anext; f = f->anext) {
		if(f == fw->f)
			fw->before = 0;
		if(pt.y < f->r.max.y)
			break;
	}

	if(!f->collapsed) {
		fw->fp = f;
		fw->fr = fw->fp->r;
		if(f == fw->f) {
			fw->fp = f->aprev;
			fw->fr.max = f->r.max;
			if(_vsnap(fw, f->r.min.y+hr))
				goto done;
		}
		if(_vsnap(fw, f->r.max.y - hr)) {
			fw->fr.min.y = f->r.max.y - labelh(def.font);
			goto done;
		}
		if(_vsnap(fw, f->r.min.y+Dy(r)+hr)) {
			fw->fr.min.y = f->r.min.y + labelh(def.font);
			goto done;
		}
		if(f->aprev == nil || f->aprev->collapsed)
			_vsnap(fw, f->r.min.y);
		else if(_vsnap(fw, f->r.min.y-hr))
			fw->fp = f->aprev;
		fw->fr.min.y = fw->n - hr;
		if(fw->fp && fw->fp->anext == fw->f)
			fw->fr.max = fw->f->r.max;
		goto done;
	}

	if(pt.y < f->r.min.y + hr) {
		fw->n = f->r.min.y;
		 if(f->aprev && !f->aprev->collapsed)
			fw->n -= hr;
	}else {
		fw->n = f->r.max.y;
		if(f->anext == fw->f)
			fw->n += hr;
	}

done:
	pt.x = a->r.min.x;
	pt.y = fw->n;
	frameadjust(fw, pt, OHoriz, Dx(a->r));	
}

static void
hplace(Framewin *fw, Point pt) {
	Area *a;
	View *v;
	int minw;
	
	minw = Dx(screen->r)/NCOL;
	v = screen->sel;

	for(a = v->area->next; a->next; a = a->next)
		if(pt.x < a->r.max.x)
			break;

	fw->ra = nil;
	if(abs(pt.x - a->r.min.x) < minw/2) {
		pt.x = a->r.min.x;
		fw->ra = a->prev;
	}
	else if(abs(pt.x - a->r.max.x) < minw/2) {
		pt.x = a->r.max.x;
		fw->ra = a;
	}

	pt.y = a->r.min.y;
	frameadjust(fw, pt, OVert, Dy(a->r));	
}

static void
do_managed_move(Client *c) {
	Rectangle r;
	WinAttr wa;
	XEvent ev;
	Framewin *fw;
	Window *cwin;
	Frame *f;
	Point pt;
	int y;

	focus(c, False);
	f = c->sel;

	pt = querypointer(&scr.root);

	pt.x = f->area->r.min.x;
	fw = framewin(f, pt, OHoriz, Dx(f->area->r));
	
	r = screen->r;
	r.min.y += fw->gb.min.y + Dy(fw->gb)/2;
	r.max.y = r.min.y + 1;
	cwin = createwindow(&scr.root, r, 0, InputOnly, &wa, 0);
	mapwin(cwin);

horiz:
	XUngrabPointer(display, CurrentTime);
	if(!grabpointer(&scr.root, nil, cursor[CurIcon], MouseMask))
		goto done;
	warppointer(pt);
	vplace(fw, pt);
	for(;;) {
		XMaskEvent(display, MouseMask | ExposureMask, &ev);
		switch (ev.type) {
		default:
			break;
		case Expose:
			dispatch_event(&ev);
			break;
		case MotionNotify:
			pt.x = ev.xmotion.x_root;
			pt.y = ev.xmotion.y_root;

			vplace(fw, pt);
			break;
		case ButtonRelease:
			switch(ev.xbutton.button) {
			case 1:
				if(f->anext && (!f->aprev || fw->fp != f->aprev || fw->fp != f->aprev->aprev)) {
					f->anext->r.min.y = f->r.min.y;
					resize_frame(f->anext, f->anext->r);
				}
				else if(f->aprev) {
					if(fw->fp == f->aprev->aprev) {
						fw->fp = f->aprev->aprev;
						f->aprev->r = f->r;
					}else
						f->aprev->r.max.y = f->r.max.y;
					resize_frame(f->aprev, f->aprev->r);
				}

				remove_frame(f);
				f->area = fw->ra;
				insert_frame(fw->fp, f, False);

				if(f->aprev) {
					f->aprev->r.max.y = fw->fr.min.y;
					resize_frame(f->aprev, f->aprev->r);
				}
				else
					fw->fr.min.y = f->area->r.min.y;
				if(f->anext)
					fw->fr.max.y = f->anext->r.min.y;
				else
					fw->fr.max.y = f->area->r.max.y;
				resize_frame(f, fw->fr);

				arrange_view(f->view);
				goto done;
			}
			break;
		case ButtonPress:
			switch(ev.xbutton.button) {
			case 2:
				goto vert;
			}
			break;
		}
	}
vert:
	y = pt.y;
	XUngrabPointer(display, CurrentTime);
	if(!grabpointer(&scr.root, cwin, cursor[CurIcon], MouseMask))
		goto done;
	hplace(fw, pt);
	for(;;) {
		XMaskEvent(display, MouseMask | ExposureMask, &ev);
		switch (ev.type) {
		default:
			break;
		case Expose:
			dispatch_event(&ev);
			break;
		case MotionNotify:
			pt.x = ev.xmotion.x_root;
			pt.y = ev.xmotion.y_root;

			hplace(fw, pt);
			break;
		case ButtonRelease:
			switch(ev.xbutton.button) {
			case 1:
				if(fw->ra) {
					fw->ra = new_column(f->view, fw->ra, 0);
					send_to_area(fw->ra, f);
				}
				goto done;
			case 2:
				pt.y = y;
				goto horiz;
			}
			break;
		}
	}
done:
	XUngrabPointer(display, CurrentTime);
	framedestroy(fw);
	destroywindow(cwin);
}

static Window *
gethsep(Rectangle r) {
	Window *w;
	WinAttr wa;
	
	wa.background_pixel = def.normcolor.border;
	w = createwindow(&scr.root, r, scr.depth, InputOutput, &wa, CWBackPixel);
	mapwin(w);
	raisewin(w);
	return w;
}

void
mouse_resizecolframe(Frame *f, Align align) {
	WinAttr wa;
	XEvent ev;
	Window *cwin, *hwin;
	Divide *d;
	View *v;
	Area *a;
	Rectangle r;
	Point pt, min;

	assert((align&(EAST|WEST)) != (EAST|WEST));
	assert((align&(NORTH|SOUTH)) != (NORTH|SOUTH));

	v = screen->sel;
	for(a = v->area->next, d = divs; a; a = a->next, d = d->next)
		if(a == f->area) break;

	if(align&EAST)
		d = d->next;

	min.x = Dx(screen->r)/NCOL;
	min.y = frame_delta_h() + labelh(def.font);
	if(align&NORTH) {
		if(f->aprev) {
			r.min.y = f->aprev->r.min.y + min.y;
			r.max.y = f->r.max.y - min.y;
		}else {
			r.min.y = a->r.min.y;
			r.max.y = r.min.y + 1;
		}
	}else {
		if(f->anext) {
			r.max.y = f->anext->r.max.y - min.y;
			r.min.y = f->r.min.y + min.y;
		}else {
			r.max.y = a->r.max.y;
			r.min.y = r.max.y - 1;
		}
	}
	if(align&WEST) {
		if(a->prev) {
			r.min.x = a->prev->r.min.x + min.x;
			r.max.x = a->r.max.x - min.x;
		}else {
			r.min.x = a->r.min.x;
			r.max.x = r.min.x + 1;
		}
	}else {
		if(a->next) {
			r.max.x = a->next->r.max.x - min.x;
			r.min.x = a->r.min.x + min.x;
		}else {
			r.max.x = a->r.max.x;
			r.min.x = r.max.x - 1;
		}
	}

	cwin = createwindow(&scr.root, r, 0, InputOnly, &wa, 0);
	mapwin(cwin);

	r = f->r;
	if(align&NORTH)
		r.min.y--;
	else
		r.min.y = r.max.y - 1;
	r.max.y = r.min.y + 2;

	hwin = gethsep(r);

	if(!grabpointer(&scr.root, cwin, cursor[CurSizing], MouseMask))
		goto done;
	
	pt.x = ((align&WEST) ? f->r.min.x : f->r.max.x);
	pt.y = ((align&NORTH) ? f->r.min.y : f->r.max.y);
	warppointer(pt);

	for(;;) {
		XMaskEvent(display, MouseMask | ExposureMask, &ev);
		switch (ev.type) {
		default:
			break;
		case Expose:
			dispatch_event(&ev);
			break;
		case MotionNotify:
			pt.x = ev.xmotion.x_root;
			pt.y = ev.xmotion.y_root;

			if(align&WEST)
				r.min.x = pt.x;
			else
				r.max.x = pt.x;
			r.min.y = ((align&SOUTH) ? pt.y : pt.y-1);
			r.max.y = r.min.y+2;

			setdiv(d, pt.x);
			reshapewin(hwin, r);
			break;
		case ButtonRelease:
			r = f->r;
			if(align&WEST)
				r.min.x = pt.x;
			else
				r.max.x = pt.x;
			if(align&NORTH)
				r.min.y = pt.y;
			else
				r.max.y = pt.y;
			resize_colframe(f, &r);

			if(align&WEST)
				pt.x = f->r.min.x + 4;
			else
				pt.x = f->r.max.x - 4;
			if(align&NORTH)
				pt.y = f->r.min.y + 4;
			else
				pt.y = f->r.max.y - 4;
			warppointer(pt);
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
	Point pt;
	uint minw;

	v = screen->sel;

	for(a = v->area->next, dp = divs; a; a = a->next, dp = dp->next)
		if(dp->next == d) break;

	/* Fix later */
	if(a == nil || a->next == nil)
		return;

	pt = querypointer(&scr.root);

	minw = Dx(screen->r)/NCOL;
	r.min.x = a->r.min.x + minw;
	r.max.x = a->next->r.max.x - minw;
	r.min.y = pt.y;
	r.max.y = pt.y+1;

	cwin = createwindow(&scr.root, r, 0, InputOnly, &wa, 0);
	mapwin(cwin);

	if(!grabpointer(&scr.root, cwin, cursor[CurInvisible], MouseMask))
		goto done;

	for(;;) {
		XMaskEvent(display, MouseMask | ExposureMask, &ev);
		switch (ev.type) {
		default:
			break;
		case Expose:
			dispatch_event(&ev);
			break;
		case MotionNotify:
			pt.x = ev.xmotion.x_root;
			setdiv(d, pt.x);
			break;
		case ButtonRelease:
			resize_column(a, pt.x - a->r.min.x);
			goto done;
		}
	}
done:
	XUngrabPointer(display, CurrentTime);
	destroywindow(cwin);
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

static int
snap_line(Rectangle *rects, int nrect, int d, int horiz, Rectangle *r, int x, int y) {
	Rectangle *rp;
	int i, tx, ty;

	if(horiz) {
		for(i=0; i < nrect; i++) {
			rp = &rects[i];
			if((rp->min.x <= r->max.x) && (rp->max.x >= r->min.x)) {
				ty = rp->min.y;
				if(abs(ty - y) <= abs(d))
					d = ty - y;

				ty = rp->max.y;
				if(abs(ty - y) <= abs(d))
					d = ty - y;
			}
		}
	}else {
		for(i=0; i < nrect; i++) {
			rp = &rects[i];
			if((rp->min.y <= r->max.y) && (rp->max.y >= r->min.y)) {
				tx = rp->min.x;
				if(abs(tx - x) <= abs(d))
					d = tx - x;

				tx = rp->max.x;
				if(abs(tx - x) <= abs(d))
					d = tx - x;
			}
		}
	}
	return d;
}

/* Returns a gravity for increment handling. It's normally the opposite of the mask
 * (the directions that we're resizing in), unless a snap occurs, in which case, it's the
 * direction of the snap.
 */
Align
snap_rect(Rectangle *rects, int num, Rectangle *r, Align *mask, int snap) {
	Align ret;
	Point d;
	
	d.x = snap+1;
	d.y = snap+1;

	if(*mask&NORTH)
		d.y = snap_line(rects, num, d.y, True, r, 0, r->min.y);
	if(*mask&SOUTH)
		d.y = snap_line(rects, num, d.y, True, r, 0, r->max.y);

	if(*mask&EAST)
		d.x = snap_line(rects, num, d.x, False, r, r->max.x, 0);
	if(*mask&WEST)
		d.x = snap_line(rects, num, d.x, False, r, r->min.x, 0);

	ret = CENTER;
	if(abs(d.x) <= snap)
		ret ^= EAST|WEST;
	else
		d.x = 0;

	if(abs(d.y) <= snap)
		ret ^= NORTH|SOUTH;
	else
		d.y = 0;

	rect_morph_xy(r, d, mask);
	return ret ^ *mask;
}

void
do_mouse_resize(Client *c, Bool opaque, Align align) {
	XEvent ev;
	Rectangle *rects;
	Rectangle frect, origin;
	Align grav;
	Cursor cur;
	Point d, pt, hr;
	float rx, ry, hrx, hry;
	uint num;
	Frame *f;

	f = c->sel;

	if(!f->area->floating) {
		if(align==CENTER)
			do_managed_move(c);
		else
			mouse_resizecolframe(f, align);
		return;
	}

	origin = frect = f->r;
	rects = rects_of_view(f->area->view, &num, c->frame);

	cur = cursor_of_quad(align);
	if((align==CENTER) && !opaque)
		cur = cursor[CurSizing];

	pt = querypointer(c->framewin);
	rx = (float)pt.x / Dx(frect);
	ry = (float)pt.y /Dy(frect);

	if(!grabpointer(c->framewin, nil, cur, MouseMask))
		return;

	pt = querypointer(&scr.root);

	hr = subpt(frect.max, frect.min);
	hr = divpt(hr, Pt(2, 2));

	if(align != CENTER) {
		d = hr;

		if(align&NORTH) d.y -= hr.y;
		if(align&SOUTH) d.y += hr.y;
		if(align&EAST) d.x += hr.x;
		if(align&WEST) d.x -= hr.x;

		pt = addpt(d, f->r.min);
		warppointer(pt);
	}
	else if(f->client->fullscreen) {
		XUngrabPointer(display, CurrentTime);
		return;
	}
	else if(!opaque) {
		hrx = (double)(Dx(screen->r) + Dx(frect) - 2 * labelh(def.font))
				/ Dx(screen->r);
		hry = (double)(Dy(screen->r)  + Dy(frect) - 3 * labelh(def.font))
				/ Dy(screen->r);

		pt.x = frect.max.x - labelh(def.font);
		pt.y = frect.max.y - labelh(def.font);
		d.x = pt.x / hrx;
		d.y = pt.y / hry;

		warppointer(d);
		flushevents(PointerMotionMask, False);
	}

	for(;;) {
		XMaskEvent(display, MouseMask | ExposureMask, &ev);
		switch (ev.type) {
		default:
			break;
		case Expose:
			dispatch_event(&ev);
			break;
		case MotionNotify:
			d.x = ev.xmotion.x_root;
			d.y = ev.xmotion.y_root;

			if(align == CENTER && !opaque) {
				d.x = (d.x * hrx) - pt.x;
				d.y = (d.y * hry) - pt.y;
			}else
				d = subpt(d, pt);
			pt = addpt(pt, d);

			rect_morph_xy(&origin, d, &align);
			origin = constrain(origin);
			frect = origin;

			grav = snap_rect(rects, num, &frect, &align, def.snap);

			frect = frame_hints(f, frect, grav);
			frect = constrain(frect);

			reshapewin(c->framewin, frect);
			break;
		case ButtonRelease:
			resize_client(c, &frect);

			if(!opaque) {
				pt = translate(c->framewin, &scr.root,
					Pt(Dx(frect)*rx, Dy(frect)*ry));
				if(pt.y > screen->brect.min.y)
					pt.y = screen->brect.min.y - 1;
				warppointer(pt);
			}

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
