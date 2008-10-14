/* Copyright ©2004-2006 Anselm R. Garbe <garbeam at gmail dot com>
 * Copyright ©2006-2008 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "dat.h"
#include <math.h>
#include <strings.h>
#include "fns.h"

char *modes[] = {
	[Coldefault] =	"default",
	[Colstack] =	"stack",
	[Colmax] =	"max",
};

bool
column_setmode(Area *a, const char *mode) {
	char *s, *t, *orig;
	char add, old;

	/* The mapping between the current internal
	 * representation and the external interface
	 * is currently a bit complex. That will probably
	 * change.
	 */

	orig = strdup(mode);
	t = orig;
	old = '\0';
	for(s=t; *s; s=t) {
		add = old;
		while((old=*s) && !strchr("+-^", old))
			s++;
		*s = '\0';
		if(s > t) {
			if(!strcmp(t, "max")) {
				if(add == '\0' || add == '+')
					a->max = true;
				else if(add == '-')
					a->max = false;
				else
					a->max = !a->max;
			}else
			if(!strcmp(t, "stack")) {
				if(add == '\0' || add == '+')
					a->mode = Colstack;
				else if(add == '-')
					a->mode = Coldefault;
				else
					a->mode = a->mode == Colstack ? Coldefault : Colstack;
			}else
			if(!strcmp(t, "default")) {
				if(add == '\0' || add == '+') {
					a->mode = Coldefault;
					column_arrange(a, true);
				}else if(add == '-')
					a->mode = Colstack;
				else
					a->mode = a->mode == Coldefault ? Colstack : Coldefault;
			}else
				return false;
		}
		t = s;
		if(old)
			t++;
		
	}
	free(orig);
	return true;
}

char*
column_getmode(Area *a) {

	return sxprint("%s%cmax", a->mode == Colstack ? "stack" : "default",
				  a->max ? '+' : '-');
}

Area*
column_new(View *v, Area *pos, int scrn, uint w) {
	Area *a;

	a = area_create(v, pos, scrn, w);
	return a;
#if 0
	if(!a)
		return nil;

	view_arrange(v);
	view_update(v);
#endif
}

void
column_insert(Area *a, Frame *f, Frame *pos) {

	f->area = a;
	f->client->floating = false;
	f->column = area_idx(a);
	frame_insert(f, pos);
	if(a->sel == nil)
		area_setsel(a, f);
}

/* Temporary. */
static void
stack_scale(Frame *first, int height) {
	Frame *f;
	Area *a;
	uint dy;
	int surplus;

	a = first->area;

	/*
	 * Will need something like this.
	column_fit(a, &ncol, &nuncol);
	*/

	dy = 0;
	for(f=first; f && !f->collapsed; f=f->anext)
		dy += Dy(f->colr);

	/* Distribute the surplus.
	 */
	surplus = height - dy;
	for(f=first; f && !f->collapsed; f=f->anext)
		f->colr.max.y += ((float)Dy(f->r) / dy) * surplus;
}

static void
stack_info(Frame *f, Frame **firstp, int *dyp, int *nframep) {
	Frame *ft, *first;
	int dy, nframe;

	nframe = 0;
	dy = 0;
	first = f;
	for(ft=f; ft && ft->collapsed; ft=ft->anext)
		;
	if(ft && ft != f) {
		f = ft;
		dy += Dy(f->colr);
	}
	for(ft=f; ft && !ft->collapsed; ft=ft->aprev) {
		first = ft;
		nframe++;
		dy += Dy(ft->colr);
	}
	for(ft=f->anext; ft && !ft->collapsed; ft=ft->anext) {
		if(first == nil)
			first = ft;
		nframe++;
		dy += Dy(ft->colr);
	}
	if(nframep) *nframep = nframe;
	if(firstp) *firstp = first;
	if(dyp) *dyp = dy;
}

int
stack_count(Frame *f, int *mp) {
	Frame *fp;
	int n, m;

	n = 0;
	for(fp=f->aprev; fp && fp->collapsed; fp=fp->aprev)
		n++;
	m = ++n;
	for(fp=f->anext; fp && fp->collapsed; fp=fp->anext)
		n++;
	if(mp) *mp = m;
	return n;
}

