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

static Handlers handlers;

enum { OHoriz, OVert };
typedef struct Framewin Framewin;
struct Framewin {
	/* Todo... give these better names. */
	Window* w;
	Rectangle grabbox;
	Rectangle fprev_r;
	Frame*	fprev;
	Frame*	f;
	Area*	ra;
	Point	pt;
	int	orientation;
	int	xy;
};

static Rectangle
framerect(Framewin *f) {
	Rectangle r;
	Point p;

	r.min = ZP;
	if(f->orientation == OHoriz) {
		r.max.x = f->xy;
		r.max.y = f->grabbox.max.y + f->grabbox.min.y;
		r = rectsubpt(r, Pt(0, Dy(r)/2));
	}else {
		r.max.x = f->grabbox.max.x + f->grabbox.min.x;
		r.max.y = f->xy;
		r = rectsubpt(r, Pt(Dx(r)/2, 0));
	}
	r = rectaddpt(r, f->pt);

	/* Keep onscreen */
	p = ZP;
	p.x -= min(0, r.min.x);
	p.x -= max(0, r.max.x - screen->r.max.x);
	p.y -= max(0, r.max.y - screen->brect.min.y - Dy(r)/2);
	return rectaddpt(r, p);
}

static void
frameadjust(Framewin *f, Point pt, int orientation, int xy) {
	f->orientation = orientation;
	f->xy = xy;
	f->pt = pt;
}

static Framewin*
framewin(Frame *f, Point pt, int orientation, int n) {
	WinAttr wa;
	Framewin *fw;

	fw = emallocz(sizeof *fw);
	wa.override_redirect = true;	
	wa.event_mask = ExposureMask;
	fw->w = createwindow(&scr.root, Rect(0, 0, 1, 1),
			scr.depth, InputOutput,
			&wa, CWEventMask);
	fw->w->aux = fw;
	sethandler(fw->w, &handlers);

	fw->f = f;
	fw->grabbox = f->grabbox;
	frameadjust(fw, pt, orientation, n);
	reshapewin(fw->w, framerect(fw));

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
	
	USED(e);

	f = w->aux;
	c = &def.focuscolor;
	buf = screen->ibuf;
	
	r = rectsubpt(w->r, w->r.min);
	fill(buf, r, c->bg);
	border(buf, r, 1, c->border);
	border(buf, f->grabbox, 1, c->border);
	border(buf, insetrect(f->grabbox, -f->grabbox.min.x), 1, c->border);

	copyimage(w, r, buf, ZP);	
}

static Handlers handlers = {
	.expose = expose_event,
};

