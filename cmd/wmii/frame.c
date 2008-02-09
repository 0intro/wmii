/* Copyright ©2006-2008 Kris Maglione <fbsdaemon@gmail.com>
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
	f->oldarea = -1;

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
			mouse_resize(c, Center);
			break;
		case Button2:
			frame_restack(f, nil);
			view_restack(f->view);
			focus(c, false);
			grabpointer(c->framewin, nil, cursor[CurNone], ButtonReleaseMask);
			break;
		case Button3:
			focus(c, false);
			mouse_resize(c, quadrant(f->r, Pt(e->x_root, e->y_root)));
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
			}
			if(rect_haspoint_p(Pt(e->x, e->y), f->grabbox))
				mouse_movegrabbox(c);
			else if(f->area->floating)
				if(!e->subwindow && !rect_haspoint_p(Pt(e->x, e->y), f->titlebar))
					mouse_resize(c, quadrant(f->r, Pt(e->x_root, e->y_root)));

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
enter_event(Window *w, XCrossingEvent *e) {
	Client *c;
	Frame *f;

	c = w->aux;
	f = c->sel;
	if(screen->focus != c || selclient() != c) {
		Dprint(DGeneric, "enter_notify(f) => %s\n", f->client->name);
		if(f->area->floating || !f->collapsed)
			focus(f->client, false);
	}
	frame_setcursor(f, Pt(e->x, e->y));
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
	frame_setcursor(c->sel, Pt(e->x, e->y));
}

Handlers framehandler = {
	.bup = bup_event,
	.bdown = bdown_event,
	.enter = enter_event,
	.expose = expose_event,
	.motion = motion_event,
};

/* These must die!!! */
Rectangle
frame_rect2client(Client *c, Rectangle r, bool floating) {

	if(c->fullscreen)
		return r;

	if(!floating) {
		r.min.x += 1;
		r.min.y += labelh(def.font);
		r.max.x -= 1;
		r.max.y -= 1;
	}else {
		if(!c->borderless) {
			r.min.x += def.border;
			r.max.x -= def.border;
			r.max.y -= def.border;
		}
		if(!c->titleless)
			r.min.y += labelh(def.font);
	}
	r.max.x = max(r.max.x, r.min.x+1);
	r.max.y = max(r.max.y, r.min.y+1);
	return r;
}

Rectangle
frame_client2rect(Client *c, Rectangle r, bool floating) {

	if(c->fullscreen)
		return r;

	if(!floating) {
		r.min.x -= 1;
		r.min.y -= labelh(def.font);
		r.max.x += 1;
		r.max.y += 1;
	}else {
		if(!c->borderless) {
			r.min.x -= def.border;
			r.max.x += def.border;
			r.max.y += def.border;
		}
		if(!c->titleless)
			r.min.y -= labelh(def.font);
	}
	return r;
}

void
frame_resize(Frame *f, Rectangle r) {
	Client *c;
	Rectangle fr, cr;
	int collapsed, dx;

	if(Dx(r) <= 0 || Dy(r) <= 0)
		die("Frame rect: %R\n", r);

	c = f->client;
	if(c->fullscreen) {
		f->crect = screen->r;
		f->r = screen->r;
		return;
	}

	if(f->area->floating)
		f->collapsed = false;

	fr = frame_hints(f, r, get_sticky(f->r, r));
	if(f->area->floating && !c->strut)
		fr = constrain(fr);

	/* Collapse managed frames which are too small */
	collapsed = f->collapsed;
	if(!f->area->floating && f->area->mode == Coldefault) {
		f->collapsed = false;
		if(Dy(f->r) < 2 * labelh(def.font))
			f->collapsed = true;
	}
	if(collapsed != f->collapsed)
		ewmh_updatestate(c);

	fr.max.x = max(fr.max.x, fr.min.x + 2*labelh(def.font));
	if(f->collapsed)
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

	if(f->area->floating)
		f->floatr = f->r;
	else
		f->colr = f->r;
}

