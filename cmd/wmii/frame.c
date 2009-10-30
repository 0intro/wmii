/* Copyright ©2006-2009 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "dat.h"
#include <math.h>
#include "fns.h"

uint
frame_idx(Frame *f) {
	Frame *fp;
	uint i;

	fp = f->area->frame;
	for(i = 1; fp != f; fp = fp->anext)
		i++;
	return i;
}

Frame*
frame_create(Client *c, View *v) {
	static ushort id = 1;
	Frame *f;

	f = emallocz(sizeof *f);
	f->id = id++;
	f->client = c;
	f->view = v;

	if(c->sel) {
		f->floatr = c->sel->floatr;
		f->r = c->sel->r;
	}else {
		f->r = client_grav(c, c->r);
		f->floatr = f->r;
		c->sel = f;
	}
	f->collapsed = false;
	f->screen = -1;
	f->oldarea = -1;
	f->oldscreen = -1;

	return f;
}

void
frame_remove(Frame *f) {
	Area *a;

	a = f->area;
	if(f->aprev)
		f->aprev->anext = f->anext;
	if(f->anext)
		f->anext->aprev = f->aprev;
	if(f == a->frame)
		a->frame = f->anext;

	if(a->floating) {
		if(f->sprev)
			f->sprev->snext = f->snext;
		if(f->snext)
			f->snext->sprev = f->sprev;
		if(f == a->stack)
			a->stack = f->snext;
	}
	f->anext = f->aprev = f->snext = f->sprev = nil;
}

void
frame_insert(Frame *f, Frame *pos) {
	Area *a;

	a = f->area;

	if(pos) {
		assert(pos != f);
		f->aprev = pos;
		f->anext = pos->anext;
	}else {
		assert(f->area->frame != f);
		f->anext = f->area->frame;
		f->area->frame = f;
	}
	if(f->aprev)
		f->aprev->anext = f;
	if(f->anext)
		f->anext->aprev = f;

	if(a->floating) {
		assert(f->sprev == nil);
		frame_restack(f, nil);
	}
}

bool
frame_restack(Frame *f, Frame *above) {
	Client *c;
	Frame *fp;
	Area *a;

	c = f->client;
	a = f->area;
	if(!a->floating)
		return false;
	if(f == above)
		return false;

	if(above == nil && !(c->w.ewmh.type & TypeDock))
		for(fp=a->stack; fp; fp=fp->snext)
			if(fp->client->w.ewmh.type & TypeDock)
				above = fp;
			else
				break;

	if(f->sprev || f == a->stack)
	if(f->sprev == above)
		return false;

	if(f->sprev)
		f->sprev->snext = f->snext;
	else if(f->snext)
		a->stack = f->snext;
	if(f->snext)
		f->snext->sprev = f->sprev;

	f->sprev = above;
	if(above == nil) {
		f->snext = a->stack;
		a->stack = f;
	}
	else {
		f->snext = above->snext;
		above->snext = f;
	}
	if(f->snext)
		f->snext->sprev = f;
	assert(f->snext != f && f->sprev != f);

	return true;
}

/* Handlers */
static void
bup_event(Window *w, XButtonEvent *e) {
	if((e->state & def.mod) != def.mod)
		XAllowEvents(display, ReplayPointer, e->time);
	else
		XUngrabPointer(display, e->time);
	event("ClientClick %C %d\n", w->aux, e->button);
}

static void
bdown_event(Window *w, XButtonEvent *e) {
	Frame *f;
	Client *c;

	c = w->aux;
	f = c->sel;

	if((e->state & def.mod) == def.mod) {
		switch(e->button) {
		case Button1:
			focus(c, false);
			mouse_resize(c, Center, true);
			break;
		case Button2:
			frame_restack(f, nil);
			view_restack(f->view);
			focus(c, false);
			grabpointer(c->framewin, nil, cursor[CurNone], ButtonReleaseMask);
			break;
		case Button3:
			focus(c, false);
			mouse_resize(c, quadrant(f->r, Pt(e->x_root, e->y_root)), true);
			break;
		default:
			XAllowEvents(display, ReplayPointer, e->time);
			break;
		}
	}else {
		if(e->button == Button1) {
			if(!e->subwindow) {
				frame_restack(f, nil);
				view_restack(f->view);
				mouse_checkresize(f, Pt(e->x, e->y), true);
			}

			if(f->client != selclient())
				focus(c, false);
		}
		if(e->subwindow)
			XAllowEvents(display, ReplayPointer, e->time);
		else {
			/* Ungrab so a menu can receive events before the button is released */
			XUngrabPointer(display, e->time);
			sync();

			event("ClientMouseDown %C %d\n", f->client, e->button);
		}
	}
}

