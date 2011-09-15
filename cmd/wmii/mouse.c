/* Copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
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

static Cursor
quad_cursor(Align align) {
	switch(align) {
	case NEast: return cursor[CurNECorner];
	case NWest: return cursor[CurNWCorner];
	case SEast: return cursor[CurSECorner];
	case SWest: return cursor[CurSWCorner];
	case South:
	case North: return cursor[CurDVArrow];
	case East:
	case West:  return cursor[CurDHArrow];
	default:    return cursor[CurMove];
	}
}

static bool
cwin_expose(Window *w, void *aux, XExposeEvent *e) {

	fill(w, rectsubpt(w->r, w->r.min), &def.focuscolor.bg);
	fill(w, w->r, &def.focuscolor.bg);
	return false;
}

static Handlers chandler = {
	.expose = cwin_expose,
};

Window*
constraintwin(Rectangle r) {
	Window *w;

	w = createwindow(&scr.root, r, 0, InputOnly, nil, 0);
	if(0) {
		Window *w2;

		w2 = createwindow(&scr.root, r, 0, InputOutput, nil, 0);
		selectinput(w2, ExposureMask);
		w->aux = w2;

		setborder(w2, 1, &def.focuscolor.border);
		sethandler(w2, &chandler);
		mapwin(w2);
		raisewin(w2);
	}
	mapwin(w);
	return w;
}

void
destroyconstraintwin(Window *w) {

	if(w->aux)
		destroywindow(w->aux);
	destroywindow(w);
}

static Window*
gethsep(Rectangle r) {
	Window *w;
	WinAttr wa;

	wa.background_pixel = pixelvalue(&scr.root, &def.normcolor.border);
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

/* Yes, yes, macros are evil. So are patterns. */
#define frob(x, y) \
	const Rectangle *rp;                                              \
	int i, tx;                                                        \
									  \
	for(i=0; i < nrect; i++) {                                        \
		rp = &rects[i];                                           \
		if((rp->min.y <= r->max.y) && (rp->max.y >= r->min.y)) {  \
			tx = rp->min.x;                                   \
			if(abs(tx - x) <= abs(dx))                        \
				dx = tx - x;                              \
									  \
			tx = rp->max.x;                                   \
			if(abs(tx - x) <= abs(dx))                        \
				dx = tx - x;                              \
		}                                                         \
	}                                                                 \
	return dx                                                         \

static int
snap_hline(const Rectangle *rects, int nrect, int dx, const Rectangle *r, int y) {
	frob(y, x);
}

static int
snap_vline(const Rectangle *rects, int nrect, int dx, const Rectangle *r, int x) {
	frob(x, y);
}

#undef frob

/* Returns a gravity for increment handling. It's normally the
 * opposite of the mask (the directions that we're resizing in),
 * unless a snap occurs, in which case, it's the direction of the
 * snap.
 */