static int
_vsnap(Framewin *f, int y) {
	if(abs(f->xy - y) < Dy(f->w->r)) {
		f->xy = y;
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

	for(a = v->area->next; a->next; a = a->next)
		if(pt.x < a->r.max.x)
			break;
	fw->ra = a;

	pt.x = a->r.min.x;
	frameadjust(fw, pt, OHoriz, Dx(a->r));

	r = fw->w->r;
	hr = Dy(r)/2;
	fw->xy = pt.y;

	for(f = a->frame; f->anext; f = f->anext)
		if(pt.y < f->r.max.y)
			break;

	if(!f->collapsed) {
		fw->fprev = f;
		fw->fprev_r = f->r;

		if(f == fw->f) {
			fw->fprev = f->aprev;
			fw->fprev_r.max = f->r.max;
			if(_vsnap(fw, f->r.min.y+hr))
				goto done;
		}

		if(_vsnap(fw, f->r.max.y - hr)) {
			fw->fprev_r.min.y = f->r.max.y - labelh(def.font);
			goto done;
		}
		if(_vsnap(fw, f->r.min.y+Dy(r)+hr)) {
			fw->fprev_r.min.y = f->r.min.y + labelh(def.font);
			goto done;
		}
		if(f->aprev == nil || f->aprev->collapsed)
			_vsnap(fw, f->r.min.y);
		else if(_vsnap(fw, f->r.min.y-hr))
			fw->fprev = f->aprev;
		fw->fprev_r.min.y = pt.y - hr;
		if(fw->fprev && fw->fprev->anext == fw->f)
			fw->fprev_r.max = fw->f->r.max;
		goto done;
	}

	if(pt.y < f->r.min.y + hr) {
		pt.y = f->r.min.y;
		 if(f->aprev && !f->aprev->collapsed)
			pt.y -= hr;
	}else {
		pt.y = f->r.max.y;
		if(f->anext == fw->f)
			pt.y += hr;
	}

done:
	pt.x = a->r.min.x;
	pt.y = fw->xy;
	frameadjust(fw, pt, OHoriz, Dx(a->r));
	reshapewin(fw->w, framerect(fw));
}

static void
hplace(Framewin *fw, Point pt) {
	Area *a;
	View *v;
	int minw;
	
	v = screen->sel;
	minw = Dx(v->r)/NCOL;

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
	reshapewin(fw->w, framerect(fw));	
}

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

static Point
grabboxcenter(Frame *f) {
	Point p;

	p = addpt(f->r.min, f->grabbox.min);
	p.x += Dx(f->grabbox)/2;
	p.y += Dy(f->grabbox)/2;
	return p;
	/* Pretty, but not concise.
	pt = addpt(pt, divpt(subpt(f->grabbox.max,
				   f->grabbox.min),
			     Pt(2, 2)))
	*/
}

static int
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

static bool
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
	if(align&North) {
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
	if(align&West) {
		if(a->prev != v->area) {
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

#if 1
static int tvcol(Frame*);
static int thcol(Frame*);
static int tfloat(Frame*);

enum {
	TDone,
	TVCol,
	THCol,
	TFloat,
};

static int (*tramp[])(Frame*) = {
	0,
	tvcol,
	thcol,
	tfloat,
};

static void
trampoline(int fn, Frame *f) {

	while(fn > 0) {
		f->collapsed = false;
		fn = tramp[fn](f);
	}
	ungrabpointer();

	warppointer(grabboxcenter(f));
}

#  if 1
void
mouse_movegrabbox(Client *c) {
	Frame *f;

	f = c->sel;
	if(f->area->floating)
		trampoline(TFloat, f);
	else
		trampoline(THCol, f);
}
#  endif

static int
thcol(Frame *f) {
	Framewin *fw;
	Frame *fprev, *fnext;
	Area *a;
	Rectangle r;
	Point pt, pt2;
	uint button;
	int ret;

	focus(f->client, false);

	pt = querypointer(&scr.root);
	pt2.x = f->area->r.min.x;
	pt2.y = pt.y;
	fw = framewin(f, pt2, OHoriz, Dx(f->area->r));

	ret = TDone;
	if(!grabpointer(&scr.root, nil, cursor[CurIcon], MouseMask))
		goto done;

	warppointer(pt);
	vplace(fw, pt);
	for(;;)
		switch (readmouse(&pt, &button)) {
		case MotionNotify:
			vplace(fw, pt);
			break;
		case ButtonRelease:
			if(button != 1)
				continue;
			/* TODO: Fix... I think that this should be
			 * simpler, and should be elsewhere. But the
			 * expected behavior turns out to be more
			 * complex than one would suspect. The
			 * simpler algorithms tend not to do what
			 * you want.
			 */
			a = f->area;
			if(a->floating)
				area_detach(f);
			else {
				fprev = f->aprev;
				fnext = f->anext;
				column_remove(f);
				if(fnext
				&& (!fprev || (fw->fprev != fprev)
					   && (fw->fprev != fprev->aprev))) {
					fnext->r.min.y = f->r.min.y;
					frame_resize(fnext, fnext->r);
				}
				else if(fprev) {
					if(fw->fprev == fprev->aprev) {
						fw->fprev = fprev->aprev;
						fprev->r = f->r;
					}else
						fprev->r.max.y = f->r.max.y;
					frame_resize(fprev, fprev->r);
				}
			}

			column_insert(fw->ra, f, fw->fprev);

			r = fw->fprev_r;
			if(f->aprev) {
				f->aprev->r.max.y = r.min.y;
				frame_resize(f->aprev, f->aprev->r);
			}else
				r.min.y = f->area->r.min.y;

			if(f->anext)
				r.max.y = f->anext->r.min.y;
			else
				r.max.y = f->area->r.max.y;

			frame_resize(f, fw->fprev_r);

			if(!a->frame && !a->floating)
				area_destroy(a);
			view_arrange(f->view);
			goto done;
		case ButtonPress:
			if(button == 2)
				ret = TVCol;
			else if(button == 3)
				ret = TFloat;
			else
				continue;
			goto done;
		}
done:
	framedestroy(fw);
	return ret;
}

static int
tvcol(Frame *f) {
	Framewin *fw;
	Window *cwin;
	WinAttr wa;
	Rectangle r;
	Point pt, pt2;
	uint button;
	int ret;

	focus(f->client, false);

	pt = querypointer(&scr.root);
	pt2.x = pt.x;
	pt2.y = f->area->r.min.y;
	fw = framewin(f, pt2, OVert, Dy(f->view->r));

	r = f->view->r;
	r.min.y += fw->grabbox.min.y + Dy(fw->grabbox)/2;
	r.max.y = r.min.y + 1;
	cwin = createwindow(&scr.root, r, 0, InputOnly, &wa, 0);
	mapwin(cwin);

	ret = TDone;
	if(!grabpointer(&scr.root, cwin, cursor[CurIcon], MouseMask))
		goto done;

	hplace(fw, pt);
	for(;;)
		switch (readmouse(&pt, &button)) {
		case MotionNotify:
			hplace(fw, pt);
			continue;
		case ButtonPress:
			if(button == 2)
				ret = THCol;
			else if(button == 3)
				ret = TFloat;
			else
				continue;
			goto done;
		case ButtonRelease:
			if(button != 1)
				continue;
			if(fw->ra) {
				fw->ra = column_new(f->view, fw->ra, 0);
				area_moveto(fw->ra, f);
				view_arrange(f->view); /* I hate this. */
			}
			goto done;
		}

done:
	framedestroy(fw);
	destroywindow(cwin);
	return ret;
}

static int
tfloat(Frame *f) {
	Rectangle *rects;
	Rectangle frect, origin;
	Point pt, pt1;
	Client *c;
	Align align;
	uint nrect, button;
	int ret;

	c = f->client;
	if(!f->area->floating)
		area_moveto(f->view->area, f);
	map_frame(f->client);
	focus(f->client, false);

	ret = TDone;
	if(!grabpointer(c->framewin, nil, cursor[CurMove], MouseMask))
		return TDone;

	rects = view_rects(f->view, &nrect, f);
	origin = f->r;
	frect = f->r;

	pt = querypointer(&scr.root);
	pt1 = grabboxcenter(f);
	goto casmotion;
label:
	for(;;pt1=pt)
		switch (readmouse(&pt, &button)) {
		default: goto label; /* shut up ken */
		case MotionNotify:
		casmotion:
			origin = rectaddpt(origin, subpt(pt, pt1));
			origin = constrain(origin);
			frect = origin;

			align = Center;
			snap_rect(rects, nrect, &frect, &align, def.snap);

			frect = frame_hints(f, frect, Center);
			frect = constrain(frect);
			client_resize(c, frect);
			continue;
		case ButtonRelease:
			if(button != 1)
				continue;
			goto done;
		case ButtonPress:
			if(button != 3)
				continue;
			unmap_frame(f->client);
			ret = THCol;
			goto done;
		}
done:
	free(rects);
	return ret;
}

#endif

/* Doesn't belong here */
void
grab_button(XWindow w, uint button, ulong mod) {
	XGrabButton(display, button, mod, w, false, ButtonMask,
			GrabModeSync, GrabModeAsync, None, None);
	if((mod != AnyModifier) && (numlock_mask != 0)) {
		XGrabButton(display, button, mod | numlock_mask, w, false, ButtonMask,
			GrabModeSync, GrabModeAsync, None, None);
		XGrabButton(display, button, mod | numlock_mask | LockMask, w, false,
			ButtonMask, GrabModeSync, GrabModeAsync, None, None);
	}
}