static void
config_event(Window *w, XConfigureEvent *e) {

	USED(w, e);
}

static void
enter_event(Window *w, XCrossingEvent *e) {
	Client *c;
	Frame *f;

	c = w->aux;
	f = c->sel;
	if(disp.focus != c || selclient() != c) {
		Dprint(DFocus, "enter_notify(f) => [%C]%s%s\n",
		       f->client, f->client->name,
		       ignoreenter == e->serial ? " (ignored)" : "");
		if(e->detail != NotifyInferior)
		if(e->serial != ignoreenter && (f->area->floating || !f->collapsed))
		if(!(c->w.ewmh.type & TypeSplash))
			focus(f->client, false);
	}
	mouse_checkresize(f, Pt(e->x, e->y), false);
}

static void
expose_event(Window *w, XExposeEvent *e) {
	Client *c;

	USED(e);

	c = w->aux;
	if(c->sel)
		frame_draw(c->sel);
	else
		fprint(2, "Badness: Expose event on a client frame which shouldn't be visible: %C\n",
			c);
}

static void
motion_event(Window *w, XMotionEvent *e) {
	Client *c;
	
	c = w->aux;
	mouse_checkresize(c->sel, Pt(e->x, e->y), false);
}

Handlers framehandler = {
	.bup = bup_event,
	.bdown = bdown_event,
	.config = config_event,
	.enter = enter_event,
	.expose = expose_event,
	.motion = motion_event,
};

WinHints
frame_gethints(Frame *f) {
	WinHints h;
	Client *c;
	Rectangle r;
	Point d;
	int minh;

	minh = labelh(def.font);

	c = f->client;
	h = *c->w.hints;

	r = frame_rect2client(c, f->r, f->area->floating);
	d.x = Dx(f->r) - Dx(r);
	d.y = Dy(f->r) - Dy(r);

	if(!f->area->floating && def.incmode == IIgnore)
		h.inc = Pt(1, 1);

	if(h.min.x < 2*minh)
		h.min.x = minh + (2*minh) % h.inc.x;
	if(h.min.y < minh)
		h.min.y = minh + minh % h.inc.y;

	h.min.x += d.x;
	h.min.y += d.y;
	/* Guard against overflow. */
	if(h.max.x + d.x > h.max.x)
		h.max.x += d.x;
	if(h.max.y + d.y > h.max.y)
		h.max.y += d.y;

	h.base.x += d.x;
	h.base.y += d.y;
	h.baspect.x += d.x;
	h.baspect.y += d.y;

	h.group = 0;
	h.grav = ZP;
	h.gravstatic = 0;
	h.position = 0;
	return h;
}

#define ADJ(PE, ME) \
	if(c->fullscreen >= 0)                       \
		return r;                            \
						     \
	if(!floating) {                              \
		r.min.x PE 1;                        \
		r.min.y PE labelh(def.font);         \
		r.max.x ME 1;                        \
		r.max.y ME 1;                        \
	}else {                                      \
		if(!c->borderless) {                 \
			r.min.x PE def.border;       \
			r.max.x ME def.border;       \
			r.max.y ME def.border;       \
		}                                    \
		if(!c->titleless)                    \
			r.min.y PE labelh(def.font); \
	}                                            \

Rectangle
frame_rect2client(Client *c, Rectangle r, bool floating) {

	ADJ(+=, -=)

	/* Force clients to be at least 1x1 */
	r.max.x = max(r.max.x, r.min.x+1);
	r.max.y = max(r.max.y, r.min.y+1);
	return r;
}

Rectangle
frame_client2rect(Client *c, Rectangle r, bool floating) {

	ADJ(-=, +=)

	return r;
}

#undef ADJ

