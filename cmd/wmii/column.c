/* Copyright ©2004-2006 Anselm R. Garbe <garbeam at gmail dot com>
 * Copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "dat.h"
#include <math.h>
#include <strings.h>
#include "fns.h"

static void	column_resizeframe_h(Frame*, Rectangle);

char *modes[] = {
	[Coldefault] =	"default",
	[Colstack] =	"stack",
	[Colmax] =	"max",
};

bool
column_setmode(Area *a, const char *mode) {
	char *str, *tok, *orig;
	char add, old;

	/*
	 * The mapping between the current internal
	 * representation and the external interface
	 * is currently a bit complex. That will probably
	 * change.
	 */

	orig = strdup(mode);
	str = orig;
	old = '\0';
	while(*(tok = str)) {
		add = old;
		while((old=*str) && !strchr("+-^", old))
			str++;
		*str = '\0';
		if(str > tok) {
			if(!strcmp(tok, "max")) {
				if(add == '\0' || add == '+')
					a->max = true;
				else if(add == '-')
					a->max = false;
				else
					a->max = !a->max;
			}else
			if(!strcmp(tok, "stack")) {
				if(add == '\0' || add == '+')
					a->mode = Colstack;
				else if(add == '-')
					a->mode = Coldefault;
				else
					a->mode = a->mode == Colstack ? Coldefault : Colstack;
			}else
			if(!strcmp(tok, "default")) {
				if(add == '\0' || add == '+') {
					a->mode = Coldefault;
					column_arrange(a, true);
				}else if(add == '-')
					a->mode = Colstack;
				else
					a->mode = a->mode == Coldefault ? Colstack : Coldefault;
			}else {
				free(orig);
				return false;
			}
		}
		if(old)
			str++;

	}
	free(orig);
	return true;
}

char*
column_getmode(Area *a) {
	return sxprint("%s%cmax", a->mode == Colstack ? "stack" : "default",
				  a->max ? '+' : '-');
}

int
column_minwidth(void)
{
	return 4 * labelh(def.font);
}

