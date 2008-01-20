/* Copyright ©2006-2008 Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */
#include "dat.h"
#include <sys/limits.h>
#include "fns.h"

static void float_placeframe(Frame*);

void
float_attach(Area *a, Frame *f) {

	f->client->floating = true;

	float_placeframe(f);
	frame_insert(f, a->sel);

	if(a->sel == nil)
		area_setsel(a, f);
}

void
float_detach(Frame *f) {
	Area *a, *sel;
	View *v;

	v = f->view;
	a = f->area;
	sel = view_findarea(v, v->selcol, false);

	frame_remove(f);

	if(v->oldsel)
		area_focus(v->oldsel);
	else if(!a->frame)
		if(sel->frame)
			area_focus(sel);
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

static void
float_placeframe(Frame *f) {
	static Vector_rect rvec, rvec2;
	Vector_rect *vp, *vp2, *vptemp;
	Rectangle *rp;
	Rectangle r, fr;
	Point dim, p;
	Client *c;
	Frame *ff;
	Area *a;
	long area, l;
	int i;

	a = f->area;
	c = f->client;

	if(c->trans)
		return;
	if(c->fullscreen || c->w.hints->position || starting) {
		f->r = client_grav(c, c->r);
		return;
	}

	dim.x = Dx(f->r);
	dim.y = Dy(f->r);

	rvec.n = 0;
	rvec2.n = 0;
	vp = &rvec;
	vp2 = &rvec2;

	/* Find all rectangles on the floating layer into which
	 * the new frame would fit. (Please ignore the man behind
	 * the curtain).
	 */
	vector_rpush(vp, a->r);
	for(ff=a->frame; ff; ff=ff->anext) {
		fr = ff->r;
		vp2->n = 0;
		for(i=0; i < vp->n; i++) {
			r = vp->ary[i];
			if(!rect_intersect_p(fr, r)) {
				rect_push(vp2, r);
				continue;
			}
			if(r.min.x < fr.min.x && fr.min.x - r.min.x >= dim.x)
				rect_push(vp2, Rect(r.min.x, r.min.y, fr.min.x, r.max.y));
			if(r.max.x > fr.max.x && r.max.x - fr.max.x >= dim.x)
				rect_push(vp2, Rect(fr.max.x, r.min.y, r.max.x, r.max.y));
			if(r.min.y < fr.min.y && fr.min.y - r.min.y >= dim.y)
				rect_push(vp2, Rect(r.min.x, r.min.y, r.max.x, fr.min.y));
			if(r.max.y > fr.max.y && r.max.y - fr.max.y >= dim.y)
				rect_push(vp2, Rect(r.min.x, fr.max.y, r.max.x, r.max.y));
		}
		vptemp = vp;
		vp = vp2;
		vp2 = vptemp;
	}

	if(vp->n == 0) {
		p.x = random() % max(0, Dx(a->r) - dim.x);
		p.y = random() % max(0, Dy(a->r) - dim.y);
	}else {
		area = LONG_MAX;
		for(i=0; i < vp->n; i++) {
			rp = &vp->ary[i];
			l = Dx(*rp) * Dy(*rp);
			if(l < area) {
				area = l;
				p = rp->min;
			}
		}
	}

	fr = rectsubpt(f->r, f->r.min);
	f->r = rectaddpt(fr, p);
}