void
frame_resize(Frame *f, Rectangle r) {
	Client *c;
	Rectangle fr, cr;
	int collapsed, dx;

	if(btassert("8 full", Dx(r) <= 0 || Dy(r) < 0
		           || Dy(r) == 0 && (!f->area->max || resizing)
			      && !f->collapsed)) {
		fprint(2, "Frame rect: %R\n", r);
		r.max.x = min(r.min.x+1, r.max.x);
		r.max.y = min(r.min.y+1, r.max.y);
	}

	c = f->client;
	if(c->fullscreen >= 0) {
		f->r = screens[c->fullscreen]->r;
		f->crect = rectsetorigin(f->r, ZP);
		return;
	}

	/*
	if(f->area->floating)
		f->collapsed = false;
	*/

	fr = frame_hints(f, r, get_sticky(f->r, r));
	if(f->area->floating && !c->strut)
		fr = constrain(fr, -1);

	/* Collapse managed frames which are too small */
	/* XXX. */
	collapsed = f->collapsed;
	if(!f->area->floating && f->area->mode == Coldefault) {
		f->collapsed = false;
		if(Dy(r) < 2 * labelh(def.font))
			f->collapsed = true;
	}
	if(collapsed != f->collapsed)
		ewmh_updatestate(c);

	fr.max.x = max(fr.max.x, fr.min.x + 2*labelh(def.font));
	if(f->collapsed && f->area->floating)
		fr.max.y = fr.min.y + labelh(def.font);

	cr = frame_rect2client(c, fr, f->area->floating);
	if(f->area->floating)
		f->r = fr;
	else {
		f->r = r;
		dx = Dx(r) - Dx(cr);
		dx -= 2 * (cr.min.x - fr.min.x);
		cr.min.x += dx / 2;
		cr.max.x += dx / 2;
	}
	f->crect = rectsubpt(cr, f->r.min);

	if(f->area->floating && !f->collapsed)
		f->floatr = f->r;
}

static void
pushlabel(Image *img, Rectangle *rp, char *s, CTuple *col) {
	Rectangle r;
	int w;

	w = textwidth(def.font, s) + def.font->height;
	w = min(w, Dx(*rp) - 30); /* Magic number. */
	if(w > 0) {
		r = *rp;
		rp->max.x -= w;
		if(0)
		drawline(img, Pt(rp->max.x, r.min.y+2),
			      Pt(rp->max.x, r.max.y-2),
			      CapButt, 1, col->border);
		drawstring(img, def.font, r, East,
			   s, col->fg);
	}
}

void
frame_draw(Frame *f) {
	Rectangle r, fr;
	Client *c;
	CTuple *col;
	Image *img;
	char *s;
	uint w;
	int n, m;

	if(f->view != selview)
		return;
	if(f->area == nil) /* Blech. */
		return;

	c = f->client;
	img = *c->ibuf;
	fr = rectsetorigin(c->framewin->r, ZP);

	/* Pick colors. */
	if(c == selclient() || c == disp.focus)
		col = &def.focuscolor;
	else
		col = &def.normcolor;

	/* Background/border */
	r = fr;
	fill(img, r, col->bg);
	border(img, r, 1, col->border);

	/* Title border */
	r.max.y = r.min.y + labelh(def.font);
	border(img, r, 1, col->border);

	f->titlebar = insetrect(r, 3);
	f->titlebar.max.y += 3;

	/* Odd focus. Unselected, with keyboard focus. */
	/* Draw a border just inside the titlebar. */
	if(c != selclient() && c == disp.focus) {
		border(img, insetrect(r, 1), 1, def.normcolor.bg);
		border(img, insetrect(r, 2), 1, def.focuscolor.border);
	}

	/* grabbox */
	r.min = Pt(2, 2);
	r.max.y -= 2;
	r.max.x = r.min.x + Dy(r);
	f->grabbox = r;

	if(c->urgent)
		fill(img, r, col->fg);
	border(img, r, 1, col->border);

	/* Odd focus. Selected, without keyboard focus. */
	/* Draw a border around the grabbox. */
	if(c != disp.focus && col == &def.focuscolor)
		border(img, insetrect(r, -1), 1, def.normcolor.bg);

	/* Draw a border on borderless+titleless selected apps. */
	if(f->area->floating && c->borderless && c->titleless && !c->fullscreen && c == selclient())
		setborder(c->framewin, def.border, def.focuscolor.border);
	else
		setborder(c->framewin, 0, 0);

	/* Label */
	r.min.x = r.max.x;
	r.max.x = fr.max.x;
	r.min.y = 0;
	r.max.y = labelh(def.font);
	/* Draw count on frames in 'max' columns. */
	if(f->area->max && !resizing) {
		/* XXX */
		n = stack_count(f, &m);
		s = smprint("%d/%d", m, n);
		pushlabel(img, &r, s, col);
		free(s);
	}
	/* Label clients with extra tags. */
	if((s = client_extratags(c))) {
		pushlabel(img, &r, s, col);
		free(s);
	}else /* Make sure floating clients have room for their indicators. */
	if(c->floating)
		r.max.x -= Dx(f->grabbox);
	w = drawstring(img, def.font, r, West,
			c->name, col->fg);

	/* Draw inner border on floating clients. */
	if(f->area->floating) {
		r.min.x = r.min.x + w + 10;
		r.max.x += Dx(f->grabbox) - 2;
		r.min.y = f->grabbox.min.y;
		r.max.y = f->grabbox.max.y;
		border(img, r, 1, col->border);
	}

	/* Border increment gaps... */
	r.min.y = f->crect.min.y;
	r.min.x = max(1, f->crect.min.x - 1);
	r.max.x = min(fr.max.x - 1, f->crect.max.x + 1);
	r.max.y = min(fr.max.y - 1, f->crect.max.y + 1);
	border(img, r, 1, col->border);

	/* Why? Because some non-ICCCM-compliant apps feel the need to
	 * change the background properties of all of their ancestor windows
	 * in order to implement pseudo-transparency.
	 * What's more, the designers of X11 felt that it would be unfair to
	 * implementers to make it possible to detect, or forbid, such changes.
	 */
	XSetWindowBackgroundPixmap(display, c->framewin->xid, None);

	copyimage(c->framewin, fr, img, ZP);
}