Area*
column_new(View *v, Area *pos, int scrn, uint w) {
	Area *a;

	assert(!pos || !pos->floating && pos->screen == scrn);
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
	f->screen = a->screen;
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
stack_info(Frame *f, Frame **firstp, Frame **lastp, int *dyp, int *nframep) {
	Frame *ft, *first, *last;
	int dy, nframe;

	nframe = 0;
	dy = 0;
	first = f;
	last = f;

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
		last = ft;
		nframe++;
		dy += Dy(ft->colr);
	}
	if(nframep) *nframep = nframe;
	if(firstp) *firstp = first;
	if(lastp) *lastp = last;
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

Frame*
stack_find(Area *a, Frame *f, int dir, bool stack) {
	Frame *fp;

#define predicate(f) !((f)->collapsed && stack || (f)->client->nofocus)
	switch (dir) {
	default:
		die("not reached");
	case North:
		if(f)
			for(f=f->aprev; f && !predicate(f); f=f->aprev)
				;
		else {
			f = nil;
			for(fp=a->frame; fp; fp=fp->anext)
				if(predicate(fp))
					f = fp;
		}
		break;
	case South:
		if(f)
			for(f=f->anext; f && !predicate(f); f=f->anext)
				;
		else
			for(f=a->frame; f && !predicate(f); f=f->anext)
				;
		break;
	}
#undef predicate
	return f;
}

/* TODO: Move elsewhere. */
bool
find(Area **ap, Frame **fp, int dir, bool wrap, bool stack) {
	Rectangle r;
	Frame *f;
	Area *a;

	f = *fp;
	a = *ap;
	r = f ? f->r : a->r;

	if(dir == North || dir == South) {
		*fp = stack_find(a, f, dir, stack);
		if(*fp)
			return true;
		if(!a->floating)
			*ap = area_find(a->view, r, dir, wrap);
		if(!*ap)
			return false;
		*fp = stack_find(*ap, *fp, dir, stack);
		return true;
	}
	if(dir != East && dir != West)
		die("not reached");
	*ap = area_find(a->view, r, dir, wrap);
	if(!*ap)
		return false;
	*fp = ap[0]->sel;
	return true;
}

void
column_attach(Area *a, Frame *f) {
	Frame *first;
	int nframe, dy, h;

	f->colr = a->r;

	if(a->sel) {
		stack_info(a->sel, &first, nil, &dy, &nframe);
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
	stack_info(f, &first, nil, &dy, nil);
	if(first && first == f)
		first = f->anext;
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
		if(r.max.y < fp->r.min.y || r.min.y > fp->r.max.y)
			continue;
		before = fp->r.min.y - r.min.y;
		after = -fp->r.max.y + r.max.y;
	}
	column_insert(a, f, pos);
	column_resizeframe_h(f, r);
	column_scale(a);
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
column_fit(Area *a, uint *n_colp, uint *n_uncolp) {
	Frame *f, **fp;
	uint minh, dy;
	uint n_col, n_uncol;
	uint col_h, uncol_h;
	int surplus, i, j;

	/* The minimum heights of collapsed and uncollpsed frames.
	 */
	minh = labelh(def.font);
	col_h = labelh(def.font);
	uncol_h = minh + col_h + 1;
	if(a->max && !resizing)
		col_h = 0;

	/* Count collapsed and uncollapsed frames. */
	n_col = 0;
	n_uncol = 0;
	for(f=a->frame; f; f=f->anext) {
		frame_resize(f, f->colr);
		if(f->collapsed)
			n_col++;
		else
			n_uncol++;
	}

	if(n_uncol == 0) {
		n_uncol++;
		n_col--;
		(a->sel ? a->sel : a->frame)->collapsed = false;
	}

	/* FIXME: Kludge. See frame_attachrect. */
	dy = Dy(a->view->r[a->screen]) - Dy(a->r);
	minh = col_h * (n_col + n_uncol - 1) + uncol_h;
	if(dy && Dy(a->r) < minh)
		a->r.max.y += min(dy, minh - Dy(a->r));

	surplus = Dy(a->r)
		- (n_col * col_h)
		- (n_uncol * uncol_h);

	/* Collapse until there is room */
	if(surplus < 0) {
		i = ceil(-1.F * surplus / (uncol_h - col_h));
		if(i >= n_uncol)
			i = n_uncol - 1;
		n_uncol -= i;
		n_col += i;
		surplus += i * (uncol_h - col_h);
	}
	/* Push to the floating layer until there is room */
	if(surplus < 0) {
		i = ceil(-1.F * surplus / col_h);
		if(i > n_col)
			i = n_col;
		n_col -= i;
		surplus += i * col_h;
	}

	/* Decide which to collapse and which to float. */
	j = n_uncol - 1;
	i = n_col - 1;
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

	if(n_colp) *n_colp = n_col;
	if(n_uncolp) *n_uncolp = n_uncol;
}

void
column_settle(Area *a) {
	Frame *f;
	uint yoff, yoffcr;
	int surplus, n_uncol, n;

	n_uncol = 0;
	surplus = column_surplus(a);
	for(f=a->frame; f; f=f->anext)
		if(!f->collapsed) n_uncol++;

	if(n_uncol == 0) {
		fprint(2, "%s: Badness: No uncollapsed frames, column %d, view %q\n",
				argv0, area_idx(a), a->view->name);
		return;
	}
	if(surplus < 0)
		fprint(2, "%s: Badness: surplus = %d in column_settle, column %d, view %q\n",
				argv0, surplus, area_idx(a), a->view->name);

	yoff = a->r.min.y;
	yoffcr = yoff;
	n = surplus % n_uncol;
	surplus /= n_uncol;
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

/*
 * Returns how much a frame "wants" to grow.
 */
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

	if(Dy(f->r) >= maxh)
		return 0;
	return h.inc.y - (Dy(f->r) - h.base.y) % h.inc.y;
}

static int
comp_frame(const void *a, const void *b) {
	int ia, ib;

	ia = foo(*(Frame**)a);
	ib = foo(*(Frame**)b);
	/*
	 * I'd like to favor the selected client, but
	 * it causes windows to jump as focus changes.
	 */
	return ia < ib ? -1 :
	       ia > ib ?  1 :
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

/*
 * Frobs a column. Which is to say, *temporary* kludge.
 * Essentially seddles the column and resizes its clients.
 */
void
column_frob(Area *a) {
	Frame *f;

	for(f=a->frame; f; f=f->anext)
		f->r = f->colr;
	column_settle(a);
	if(a->view == selview)
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
		f->dy = Dy(f->colr);
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
		fprint(2, "Dieing: %s: screen: %d a: %p mode: %x floating: %d\n",
		       v->name, a->screen, a, a->mode, a->floating);
		die("not reached");
		break;
	}
	column_scale(a);
	/* XXX */
	if(a->sel->collapsed)
		area_setsel(a, a->sel);
	if(v == selview) {
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
	else
		r.min.y = max(r.min.y, a->r.min.y);

	if(fn)
		r.max.y = min(r.max.y, fn->colr.max.y - minh);
	else
		r.max.y = min(r.max.y, a->r.max.y);

	if(fp) {
		fp->colr.max.y = r.min.y;
		frame_resize(fp, fp->colr);
	}
	else
		r.min.y = min(r.min.y, r.max.y - minh);

	if(fn) {
		fn->colr.min.y = r.max.y;
		frame_resize(fn, fn->colr);
	}
	else
		r.max.y = max(r.max.y, r.min.y + minh);

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

	minw = column_minwidth();

	al = a->prev;
	ar = a->next;

	if(al)
		r.min.x = max(r.min.x, al->r.min.x + minw);
	else { /* Hm... */
		r.min.x = max(r.min.x, v->r[a->screen].min.x);
		r.max.x = max(r.max.x, r.min.x + minw);
	}

	if(ar)
		r.max.x = min(r.max.x, ar->r.max.x - minw);
	else {
		r.max.x = min(r.max.x, v->r[a->screen].max.x);
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

	view_update(v);
}