void
column_attach(Area *a, Frame *f) {
	Frame *first;
	int nframe, dy, h;

	f->colr = a->r;

	if(a->sel) {
		stack_info(a->sel, &first, &dy, &nframe);
		h = dy / (nframe+1);
		f->colr.max.y = f->colr.min.y + h;
		stack_scale(first, dy - h);
	}

	column_insert(a, f, a->sel);
	column_arrange(a, false);
}

void
column_detach(Frame *f) {
	Frame *first;
	Area *a;
	int dy;

	a = f->area;
	stack_info(f, &first, &dy, nil);
	column_remove(f);
	if(a->frame) {
		if(first)
			stack_scale(first, dy);
		column_arrange(a, false);
	}else if(a->view->areas[a->screen]->next)
		area_destroy(a);
}

static void column_scale(Area*);

void
column_attachrect(Area *a, Frame *f, Rectangle r) {
	Frame *fp, *pos;
	int before, after;

	pos = nil;
	for(fp=a->frame; fp; pos=fp, fp=fp->anext) {
		if(r.max.y < fp->r.min.y)
			continue;
		if(r.min.x > fp->r.max.y)
			continue;
		before = fp->r.min.y - r.min.y;
		after = r.max.y - fp->r.max.y;
		if(abs(before) <= abs(after))
			break;
	}
	if(Dy(a->r) > Dy(r)) {
		/* Kludge. */
		a->r.max.y -= Dy(r);
		column_scale(a);
		a->r.max.y += Dy(r);
	}
	column_insert(a, f, pos);
	for(fp=f->anext; fp; fp=fp->anext) {
		fp->r.min.y += Dy(r);
		fp->r.max.y += Dy(r);
	}
	column_resizeframe(f, r);
}

void
column_remove(Frame *f) {
	Frame *pr;
	Area *a;

	a = f->area;
	pr = f->aprev;

	frame_remove(f);

	f->area = nil;
	if(a->sel == f) {
		if(pr == nil)
			pr = a->frame;
		if(pr && pr->collapsed)
			if(pr->anext && !pr->anext->collapsed)
				pr = pr->anext;
			else
				pr->collapsed = false;
		a->sel = nil;
		area_setsel(a, pr);
	}
}

static int
column_surplus(Area *a) {
	Frame *f;
	int surplus;

	surplus = Dy(a->r);
	for(f=a->frame; f; f=f->anext)
		surplus -= Dy(f->r);
	return surplus;
}

static void
column_fit(Area *a, uint *ncolp, uint *nuncolp) {
	Frame *f, **fp;
	uint minh, dy;
	uint ncol, nuncol;
	uint colh, uncolh;
	int surplus, i, j;

	/* The minimum heights of collapsed and uncollpsed frames.
	 */
	minh = labelh(def.font);
	colh = labelh(def.font);
	uncolh = minh + colh + 1;
	if(a->max && !resizing)
		colh = 0;

	/* Count collapsed and uncollapsed frames. */
	ncol = 0;
	nuncol = 0;
	for(f=a->frame; f; f=f->anext) {
		frame_resize(f, f->colr);
		if(f->collapsed)
			ncol++;
		else
			nuncol++;
	}

	if(nuncol == 0) {
		nuncol++;
		ncol--;
		if(a->sel)
			a->sel->collapsed = false;
		else
			a->frame->collapsed = false;
	}

	/* FIXME: Kludge. */
	dy = Dy(a->view->r) - Dy(a->r);
	minh = colh * (ncol + nuncol - 1) + uncolh;
	if(dy && Dy(a->r) < minh)
		a->r.max.y += min(dy, minh - Dy(a->r));

	surplus = Dy(a->r)
		- (ncol * colh)
		- (nuncol * uncolh);

	/* Collapse until there is room */
	if(surplus < 0) {
		i = ceil(-1.F * surplus / (uncolh - colh));
		if(i >= nuncol)
			i = nuncol - 1;
		nuncol -= i;
		ncol += i;
		surplus += i * (uncolh - colh);
	}
	/* Push to the floating layer until there is room */
	if(surplus < 0) {
		i = ceil(-1.F * surplus / colh);
		if(i > ncol)
			i = ncol;
		ncol -= i;
		surplus += i * colh;
	}

	/* Decide which to collapse and which to float. */
	j = nuncol - 1;
	i = ncol - 1;
	for(fp=&a->frame; *fp;) {
		f = *fp;
		if(f != a->sel) {
			if(!f->collapsed) {
				if(j < 0)
					f->collapsed = true;
				j--;
			}
			if(f->collapsed) {
				if(i < 0) {
					f->collapsed = false;
					area_moveto(f->view->floating, f);
					continue;
				}
				i--;
			}
		}
		/* Doesn't change if we 'continue' */
		fp = &f->anext;
	}

	if(ncolp) *ncolp = ncol;
	if(nuncolp) *nuncolp = nuncol;
}