void
frame_draw_all(void) {
	Client *c;

	for(c=client; c; c=c->next)
		if(c->sel && c->sel->view == selview)
			frame_draw(c->sel);
}

void
frame_swap(Frame *fa, Frame *fb) {
	Frame **fp;
	Client *c;

	if(fa == fb) return;

	for(fp = &fa->client->frame; *fp; fp = &fp[0]->cnext)
		if(*fp == fa) break;
	fp[0] = fp[0]->cnext;

	for(fp = &fb->client->frame; *fp; fp = &fp[0]->cnext)
		if(*fp == fb) break;
	fp[0] = fp[0]->cnext;

	c = fa->client;
	fa->client = fb->client;
	fb->client = c;
	fb->cnext = c->frame;
	c->frame = fb;

	c = fa->client;
	fa->cnext = c->frame;
	c->frame = fa;

	if(c->sel)
		view_update(c->sel->view);
}

void
move_focus(Frame *old_f, Frame *f) {
	int noinput;

	noinput = (old_f && old_f->client->noinput) ||
		  (f && f->client->noinput) ||
		  disp.hasgrab != &c_root;
	if(noinput) {
		if(old_f)
			frame_draw(old_f);
		if(f)
			frame_draw(f);
	}
}

void
frame_focus(Frame *f) {
	Frame *old_f, *ff;
	View *v;
	Area *a, *old_a;

	v = f->view;
	a = f->area;
	old_a = v->sel;

	if(0 && f->collapsed) {
		for(ff=f; ff->collapsed && ff->anext; ff=ff->anext)
			;
		for(; ff->collapsed && ff->aprev; ff=ff->aprev)
			;
		/* XXX */
		f->colr.max.y = f->colr.min.y + Dy(ff->colr);
		ff->colr.max.y = ff->colr.min.y + labelh(def.font);
	}else if(f->area->mode == Coldefault) {
		for(; f->collapsed && f->anext; f=f->anext)
			;
		for(; f->collapsed && f->aprev; f=f->aprev)
			;
	}

	old_f = old_a->sel;
	a->sel = f;

	if(a != old_a)
		area_focus(f->area);
	if(old_a != v->oldsel && f != old_f)
		v->oldsel = nil;

	if(v != selview || a != v->sel)
		return;

	move_focus(old_f, f);
	if(a->floating)
		float_arrange(a);
	client_focus(f->client);

	/*
	if(!a->floating && ((a->mode == Colstack) || (a->mode == Colmax)))
	*/
		column_arrange(a, false);
}

int
frame_delta_h(void) {
	return def.border + labelh(def.font);
}

Rectangle
constrain(Rectangle r, int inset) {
	WMScreen **sp;
	WMScreen *s, *sbest;
	Rectangle isect;
	Point p;
	int best, n;

	if(inset < 0)
		inset = Dy(screen->brect);
	/* 
	 * FIXME: This will cause problems for windows with
	 * D(r) < 2 * inset
	 */

	SET(best);
	sbest = nil;
	for(sp=screens; (s = *sp); sp++) {
		if (!screen->showing)
			continue;
		isect = rect_intersection(r, insetrect(s->r, inset));
		if(Dx(isect) >= 0 && Dy(isect) >= 0)
			return r;
		if(Dx(isect) <= 0 && Dy(isect) <= 0)
			n = max(Dx(isect), Dy(isect));
		else
			n = min(Dx(isect), Dy(isect));
		if(!sbest || n > best) {
			sbest = s;
			best = n;
		}
	}

	isect = insetrect(sbest->r, inset);
	p = ZP;
	p.x -= min(r.max.x - isect.min.x, 0);
	p.x -= max(r.min.x - isect.max.x, 0);
	p.y -= min(r.max.y - isect.min.y, 0);
	p.y -= max(r.min.y - isect.max.y, 0);
	return rectaddpt(r, p);
}

