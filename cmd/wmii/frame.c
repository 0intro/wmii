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
		f->revert = c->sel->revert;
		f->r = c->sel->r;
	}
	else{
		f->r = frame_client2rect(f, client_grav(c, ZR));
		f->revert = f->r;
		c->sel = f;
	}
	f->collapsed = False;

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
frame_insert(Frame *pos, Frame *f) {
	Area *a;

	a = f->area;

	if(pos) {
		f->aprev = pos;
		f->anext = pos->anext;
	}else {
		f->anext = f->area->frame;
		f->area->frame = f;
	}
	if(f->aprev)
		f->aprev->anext = f;
	if(f->anext)
		f->anext->aprev = f;

	if(a->floating) {
		f->snext = a->stack;
		a->stack = f;
		if(f->snext)
			f->snext->sprev = f;
	}
}

bool
frame_restack(Frame *f, Frame *above) {
	Area *a;

	a = f->area;
	if(!a->floating)
		return false;
	if(above && above->area != a)
		return false;

	if(f->sprev)
		f->sprev->snext = f->snext;
	else
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

	return true;
}

/* Handlers */
static void
bup_event(Window *w, XButtonEvent *e) {
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
			mouse_resize(c, False, CENTER);
			focus(c, false);
			frame_restack(f, nil);
			focus(c, false); /* Blech */
			break;
		case Button3:
			mouse_resize(c, False, quadrant(f->r, Pt(e->x_root, e->y_root)));
			frame_restack(f, nil);
			focus(c, false);
			break;
		default:
			XAllowEvents(display, ReplayPointer, e->time);
			break;
		}
		if(e->button != Button1)
			XUngrabPointer(display, e->time);
	}else{
		if(e->button == Button1) {
			if(frame_restack(f, nil))
				view_restack(f->view);
			else if(rect_haspoint_p(Pt(e->x, e->y), f->grabbox))
				mouse_resize(c, True, CENTER);
			else if(f->area->floating)
				if(!e->subwindow && !rect_haspoint_p(Pt(e->x, e->y), f->titlebar))
					mouse_resize(c, False, quadrant(f->r, Pt(e->x_root, e->y_root)));

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

Rectangle
frame_rect2client(Frame *f, Rectangle r) {
	if(f == nil || f->area == nil || f->area->floating) {
		r.max.x -= def.border * 2;
		r.max.y -= frame_delta_h();
		if(f) {
			if(f->client->borderless) {
				r.max.x += 2 * def.border;
				r.max.y += def.border;
			}
			if(f->client->titleless)
				r.max.y += labelh(def.font);
		}
	}else {
		r.max.x -= 2;
		r.max.y -= labelh(def.font) + 1;
	}
	r.max.x = max(r.min.x+1, r.max.x);
	r.max.y = max(r.min.y+1, r.max.y);
	return r;
}

Rectangle
frame_client2rect(Frame *f, Rectangle r) {
	if(f == nil || f->area == nil ||  f->area->floating) {
		r.max.x += def.border * 2;
		r.max.y += frame_delta_h();
		if(f) {
			if(f->client->borderless) {
				r.max.x -= 2 * def.border;
				r.max.y -= def.border;
			}
			if(f->client->titleless)
				r.max.y -= labelh(def.font);
		}
	}else {
		r.max.x += 2;
		r.max.y += labelh(def.font) + 1;
	}
	return r;
}

void
frame_resize(Frame *f, Rectangle r) {
	Align stickycorner;
	Point pt;
	Client *c;
	int collapsed;

	c = f->client;
	stickycorner = get_sticky(f->r, r);

	f->crect = frame_hints(f, r, stickycorner);

	if(Dx(r) <= 0 || Dy(r) <= 0)
		fprint(2, "Badness: Frame rect: %R\n", r);

	if(f->area->floating)
		f->r = f->crect;
	else
		f->r = r;

	f->crect = frame_rect2client(f, f->crect);
	f->crect = rectsubpt(f->crect, f->crect.min);

	collapsed = f->collapsed;

	if(!f->area->floating && f->area->mode == Coldefault) {
		if(Dy(f->r) < 2 * labelh(def.font))
			f->collapsed = True;
		else
			f->collapsed = False;
	}

	if(Dx(f->crect) < labelh(def.font)) {
		f->r.max.x = f->r.min.x + frame_delta_h();
		f->collapsed = True;
	}

	if(f->collapsed) {
		f->r.max.y= f->r.min.y + labelh(def.font);
		f->crect = f->r;
	}

	if(collapsed != f->collapsed)
		ewmh_updatestate(c);

	pt = ZP;
	if(!f->client->borderless || !f->area->floating)
		pt.y += 1;
	if(!f->client->titleless || !f->area->floating)
		pt.y += labelh(def.font) - 1;

	if(f->area->floating) {
		if(c->fullscreen) {
			f->crect = screen->r;
			f->r = frame_client2rect(f, f->crect);
			pt.x = (Dx(f->r) - Dx(f->crect)) / 2;
			f->r = rectsubpt(f->r, pt);
		}else
			f->r = constrain(f->r);
	}
	pt.x = (Dx(f->r) - Dx(f->crect)) / 2;
	f->crect = rectaddpt(f->crect, pt);
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
	Client *c;
	Frame *old_f;
	View *v;
	Area *a, *old_a;

	c = f->client;
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
		column_arrange(a, False);
}

int
frame_delta_h(void) {
	return def.border + labelh(def.font);
}

void
frame_draw(Frame *f) {
	Rectangle r, fr;
	CTuple *col;
	Frame *tf;
	uint w;

	if(f->view != screen->sel)
		return;

	if(f->client == screen->focus
	|| f->client == selclient())
		col = &def.focuscolor;
	else
		col = &def.normcolor;
	if(!f->area->floating && f->area->mode == Colmax)
		for(tf = f->area->frame; tf; tf=tf->anext)
			if(tf->client == screen->focus) {
				col = &def.focuscolor;
				break;
			}
	fr = f->client->framewin->r;
	fr = rectsubpt(fr, fr.min);

	/* background */
	r = fr;
	fill(screen->ibuf, r, col->bg);
	border(screen->ibuf, r, 1, col->border);

	r.max.y = r.min.y + labelh(def.font);
	border(screen->ibuf, r, 1, col->border);

	f->titlebar = insetrect(r, 3);
	f->titlebar.max.y += 3;

	/* Odd state of focus. */
	if(f->client != selclient() && col == &def.focuscolor)
		border(screen->ibuf, insetrect(r, 1),
			1, def.normcolor.bg);

	/* grabbox */
	r.min = Pt(2, 2);
	r.max.x = r.min.x + def.font->height - 3;
	r.max.y -= 2;
	f->grabbox = r;

	if(f->client->urgent)
		fill(screen->ibuf, r, col->fg);
	border(screen->ibuf, r, 1, col->border);

	/* Odd state of focus. */
	if(f->client != screen->focus && col == &def.focuscolor)
		border(screen->ibuf, insetrect(r, -1),
			1, def.normcolor.bg);

	/* Label */
	r.min.x = r.max.x;
	r.max.x = fr.max.x;
	r.min.y = 0;
	r.max.y = labelh(def.font);
	if(f->client->floating)
		r.max.x -= Dx(f->grabbox);
	w = drawstring(screen->ibuf, def.font, r, WEST,
			f->client->name, col->fg);

	if(f->client->floating) {
		r.min.x = r.min.x + w + 10;
		r.max.x = f->titlebar.max.x + 1;
		r.min.y = f->grabbox.min.y;
		r.max.y = f->grabbox.max.y;
		border(screen->ibuf, r, 1, col->border);
	}

	/* Why? Because some non-ICCCM-compliant apps feel the need to
	 * change the background properties of all of their ancestor windows
	 * in order to implement pseudo-transparency.
	 * What's more, the designers of X11 felt that it would be unfair to
	 * implementers to make it possible to detect, or forbid, such changes.
	 */
	XSetWindowBackgroundPixmap(display, f->client->framewin->w, None);

	copyimage(f->client->framewin, fr, screen->ibuf, ZP);
	sync();
}

void
frame_draw_all(void) {
	Client *c;

	for(c=client; c; c=c->next)
		if(c->sel && c->sel->view == screen->sel)
			frame_draw(c->sel);
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
