/* (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */
#include "wmii.h"

Bool
ispointinrect(int x, int y, XRectangle * r) {
	return (x >= r->x) && (x <= r->x + r->width)
		&& (y >= r->y) && (y <= r->y + r->height);
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