Align
snap_rect(const Rectangle *rects, int num, Rectangle *r, Align *mask, int snap) {
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
		XMaskEvent(display, MouseMask|ExposureMask|PropertyChangeMask, &ev);
		debug_event(&ev);
		switch(ev.type) {
		case Expose:
		case NoExpose:
		case PropertyNotify:
			event_dispatch(&ev);
		default:
			Dprint(DEvent, "readmouse(): ignored: %E\n", &ev);
			continue;
		case ButtonPress:
		case ButtonRelease:
			*button = ev.xbutton.button;
		case MotionNotify:
			p->x = ev.xmotion.x_root;
			p->y = ev.xmotion.y_root;
			if(p->x == scr.rect.max.x - 1)
				p->x = scr.rect.max.x;
			if(p->y == scr.rect.max.y - 1)
				p->y = scr.rect.max.y;
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
	Window *cwin, *hwin = nil;
	Divide *d;
	View *v;
	Area *a;
	Rectangle r;
	Point pt, min;
	int s;

	assert((align&(East|West)) != (East|West));
	assert((align&(North|South)) != (North|South));

	f->collapsed = false;

	v = selview;
	d = divs;
	SET(a);
	foreach_column(v, s, a) {
		if(a == f->area)
			break;
		d = d->next;
	}

	if(align & East)
		d = d->next;

	min.x = column_minwidth();
	min.y = /*frame_delta_h() +*/ labelh(def.font);
	/* Set the limits of where this box may be dragged. */
#define frob(pred, f, aprev, rmin, rmax, plus, minus, xy, use_screen) BLOCK( \
		if(pred) {                                           \
			r.rmin.xy = f->aprev->r.rmin.xy plus min.xy; \
			r.rmax.xy = f->r.rmax.xy minus min.xy;       \
		}else if(use_screen) {                               \
			r.rmin.xy = v->r[f->screen].rmin.xy plus 1;  \
			r.rmax.xy = a->r.rmax.xy minus min.xy;       \
		}else {                                              \
			r.rmin.xy = a->r.rmin.xy;                    \
			r.rmax.xy = r.rmin.xy plus 1;                \
		})

	r = f->r;
	if(align & North)
		frob(f->aprev, f, aprev, min, max, +, -, y, false);
	else if(align & South)
		frob(f->anext, f, anext, max, min, -, +, y, false);
	if(align & West)
		frob(a->prev,  a, prev,  min, max, +, -, x, true);
	else if(align & East)
		frob(a->next,  a, next,  max, min, -, +, x, true);
#undef frob

	cwin = constraintwin(r);

	r = f->r;
	if(align & North)
		r.min.y--;
	else if(align & South)
		r.min.y = r.max.y - 1;
	r.max.y = r.min.y + 2;

	if(align & (North|South))
		hwin = gethsep(r);

	if(!grabpointer(&scr.root, cwin, cursor[CurSizing], MouseMask))
		goto done;

	pt.x = (align & West ? f->r.min.x : f->r.max.x);
	pt.y = (align & North ? f->r.min.y : f->r.max.y);
	warppointer(pt);

	while(readmotion(&pt)) {
		if(align & West)
			r.min.x = pt.x;
		else if(align & East)
			r.max.x = pt.x;

		if(align & South)
			r.min.y = pt.y;
		else if(align & North)
			r.min.y = pt.y - 1;
		r.max.y = r.min.y+2;

		if(align & (East|West))
			div_set(d, pt.x);
		if(hwin)
			reshapewin(hwin, r);
	}

	r = f->r;
	if(align & West)
		r.min.x = pt.x;
	else if(align & East)
		r.max.x = pt.x;
	if(align & North)
		r.min.y = pt.y;
	else if(align & South)
		r.max.y = pt.y;
	column_resizeframe(f, r);

	/* XXX: Magic number... */
	if(align & West)
		pt.x = f->r.min.x + 4;
	else if(align & East)
		pt.x = f->r.max.x - 4;

	if(align & North)
		pt.y = f->r.min.y + 4;
	else if(align & South)
		pt.y = f->r.max.y - 4;
	warppointer(pt);

done:
	ungrabpointer();
	destroyconstraintwin(cwin);
	if (hwin)
		destroywindow(hwin);
}

void
mouse_resizecol(Divide *d) {
	Window *cwin;
	View *v;
	Rectangle r;
	Point pt;
	int minw, scrn;

	v = selview;

	scrn = (d->left ? d->left : d->right)->screen;

	pt = querypointer(&scr.root);

	minw = column_minwidth();
	r.min.x = d->left  ? d->left->r.min.x + minw  : v->r[scrn].min.x;
	r.max.x = d->right ? d->right->r.max.x - minw : v->r[scrn].max.x;
	r.min.y = pt.y;
	r.max.y = pt.y+1;

	cwin = constraintwin(r);

	if(!grabpointer(&scr.root, cwin, cursor[CurNone], MouseMask))
		goto done;

	while(readmotion(&pt))
		div_set(d, pt.x);

	if(d->left)
		d->left->r.max.x = pt.x;
	else
		v->pad[scrn].min.x = pt.x - v->r[scrn].min.x;

	if(d->right)
		d->right->r.min.x = pt.x;
	else
		v->pad[scrn].max.x = pt.x - v->r[scrn].max.x;

	view_arrange(v);

done:
	ungrabpointer();
	destroyconstraintwin(cwin);
}

void
mouse_resize(Client *c, Align align, bool grabmod) {
	Rectangle *rects;
	Rectangle frect, origin;
	Align grav;
	Cursor cur;
	Point d, pt, hr;
	float rx, ry, hrx, hry;
	uint nrect;
	Frame *f;

	f = c->sel;
	if(f->client->fullscreen >= 0) {
		ungrabpointer();
		return;
	}
	if(!f->area->floating) {
		if(align==Center)
			mouse_movegrabbox(c, grabmod);
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
		hrx = (double)(Dx(scr.rect)
			     + Dx(frect)
			     - 2 * labelh(def.font))
		    / Dx(scr.rect);
		hry = (double)(Dy(scr.rect)
			     + Dy(frect)
			     - 3 * labelh(def.font))
		    / Dy(scr.rect);

		pt.x = frect.max.x - labelh(def.font);
		pt.y = frect.max.y - labelh(def.font);
		d.x = pt.x / hrx;
		d.y = pt.y / hry;

		warppointer(d);
	}
	sync();
	event_flush(PointerMotionMask, false);

	while(readmotion(&d)) {
		if(align == Center) {
			d.x = (d.x * hrx) - pt.x;
			d.y = (d.y * hry) - pt.y;
		}else
			d = subpt(d, pt);
		pt = addpt(pt, d);

		rect_morph(&origin, d, &align);
		frect = constrain(origin, -1);

		grav = snap_rect(rects, nrect, &frect, &align, def.snap);

		frect = frame_hints(f, frect, grav);
		frect = constrain(frect, -1);

		client_resize(c, frect);
	}

	pt = addpt(c->framewin->r.min,
		   Pt(Dx(frect) * rx,
		      Dy(frect) * ry));
	if(pt.y > scr.rect.max.y)
		pt.y = scr.rect.max.y - 1;
	warppointer(pt);

	free(rects);
	ungrabpointer();
}

