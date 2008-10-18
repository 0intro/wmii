/* Copyright ©2006-2008 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "dat.h"
#include <math.h>
#include <stdlib.h>
#include "fns.h"

#ifdef notdef
void
mapscreens(void) {
	WMScreen *s, *ss;
	Rectangle r;
	int i, j;

#define frob(left, min, max, x, y) \
	if(Dy(r) > 0) /* If they intersect at some point on this axis */        \
	if(ss->r.min.x < s->r.min.x) {                                          \
		if((!s->left)                                                   \
		|| (abs(Dy(r)) < abs(s->left.max.x - s->min.x))) \
			s->left = ss;                                           \
	}

	/* Variable hell? Certainly. */
	for(i=0; i < nscreens; i++) {
		s = screens[i];
		for(j=0; j < nscreens; j++) {
			if(i == j)
				continue;
			ss = screens[j];
			r = rect_intersection(ss->r, s->r);
			frob(left,   min, max, x, y);
			frob(right,  max, min, x, y);
			frob(atop,   min, max, y, x);
			frob(below,  max, min, y, x);
		}
	}
#undef frob
}

int	findscreen(Rectangle, int);
int
findscreen(Rectangle rect, int direction) {
	Rectangle r;
	WMScreen *ss, *s;
	int best, i, j;

	best = -1;
#define frob(min, max, x, y)
	if(Dy(r) > 0) /* If they intersect at some point on this axis */
	if(ss->r.min.x < rect.min.x) {
		if(best == -1
		|| (abs(ss->r.max.x - rect.min.x) < abs(screens[best]->r.max.x - rect.min.x)))
			best = s->idx;
	}

	/* Variable hell? Certainly. */
	for(i=0; i < nscreens; i++) {
		ss = screens[j];
		r = rect_intersection(ss->r, rect);
		switch(direction) {
		default:
			return -1;
		case West:
			frob(min, max, x, y);
			break;
		case East:
			frob(max, min, x, y);
			break;
		case North:
			frob(min, max, y, x);
			break;
		case South:
			frob(max, min, y, x);
			break;
		}
	}
#undef frob
}
#endif

int
ownerscreen(Rectangle r) {
	Rectangle isect;
	int s, a, best, besta;

	SET(besta);
	best = -1;
	for(s=0; s < nscreens; s++) {
		isect = rect_intersection(r, screens[s]->r);
		a = Dx(isect) * Dy(isect);
		if(best < 0 || a > besta) {
			besta = a;
			best = s;
		}
	}
	return best;
}

