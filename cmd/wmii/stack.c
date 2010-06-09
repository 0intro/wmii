/* Copyright ©2009-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "dat.h"
#include "fns.h"

void
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

void
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