static int
pushstack_down(Frame *f, int y) {
	int ret;
	int dh, dy;

	if(f == nil)
		return 0;;
	ret = 0;
	dy = y - f->colr.min.y;
	if(dy < 0)
		return 0;
	if(!f->collapsed) {
		dh = Dy(f->colr) - labelh(def.font);
		if(dy <= dh) {
			f->colr.min.y += dy;
			return dy;
		}else {
			f->collapsed = true;
			f->colr.min.y += dh;
			ret = dh;
			dy -= dh;
		}
	}
	dy = pushstack_down(f->anext, f->colr.max.y + dy);
	f->colr.min.y += dy;
	f->colr.max.y += dy;
	return ret + dy;
}

static int
pushstack_up(Frame *f, int y) {
	int ret;
	int dh, dy;

	if(f == nil)
		return 0;
	ret = 0;
	dy = f->colr.max.y - y;
	if(dy < 0)
		return 0;
	if(!f->collapsed) {
		dh = Dy(f->colr) - labelh(def.font);
		if(dy <= dh) {
			f->colr.max.y -= dy;
			return dy;
		}else {
			f->collapsed = true;
			f->colr.max.y -= dh;
			ret = dh;
			dy -= dh;
		}
	}
	dy = pushstack_up(f->aprev, f->colr.min.y - dy);
	f->colr.min.y -= dy;
	f->colr.max.y -= dy;
	return ret + dy;
}

static void
mouse_tempvertresize(Area *a, Point p) {
	Frame *fa, *fb, *f;
	Window *cwin;
	Rectangle r;
	Point pt;
	int incmode, nabove, nbelow;

	if(a->mode != Coldefault)
		return;

	for(fa=a->frame; fa; fa=fa->anext)
		if(p.y < fa->r.max.y + labelh(def.font)/2)
			break;
	if(!(fa && fa->anext))
		return;
	fb = fa->anext;
	nabove=0;
	nbelow=0;
	for(f=fa; f; f=f->aprev)
		nabove++;
	for(f=fa->anext; f; f=f->anext)
		nbelow++;

	incmode = def.incmode;
	def.incmode = IIgnore;
	resizing = true;
	column_arrange(a, false);

	r.min.x = p.x;
	r.max.x = p.x + 1;
	r.min.y = a->r.min.y + labelh(def.font) * nabove;
	r.max.y = a->r.max.y - labelh(def.font) * nbelow;
	cwin = constraintwin(r);

	if(!grabpointer(&scr.root, cwin, cursor[CurDVArrow], MouseMask))
		goto done;

	for(f=a->frame; f; f=f->anext)
		f->colr_old = f->colr;

	while(readmotion(&pt)) {
		for(f=a->frame; f; f=f->anext) {
			f->collapsed = false;
			f->colr = f->colr_old;
		}
		if(pt.y > p.y)
			pushstack_down(fb, pt.y);
		else
			pushstack_up(fa, pt.y);
		fa->colr.max.y = pt.y;
		fb->colr.min.y = pt.y;
		column_frob(a);
	}

done:
	ungrabpointer();
	destroyconstraintwin(cwin);
	def.incmode = incmode;
	resizing = false;
	column_arrange(a, false);
}

void
mouse_checkresize(Frame *f, Point p, bool exec) {
	Rectangle r;
	Cursor cur;
	int q;

	cur = cursor[CurNormal];
	if(rect_haspoint_p(f->crect, p)) {
		client_setcursor(f->client, cur);
		return;
	}

	r = rectsubpt(f->r, f->r.min);
	q = quadrant(r, p);
	if(rect_haspoint_p(f->grabbox, p)) {
		cur = cursor[CurTCross];
		if(exec)
			mouse_movegrabbox(f->client, false);
	}
	else if(f->area->floating) {
		if(p.x <= 2
		|| p.y <= 2
		|| r.max.x - p.x <= 2
		|| r.max.y - p.y <= 2) {
			cur = quad_cursor(q);
			if(exec)
				mouse_resize(f->client, q, false);
		}
		else if(exec && rect_haspoint_p(f->titlebar, p))
			mouse_movegrabbox(f->client, true);
	}else {
		if(f->aprev && p.y <= 2
		|| f->anext && r.max.y - p.y <= 2) {
			cur = cursor[CurDVArrow];
			if(exec)
				mouse_tempvertresize(f->area, addpt(p, f->r.min));
		}
	}

	if(!exec)
		client_setcursor(f->client, cur);
}

static void
_grab(XWindow w, uint button, ulong mod) {
	XGrabButton(display, button, mod, w, false, ButtonMask,
			GrabModeSync, GrabModeSync, None, None);
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

