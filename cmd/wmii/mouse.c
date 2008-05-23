/* Copyright ©2006-2008 Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */
#include "dat.h"
#include "fns.h"

/* Here be dragons. */

enum {
	ButtonMask =
		ButtonPressMask | ButtonReleaseMask,
	MouseMask =
		ButtonMask | PointerMotionMask
};

static Window*
gethsep(Rectangle r) {
	Window *w;
	WinAttr wa;
	
	wa.background_pixel = def.normcolor.border;
	w = createwindow(&scr.root, r, scr.depth, InputOutput, &wa, CWBackPixel);
	mapwin(w);
	raisewin(w);
	return w;
}

static void
rect_morph(Rectangle *r, Point d, Align *mask) {
	int n;

	if(*mask & North)
		r->min.y += d.y;
	if(*mask & West)
		r->min.x += d.x;
	if(*mask & South)
		r->max.y += d.y;
	if(*mask & East)
		r->max.x += d.x;

	if(r->min.x > r->max.x) {
		n = r->min.x;
		r->min.x = r->max.x;
		r->max.x = n;
		*mask ^= East|West;
	}
	if(r->min.y > r->max.y) {
		n = r->min.y;
		r->min.y = r->max.y;
		r->max.y = n;
		*mask ^= North|South;
	}
}

static int
snap_hline(Rectangle *rects, int nrect, int dy, Rectangle *r, int y) {
	Rectangle *rp;
	int i, ty;

	for(i=0; i < nrect; i++) {
		rp = &rects[i];
		if((rp->min.x <= r->max.x) && (rp->max.x >= r->min.x)) {
			ty = rp->min.y;
			if(abs(ty - y) <= abs(dy))
				dy = ty - y;

			ty = rp->max.y;
			if(abs(ty - y) <= abs(dy))
				dy = ty - y;
		}
	}
	return dy;
}

static int
snap_vline(Rectangle *rects, int nrect, int dx, Rectangle *r, int x) {
	Rectangle *rp;
	int i, tx;

	for(i=0; i < nrect; i++) {
		rp = &rects[i];
		if((rp->min.y <= r->max.y) && (rp->max.y >= r->min.y)) {
			tx = rp->min.x;
			if(abs(tx - x) <= abs(dx))
				dx = tx - x;

			tx = rp->max.x;
			if(abs(tx - x) <= abs(dx))
				dx = tx - x;
		}
	}
	return dx;
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

	if(*mask&North)
		d.y = snap_hline(rects, num, d.y, r, r->min.y);
	if(*mask&South)
		d.y = snap_hline(rects, num, d.y, r, r->max.y);

	if(*mask&East)
		d.x = snap_vline(rects, num, d.x, r, r->max.x);
	if(*mask&West)
		d.x = snap_vline(rects, num, d.x, r, r->min.x);

	ret = Center;
	if(abs(d.x) <= snap)
		ret ^= East|West;
	else
		d.x = 0;

	if(abs(d.y) <= snap)
		ret ^= North|South;
	else
		d.y = 0;

	rect_morph(r, d, mask);
	return ret ^ *mask;
}

int
readmouse(Point *p, uint *button) {
	XEvent ev;

	for(;;) {
		XMaskEvent(display, MouseMask|ExposureMask, &ev);
		switch(ev.type) {
		case Expose:
			dispatch_event(&ev);
		default:
			continue;
		case ButtonPress:
		case ButtonRelease:
			*button = ev.xbutton.button;
		case MotionNotify:
			p->x = ev.xmotion.x_root;
			p->y = ev.xmotion.y_root;
			break;
		}
		return ev.type;
	}
}

bool
readmotion(Point *p) {
	uint button;

	for(;;)
		switch(readmouse(p, &button)) {
		case MotionNotify:
			return true;
		case ButtonRelease:
			return false;
		}
}

static void
mouse_resizecolframe(Frame *f, Align align) {
	WinAttr wa;
	Window *cwin, *hwin;
	Divide *d;
	View *v;
	Area *a;
	Rectangle r;
	Point pt, min;

	assert((align&(East|West)) != (East|West));
	assert((align&(North|South)) != (North|South));

	f->collapsed = false;

	v = screen->sel;
	d = divs;
	for(a=v->area->next; a != f->area; a=a->next)
		d = d->next;

	if(align&East)
		d = d->next;

	min.x = Dx(v->r)/NCOL;
	min.y = /*frame_delta_h() +*/ labelh(def.font);
	/* This would be so simple in lisp... */
	/* This must be evil. But, I hate to repeat myself. */
	/* And I hate to see patterns. */
	/* At any rate, set the limits of where this box may be
	 * dragged.
	 */
#define frob(pred, rmin, rmax, plus, minus, xy) BLOCK( \
		if(pred) {                                           \
			r.rmin.xy = f->aprev->r.rmin.xy plus min.xy; \
			r.rmax.xy = f->r.rmax.xy minus min.xy;       \
		}else {                                              \
			r.rmin.xy = a->r.rmin.xy;                    \
			r.rmax.xy = r.rmin.xy plus 1;                \
		})
	if(align&North)
		frob(f->aprev,           min, max, +, -, y);
	else
		frob(f->anext,           max, min, -, +, y);
	if(align&West)
		frob(a->prev != v->area, min, max, +, -, x);
	else
		frob(a->next,            max, min, -, +, x);