void
column_settle(Area *a) {
	Frame *f;
	uint yoff, yoffcr;
	int surplus, nuncol, n;

	nuncol = 0;
	surplus = column_surplus(a);
	for(f=a->frame; f; f=f->anext)
		if(!f->collapsed) nuncol++;

	if(nuncol == 0) {
		fprint(2, "%s: Badness: No uncollapsed frames, column %d, view %q\n",
				argv0, area_idx(a), a->view->name);
		return;
	}
	if(surplus < 0)
		fprint(2, "%s: Badness: surplus = %d in column_settle, column %d, view %q\n",
				argv0, surplus, area_idx(a), a->view->name);

	yoff = a->r.min.y;
	yoffcr = yoff;
	n = surplus % nuncol;
	surplus /= nuncol;
	for(f=a->frame; f; f=f->anext) {
		f->r = rectsetorigin(f->r, Pt(a->r.min.x, yoff));
		f->colr = rectsetorigin(f->colr, Pt(a->r.min.x, yoffcr));
		f->r.min.x = a->r.min.x;
		f->r.max.x = a->r.max.x;
		if(def.incmode == ISqueeze && !resizing)
		if(!f->collapsed) {
			f->r.max.y += surplus;
			if(n-- > 0)
				f->r.max.y++;
		}
		yoff = f->r.max.y;
		yoffcr = f->colr.max.y;
	}
}

static int
foo(Frame *f) {
	WinHints h;
	int maxh;

	h = frame_gethints(f);
	maxh = 0;
	if(h.aspect.max.x)
		maxh = h.baspect.y +
		       (Dx(f->r) - h.baspect.x) *
		       h.aspect.max.y / h.aspect.max.x;
	maxh = max(maxh, h.max.y);

	if(Dy(f->r) > maxh)
		return 0;
	return h.inc.y - (Dy(f->r) - h.base.y) % h.inc.y;
}

static int
comp_frame(const void *a, const void *b) {
	Frame *fa, *fb;
	int ia, ib;

	fa = *(Frame**)a;
	fb = *(Frame**)b;
	ia = foo(fa);
	ib = foo(fb);
	return ia < ib             ? -1 :
	       ia > ib             ?  1 :
	       /* Favor the selected client. */
	       /* No... don't. Windows shouldn't jump when the mouse
		* enters them.
	       fa == fa->area->sel ? -1 :
	       fb == fa->area->sel ?  1 :
	       */
	                              0;
}

static void
column_squeeze(Area *a) {
	static Vector_ptr fvec; 
	Frame *f;
	int surplus, osurplus, dy, i;

	fvec.n = 0;
	for(f=a->frame; f; f=f->anext)
		if(!f->collapsed) {
			f->r = frame_hints(f, f->r, 0);
			vector_ppush(&fvec, f);
		}

	surplus = column_surplus(a);
	for(osurplus=0; surplus != osurplus;) {
		osurplus = surplus;
		qsort(fvec.ary, fvec.n, sizeof *fvec.ary, comp_frame);
		for(i=0; i < fvec.n; i++) {
			f=fvec.ary[i];
			dy = foo(f);
			if(dy > surplus)
				break;
			surplus -= dy;
			f->r.max.y += dy;
		}
	}
}

void
column_frob(Area *a) {
	Frame *f;

	for(f=a->frame; f; f=f->anext)
		f->r = f->colr;
	column_settle(a);
	if(a->view == screen->sel)
	for(f=a->frame; f; f=f->anext)
		client_resize(f->client, f->r);
}

