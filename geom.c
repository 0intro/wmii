/* © 2004-2006 Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */
#include "wmii.h"

Bool
ispointinrect(int x, int y, XRectangle * r) {
	return (x >= r->x) && (x < r_east(r))
		&& (y >= r->y) && (y < r_south(r));
}

BlitzAlign
quadofcoord(XRectangle *rect, int x, int y) {
	BlitzAlign ret = 0;
	x -= rect->x;
	y -= rect->y;

	if(x >= rect->width * .5)
		ret |= EAST;
	if(x <= rect->width * .5)
		ret |= WEST;
	if(y <= rect->height * .5)
		ret |= NORTH;
	if(y >= rect->height * .5)
		ret |= SOUTH;

	return ret;
}

Cursor
cursor_of_quad(BlitzAlign align) {
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

/* Syntax: <x> <y> <width> <height> */
int
strtorect(XRectangle *r, const char *val) {
	XRectangle new;
	if (!val)
		return -1;

	if(sscanf(val, "%hd %hd %hu %hu", &new.x, &new.y, &new.width, &new.height) != 4)
		return -1;

	*r = new;
	return 0;
}

int
r_east(XRectangle *r) {
	return r->x + r->width;
}

int
r_south(XRectangle *r) {
	return r->y + r->height;
}

BlitzAlign
get_sticky(XRectangle *src, XRectangle *dst) {
	BlitzAlign stickycorner = 0;

	if(src->x != dst->x && r_east(src) == r_east(dst))
		stickycorner |= EAST;
	else
		stickycorner |= WEST;
	if(src->y != dst->y && r_south(src) == r_south(dst))
		stickycorner |= SOUTH;
	else    
		stickycorner |= NORTH;

	return stickycorner;
}