#undef frob

	cwin = createwindow(&scr.root, r, 0, InputOnly, &wa, 0);
	mapwin(cwin);

	r = f->r;
	if(align&North)
		r.min.y--;
	else
		r.min.y = r.max.y - 1;
	r.max.y = r.min.y + 2;

	hwin = gethsep(r);

	if(!grabpointer(&scr.root, cwin, cursor[CurSizing], MouseMask))
		goto done;

	pt.x = ((align&West) ? f->r.min.x : f->r.max.x);
	pt.y = ((align&North) ? f->r.min.y : f->r.max.y);
	warppointer(pt);

	while(readmotion(&pt)) {
		if(align&West)
			r.min.x = pt.x;
		else
			r.max.x = pt.x;
		r.min.y = ((align&South) ? pt.y : pt.y-1);
		r.max.y = r.min.y+2;

		div_set(d, pt.x);
		reshapewin(hwin, r);
	}

	r = f->r;
	if(align&West)
		r.min.x = pt.x;
	else
		r.max.x = pt.x;
	if(align&North)
		r.min.y = pt.y;
	else
		r.max.y = pt.y;
	column_resizeframe(f, r);

	/* XXX: Magic number... */
	if(align&West)
		pt.x = f->r.min.x + 4;
	else
		pt.x = f->r.max.x - 4;
	if(align&North)
		pt.y = f->r.min.y + 4;
	else
		pt.y = f->r.max.y - 4;
	warppointer(pt);

done:
	ungrabpointer();
	destroywindow(cwin);
	destroywindow(hwin);
}

void
mouse_resizecol(Divide *d) {
	WinAttr wa;
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

	minw = Dx(v->r)/NCOL;
	r.min.x = a->r.min.x + minw;
	r.max.x = a->next->r.max.x - minw;
	r.min.y = pt.y;
	r.max.y = pt.y+1;

	cwin = createwindow(&scr.root, r, 0, InputOnly, &wa, 0);
	mapwin(cwin);

	if(!grabpointer(&scr.root, cwin, cursor[CurNone], MouseMask))
		goto done;

	while(readmotion(&pt))
		div_set(d, pt.x);

	column_resize(a, pt.x - a->r.min.x);

done:
	ungrabpointer();
	destroywindow(cwin);
}

void
mouse_resize(Client *c, Align align) {
	Rectangle *rects;
	Rectangle frect, origin;
	Align grav;
	Cursor cur;
	Point d, pt, hr;
	float rx, ry, hrx, hry;
	uint nrect;
	Frame *f;

	f = c->sel;
	if(f->client->fullscreen)
		return;
	if(!f->area->floating) {
		if(align==Center)
			mouse_movegrabbox(c);
		else
			mouse_resizecolframe(f, align);
		return;
	}

	cur = quad_cursor(align);
	if(align == Center)
		cur = cursor[CurSizing];

	if(!grabpointer(c->framewin, nil, cur, MouseMask))
		return;

	origin = f->r;
	frect = f->r;
	rects = view_rects(f->area->view, &nrect, c->frame);

	pt = querypointer(c->framewin);
	rx = (float)pt.x / Dx(frect);
	ry = (float)pt.y / Dy(frect);

	pt = querypointer(&scr.root);

	SET(hrx);
	SET(hry);
	if(align != Center) {
		hr = subpt(frect.max, frect.min);
		hr = divpt(hr, Pt(2, 2));
		d = hr;
		if(align&North) d.y -= hr.y;
		if(align&South) d.y += hr.y;
		if(align&East) d.x += hr.x;
		if(align&West) d.x -= hr.x;

		pt = addpt(d, f->r.min);
		warppointer(pt);
	}else {
		hrx = (double)(Dx(screen->r)
			     + Dx(frect)
			     - 2 * labelh(def.font))
		    / Dx(screen->r);
		hry = (double)(Dy(screen->r)
			     + Dy(frect)
			     - 3 * labelh(def.font))
		    / Dy(screen->r);

		pt.x = frect.max.x - labelh(def.font);
		pt.y = frect.max.y - labelh(def.font);
		d.x = pt.x / hrx;
		d.y = pt.y / hry;

		warppointer(d);
	}
	sync();
	flushevents(PointerMotionMask, false);

	while(readmotion(&d)) {
		if(align == Center) {
			d.x = (d.x * hrx) - pt.x;
			d.y = (d.y * hry) - pt.y;
		}else
			d = subpt(d, pt);
		pt = addpt(pt, d);

		rect_morph(&origin, d, &align);
		origin = constrain(origin);
		frect = origin;

		grav = snap_rect(rects, nrect, &frect, &align, def.snap);

		frect = frame_hints(f, frect, grav);
		frect = constrain(frect);

		client_resize(c, frect);
	}

	pt = addpt(c->framewin->r.min,
		   Pt(Dx(frect) * rx,
		      Dy(frect) * ry));
	if(pt.y > f->view->r.max.y)
		pt.y = f->view->r.max.y - 1;
	warppointer(pt);

	free(rects);
	ungrabpointer();
}

static void
_grab(XWindow w, uint button, ulong mod) {
	XGrabButton(display, button, mod, w, false, ButtonMask,
			GrabModeSync, GrabModeAsync, None, None);
}

/* Doesn't belong here */
void
grab_button(XWindow w, uint button, ulong mod) {
	_grab(w, button, mod);
	if((mod != AnyModifier) && numlock_mask) {
		_grab(w, button, mod | numlock_mask);
		_grab(w, button, mod | numlock_mask | LockMask);
	}
}