static void
column_scale(Area *a) {
	Frame *f;
	uint dy;
	uint ncol, nuncol;
	uint colh;
	int surplus;

	if(!a->frame)
		return;

	column_fit(a, &ncol, &nuncol);

	colh = labelh(def.font);
	if(a->max && !resizing)
		colh = 0;

	dy = 0;
	surplus = Dy(a->r);
	for(f=a->frame; f; f=f->anext) {
		if(f->collapsed)
			f->colr.max.y = f->colr.min.y + colh;
		else if(Dy(f->colr) == 0)
			f->colr.max.y++;
		surplus -= Dy(f->colr);
		if(!f->collapsed)
			dy += Dy(f->colr);
	}
	for(f=a->frame; f; f=f->anext) {
		f->dy = Dy(f->r);
		f->colr.min.x = a->r.min.x;
		f->colr.max.x = a->r.max.x;
		if(!f->collapsed)
			f->colr.max.y += ((float)f->dy / dy) * surplus;
		if(btassert("6 full", !(f->collapsed ? Dy(f->r) >= 0 : dy > 0)))
			warning("Something's fucked: %s:%d:%s()",
				__FILE__, __LINE__, __func__);
		frame_resize(f, f->colr);
	}

	if(def.incmode == ISqueeze && !resizing)
		column_squeeze(a);
	column_settle(a);
}

void
column_arrange(Area *a, bool dirty) {
	Frame *f;
	View *v;

	if(a->floating)
		float_arrange(a);
	if(a->floating || !a->frame)
		return;

	v = a->view;

	switch(a->mode) {
	case Coldefault:
		if(dirty)
			for(f=a->frame; f; f=f->anext)
				f->colr = Rect(0, 0, 100, 100);
		break;
	case Colstack:
		/* XXX */
		for(f=a->frame; f; f=f->anext)
			f->collapsed = (f != a->sel);
		break;
	default:
		print("Dieing: %s: screen: %d a: %p mode: %x floating: %d\n", v->name, a->screen, a, a->mode, a->floating);
		die("not reached");
		break;
	}
	column_scale(a);
	/* XXX */
	if(a->sel->collapsed)
		area_setsel(a, a->sel);
	if(v == screen->sel) {
		//view_restack(v);
		client_resize(a->sel->client, a->sel->r);
		for(f=a->frame; f; f=f->anext)
			client_resize(f->client, f->r);
	}
}

void
column_resize(Area *a, int w) {
	Area *an;
	int dw;

	an = a->next;
	assert(an != nil);

	dw = w - Dx(a->r);
	a->r.max.x += dw;
	an->r.min.x += dw;

	/* view_arrange(a->view); */
	view_update(a->view);
}

static void
column_resizeframe_h(Frame *f, Rectangle r) {
	Area *a;
	Frame *fn, *fp;
	uint minh;

	minh = labelh(def.font);

	a = f->area;
	fn = f->anext;
	fp = f->aprev;

	if(fp)
		r.min.y = max(r.min.y, fp->colr.min.y + minh);
	else /* XXX. */
		r.min.y = max(r.min.y, a->r.min.y);

	if(fn)
		r.max.y = min(r.max.y, fn->colr.max.y - minh);
	else
		r.max.y = min(r.max.y, a->r.max.y);

	if(fp) {
		fp->colr.max.y = r.min.y;
		frame_resize(fp, fp->colr);
	}
	if(fn) {
		fn->colr.min.y = r.max.y;
		frame_resize(fn, fn->colr);
	}

	f->colr = r;
	frame_resize(f, r);
}

void
column_resizeframe(Frame *f, Rectangle r) {
	Area *a, *al, *ar;
	View *v;
	uint minw;

	a = f->area;
	v = a->view;

	minw = Dx(v->r) / NCOL;

	ar = a->next;
	al = a->prev;
	if(al == v->floating)
		al = nil;

	if(al)
		r.min.x = max(r.min.x, al->r.min.x + minw);
	else { /* Hm... */
		r.min.x = max(r.min.x, v->r.min.x);
		r.max.x = max(r.max.x, r.min.x + minw);
	}

	if(ar)
		r.max.x = min(r.max.x, ar->r.max.x - minw);
	else {
		r.max.x = min(r.max.x, v->r.max.x);
		r.min.x = min(r.min.x, r.max.x - minw);
	}

	a->r.min.x = r.min.x;
	a->r.max.x = r.max.x;
	if(al) {
		al->r.max.x = a->r.min.x;
		column_arrange(al, false);
	}
	if(ar) {
		ar->r.min.x = a->r.max.x;
		column_arrange(ar, false);
	}

	column_resizeframe_h(f, r);

	/* view_arrange(v); */
	view_update(v);
}

