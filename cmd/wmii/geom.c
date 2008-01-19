/* Copyright ©2006-2008 Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */
#include "dat.h"
#include "fns.h"

bool
rect_haspoint_p(Point pt, Rectangle r) {
	return (pt.x >= r.min.x) && (pt.x < r.max.x)
		&& (pt.y >= r.min.y) && (pt.y < r.max.y);
}

bool
rect_intersect_p(Rectangle r, Rectangle r2) {
	return r.min.x <= r2.max.x
	    && r.max.x >= r2.min.x
	    && r.min.y <= r2.max.y
	    && r.max.y >= r2.min.y;
}

Rectangle
rect_intersection(Rectangle r, Rectangle r2) {
	Rectangle ret;

	/* canonrect(ret) != ret if not intersection */
	ret.min.x = max(r.min.x, r2.min.x);
	ret.max.x = min(r.max.x, r2.max.x);
	ret.min.y = max(r.min.y, r2.min.y);
	ret.max.y = min(r.max.y, r2.max.y);
	return ret;
}

bool
rect_contains_p(Rectangle r, Rectangle r2) {
	return r2.min.x >= r.min.x
	    && r2.max.x <= r.max.x
	    && r2.min.y >= r.min.y
	    && r2.max.y <= r.max.y;
}

Align
quadrant(Rectangle r, Point pt) {
	Align ret;

	pt = subpt(pt, r.min);
	ret = 0;

	if(pt.x >= Dx(r) * .5)
		ret |= EAST;
	if(pt.x <= Dx(r) * .5)
		ret |= WEST;
	if(pt.y <= Dy(r) * .5)
		ret |= NORTH;
	if(pt.y >= Dy(r) * .5)
		ret |= SOUTH;

	return ret;
}

Cursor
quad_cursor(Align align) {
	switch(align) {
	case NEAST:
		return cursor[CurNECorner];
	case NWEST:
		return cursor[CurNWCorner];
	case SEAST:
		return cursor[CurSECorner];
	case SWEST:
		return cursor[CurSWCorner];
	default:
		return cursor[CurMove];
	}
}

Align
get_sticky(Rectangle src, Rectangle dst) {
	Align stickycorner = 0;

	if(src.min.x != dst.min.x && src.max.x == dst.max.x)
		stickycorner |= EAST;
	else
		stickycorner |= WEST;
	if(src.min.y != dst.min.y && src.max.y == dst.max.y)
		stickycorner |= SOUTH;
	else    
		stickycorner |= NORTH;

	return stickycorner;
}

/* XXX: These don't belong here. */
/* Blech. */
#define VECTOR(type, nam, c) \
void                                                                    \
vector_##c##init(Vector_##nam *v) {                                     \
	memset(v, 0, sizeof *v);                                        \
}                                                                       \
                                                                        \
void                                                                    \
vector_##c##free(Vector_##nam *v) {                                     \
	free(v->ary);                                                   \
	memset(v, 0, sizeof *v);                                        \
}                                                                       \
                                                                        \
void                                                                    \
vector_##c##push(Vector_##nam *v, type val) {                           \
	if(v->n == v->size) {                                           \
		if(v->size == 0)                                        \
			v->size = 2;                                    \
		v->size <<= 2;                                          \
		v->ary = erealloc(v->ary, v->size * sizeof *v->ary);    \
	}                                                               \
	v->ary[v->n++] = val;                                           \
}                                                                       \

VECTOR(long, long, l)
VECTOR(Rectangle, rect, r)

