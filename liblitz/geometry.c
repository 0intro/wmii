/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <cext.h>

#include "blitz.h"

BlitzAlign
blitz_quadofcoord(XRectangle *rect, int x, int y)
{
	int w = x <= rect->x + rect->width / 2;
	int n = y <= rect->y + rect->height / 2;
	int e = x > rect->x + rect->width / 2;
	int s = y > rect->y + rect->height / 2;
	int nw = w && n;
	int ne = e && n;
	int sw = w && s;
	int se = e && s;

	if(nw)
		return NWEST;
	else if(ne)
		return NEAST;
	else if(se)
		return SEAST;
	else if(sw)
		return SWEST;

	return CENTER;
}

Bool blitz_ispointinrect(int x, int y, XRectangle * r)
{
	return (x >= r->x) && (x <= r->x + r->width)
		&& (y >= r->y) && (y <= r->y + r->height);
}


/* Syntax: <x> <y> <width> <height> */
int blitz_strtorect(XRectangle *r, const char *val)
{
	XRectangle new;
	if (!val)
		return -1;

	if(sscanf(val, "%hd %hd %hu %hu", &new.x, &new.y, &new.width, &new.height) != 4)
		return -1;

	*r = new;
	return 0;
}
