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
		ret |= East;
	if(pt.x <= Dx(r) * .5)
		ret |= West;
	if(pt.y <= Dy(r) * .5)
		ret |= North;
	if(pt.y >= Dy(r) * .5)
		ret |= South;

	return ret;
}

Cursor
quad_cursor(Align align) {
	switch(align) {
	case NEast:
		return cursor[CurNECorner];
	case NWest:
		return cursor[CurNWCorner];
	case SEast:
		return cursor[CurSECorner];
	case SWest:
		return cursor[CurSWCorner];
	default:
		return cursor[CurMove];
	}
}

Align
get_sticky(Rectangle src, Rectangle dst) {
	Align stickycorner = 0;

	if(src.min.x != dst.min.x && src.max.x == dst.max.x)
		stickycorner |= East;
	else
		stickycorner |= West;
	if(src.min.y != dst.min.y && src.max.y == dst.max.y)
		stickycorner |= South;
	else    
		stickycorner |= North;

	return stickycorner;
}
