/* Copyright ©2006-2009 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "dat.h"
#include <limits.h>
#include "fns.h"

static void float_placeframe(Frame*);

void
float_attach(Area *a, Frame *f) {

	f->client->floating = true;

	f->r = f->floatr;
	float_placeframe(f);
	assert(a->sel != f);
	frame_insert(f, a->sel);

	if(a->sel == nil)
		area_setsel(a, f);
}

void
float_detach(Frame *f) {
	Frame *pr;
	Area *a, *sel, *oldsel;
	View *v;

	v = f->view;
	a = f->area;
	sel = view_findarea(v, v->selscreen, v->selcol, false);
	oldsel = v->oldsel;
	pr = f->aprev;

	frame_remove(f);

	if(a->sel == f) {
		if(!pr)
			pr = a->frame;
		a->sel = nil;
		area_setsel(a, pr);
	}
	f->area = nil;

	if(oldsel)
		area_focus(oldsel);
	else if(!a->frame)
		if(sel && sel->frame)
			area_focus(sel);
}

void
float_resizeframe(Frame *f, Rectangle r) {

	if(f->area->view == selview)
		client_resize(f->client, r);
	else
		frame_resize(f, r);
}

void
float_arrange(Area *a) {
	Frame *f;

	assert(a->floating);

	switch(a->mode) {
	case Coldefault:
		for(f=a->frame; f; f=f->anext)
			f->collapsed = false;
		break;
	case Colstack:
		for(f=a->frame; f; f=f->anext)
			f->collapsed = (f != a->sel);
		break;
	default:
		die("not reached");
		break;
	}
	for(f=a->frame; f; f=f->anext)
		f->r = f->floatr;
	view_update(a->view);
}

static void
rect_push(Vector_rect *vec, Rectangle r) {
	Rectangle *rp;
	int i;

	for(i=0; i < vec->n; i++) {
		rp = &vec->ary[i];
		if(rect_contains_p(*rp, r))
			return;
		if(rect_contains_p(r, *rp)) {
			*rp = r;
			return;
		}
	}
	vector_rpush(vec, r);
}

Vector_rect*
unique_rects(Vector_rect *vec, Rectangle orig) {
	static Vector_rect vec1, vec2;
	Vector_rect *v1, *v2, *v;
	Rectangle r1, r2;
	int i, j;

	v1 = &vec1;
	v2 = &vec2;
	v1->n = 0;
	vector_rpush(v1, orig);
	for(i=0; i < vec->n; i++) {
		v2->n = 0;
		r1 = vec->ary[i];
		for(j=0; j < v1->n; j++) {
			r2 = v1->ary[j];
			if(!rect_intersect_p(r1, r2)) {
				rect_push(v2, r2);
				continue;
			}
			if(r2.min.x < r1.min.x)
				rect_push(v2, Rect(r2.min.x, r2.min.y, r1.min.x, r2.max.y));
			if(r2.min.y < r1.min.y)
				rect_push(v2, Rect(r2.min.x, r2.min.y, r2.max.x, r1.min.y));
			if(r2.max.x > r1.max.x)
				rect_push(v2, Rect(r1.max.x, r2.min.y, r2.max.x, r2.max.y));
			if(r2.max.y > r1.max.y)
				rect_push(v2, Rect(r2.min.x, r1.max.y, r2.max.x, r2.max.y));
		}
		v = v1;
		v1 = v2;
		v2 = v;
	}
	return v1;
}

Rectangle
max_rect(Vector_rect *vec) {
	Rectangle *r, *rp;
	int i, a, area;

	area = 0;
	r = 0;
	for(i=0; i < vec->n; i++) {
		rp = &vec->ary[i];
		a = Dx(*rp) * Dy(*rp);
		if(a > area) {
			area = a;
			r = rp;
		}
	}
	return r ? *r : ZR;
}

static void
float_placeframe(Frame *f) {
	static Vector_rect vec;
	Vector_rect *vp;
	Rectangle r;
	Point dim, p;
	Client *c;
	Frame *ff;
	Area *a, *sel;
	long area, l;
	int i, s;

	a = f->area;
	c = f->client;

	/*
	if(c->trans)
		return;
	*/

	if(c->fullscreen >= 0 || c->w.hints->position || starting) {
		f->r = f->floatr;
		return;
	}

	/* Find all rectangles on the floating layer into which
	 * the new frame would fit.
	 */
	vec.n = 0;
	for(ff=a->frame; ff; ff=ff->anext)
		/* TODO: Find out why this check is needed.
		 * The frame hasn't been inserted yet, but somehow,
		 * its old rectangle winds up in the list.
		 */
		if(ff->client != f->client)
			vector_rpush(&vec, ff->r);

	/* Decide which screen we want to place this on.
	 * Ideally, it should probably Do the Right Thing
	 * when a screen fills, but what's the right thing?
	 * I think usage will show...
	 */
	s = -1;
	ff = client_groupframe(c, f->view);
	if (f->screen >= 0)
		s = f->screen;
	else if (ff)
		s = ownerscreen(ff->r);
	else if (selclient())
		s = ownerscreen(selclient()->sel->r);
	else {
		sel = view_findarea(a->view, a->view->selscreen, a->view->selcol, false);
		if (sel)
			s = sel->screen;
	}

	r = s == -1 ? a->r : screens[s]->r;
	vp = unique_rects(&vec, r);

	area = LONG_MAX;
	dim.x = Dx(f->r);
	dim.y = Dy(f->r);
	p = ZP;

	for(i=0; i < vp->n; i++) {
		r = vp->ary[i];
		if(Dx(r) < dim.x || Dy(r) < dim.y)
			continue;
		l = Dx(r) * Dy(r);
		if(l < area) {
			area = l;
			p = r.min;
		}
	}

	if(area == LONG_MAX) {
		/* Cascade. */
		s = max(s, 0);
		ff = a->sel;
		if(ff)
			p = addpt(ff->r.min, Pt(Dy(ff->titlebar), Dy(ff->titlebar)));
		if(p.x + Dx(f->r) > screens[s]->r.max.x ||
		   p.y + Dy(f->r) > screens[s]->r.max.y)
			p = screens[s]->r.min;
	}

	f->floatr = rectsetorigin(f->r, p);
}