void
frame_draw(Frame *f) {
	Rectangle r, fr;
	Client *c;
	CTuple *col;
	Frame *tf;
	uint w;

	if(f->view != screen->sel)
		return;
	if(f->area == nil) /* Blech. */
		return;

	c = f->client;
	fr = rectsetorigin(c->framewin->r, ZP);

	/* Pick colors. */
	if(c == screen->focus
	|| c == selclient())
		col = &def.focuscolor;
	else
		col = &def.normcolor;
	if(!f->area->floating && f->area->mode == Colmax)
		for(tf = f->area->frame; tf; tf=tf->anext)
			if(tf->client == screen->focus) {
				col = &def.focuscolor;
				break;
			}

	/* Background/border */
	r = fr;
	fill(screen->ibuf, r, col->bg);
	border(screen->ibuf, r, 1, col->border);

	/* Title border */
	r.max.y = r.min.y + labelh(def.font);
	border(screen->ibuf, r, 1, col->border);

	f->titlebar = insetrect(r, 3);
	f->titlebar.max.y += 3;

	/* Odd focus. Ulselected, with keyboard focus. */
	/* Draw a border just inside the titlebar. */
	/* FIXME: Perhaps this should be normcolored? */
	if(c != selclient() && col == &def.focuscolor)
		border(screen->ibuf, insetrect(r, 1), 1, def.normcolor.bg);

	/* grabbox */
	r.min = Pt(2, 2);
	r.max.x = r.min.x + def.font->height - 3;
	r.max.y -= 2;
	f->grabbox = r;

	if(c->urgent)
		fill(screen->ibuf, r, col->fg);
	border(screen->ibuf, r, 1, col->border);

	/* Odd focus. Selected, without keyboard focus. */
	/* Draw a border around the grabbox. */
	if(c != screen->focus && col == &def.focuscolor)
		border(screen->ibuf, insetrect(r, -1), 1, def.normcolor.bg);

	/* Draw a border on borderless/titleless selected apps. */
	if(c->borderless && c->titleless && c == selclient())
		setborder(c->framewin, def.border, def.focuscolor.border);
	else
		setborder(c->framewin, 0, 0);

	/* Label */
	r.min.x = r.max.x;
	r.max.x = fr.max.x;
	r.min.y = 0;
	r.max.y = labelh(def.font);
	if(c->floating)
		r.max.x -= Dx(f->grabbox);
	w = drawstring(screen->ibuf, def.font, r, West,
			c->name, col->fg);

	if(f->area->floating) {
		r.min.x = r.min.x + w + 10;
		r.max.x = f->titlebar.max.x + 1;
		r.min.y = f->grabbox.min.y;
		r.max.y = f->grabbox.max.y;
		border(screen->ibuf, r, 1, col->border);
	}

	/* Border increment gaps... */
	r.min.y = f->crect.min.y;
	r.min.x = max(1, f->crect.min.x - 1);
	r.max.x = min(fr.max.x - 1, f->crect.max.x + 1);
	r.max.y = min(fr.max.y - 1, f->crect.max.y + 1);
	border(screen->ibuf, r, 1, col->border);

	/* Why? Because some non-ICCCM-compliant apps feel the need to
	 * change the background properties of all of their ancestor windows
	 * in order to implement pseudo-transparency.
	 * What's more, the designers of X11 felt that it would be unfair to
	 * implementers to make it possible to detect, or forbid, such changes.
	 */
	XSetWindowBackgroundPixmap(display, c->framewin->w, None);

	copyimage(c->framewin, fr, screen->ibuf, ZP);
	sync();
}

void
frame_draw_all(void) {
	Client *c;

	for(c=client; c; c=c->next)
		if(c->sel && c->sel->view == screen->sel)
			frame_draw(c->sel);
}

void
frame_setcursor(Frame *f, Point pt) {
	Rectangle r;
	Cursor cur;

	if(f->area->floating
	&& !rect_haspoint_p(pt, f->titlebar)
	&& !rect_haspoint_p(pt, f->crect)) {
	 	r = rectsubpt(f->r, f->r.min);
	 	cur = quad_cursor(quadrant(r, pt));
		client_setcursor(f->client, cur);
	} else
		client_setcursor(f->client, cursor[CurNormal]);
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

	if(c->sel && c->sel->view == screen->sel)
		view_focus(screen, c->sel->view);
}

void
move_focus(Frame *old_f, Frame *f) {
	int noinput;

	noinput = (old_f && old_f->client->noinput) ||
		  (f && f->client->noinput) ||
		  screen->hasgrab != &c_root;
	if(noinput) {
		if(old_f)
			frame_draw(old_f);
		if(f)
			frame_draw(f);
	}
}

void
frame_focus(Frame *f) {
	Frame *old_f;
	View *v;
	Area *a, *old_a;

	v = f->view;
	a = f->area;
	old_a = v->sel;

	old_f = old_a->sel;
	a->sel = f;

	if(a != old_a)
		area_focus(f->area);
	if(old_a != v->oldsel && f != old_f)
		v->oldsel = nil;

	if(v != screen->sel || a != v->sel)
		return;

	move_focus(old_f, f);
	client_focus(f->client);

	if(!a->floating && ((a->mode == Colstack) || (a->mode == Colmax)))
		column_arrange(a, false);
}

int
frame_delta_h(void) {
	return def.border + labelh(def.font);
}

Rectangle
constrain(Rectangle r) {
	Rectangle sr;
	Point p;

	sr = screen->r;
	sr.max.y = screen->brect.min.y;

	if(Dx(r) > Dx(sr))
		r.max.x = r.min.x + Dx(sr);
	if(Dy(r) > Dy(sr))
		r.max.y = r.min.y + Dy(sr);

	sr = insetrect(sr, Dy(screen->brect));
	p = ZP;
	p.x -= min(r.max.x - sr.min.x, 0);
	p.x -= max(r.min.x - sr.max.x, 0);
	p.y -= min(r.max.y - sr.min.y, 0);
	p.y -= max(r.min.y - sr.max.y, 0);
	return rectaddpt(r, p);
}
