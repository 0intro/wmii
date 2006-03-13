/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <math.h>
#include <string.h>
#include <stdlib.h>

#include "blitz.h"

int blitz_strtoalign(Align *result, char *val)
{
	/*
	 * note, resize allows syntax like "east-20", this we cannot do
	 * include zero termination in strncmp checking!
	 */

	*result = CENTER;
	if (!strncmp(val, "west", 4))
		*result = WEST;
	else if (!strncmp(val, "nwest", 5))
		*result = NWEST;
	else if (!strncmp(val, "north", 5))
		*result = NORTH;
	else if (!strncmp(val, "neast", 5))
		*result = NEAST;
	else if (!strncmp(val, "east", 4))
		*result = EAST;
	else if (!strncmp(val, "seast", 5))
		*result = SEAST;
	else if (!strncmp(val, "south", 5))
		*result = SOUTH;
	else if (!strncmp(val, "swest", 5))
		*result = SWEST;
	else if (!strncmp(val, "center", 6))
		*result = CENTER;
	else
		return -1;
	return 0;
}

/**
 * Basic Syntax: <x> <y> <width> <height>
 * Each component can be of following format:
 * <...> = [+|-]0..n|<alignment>[[+|-]0..n]
 */
int blitz_strtorect(XRectangle *root, XRectangle *r, char *val)
{
	const char *err;
	char buf[64];
	char *x, *y, *w, *h;
	char *p;
	int rx, ry, rw, rh, sx, sy, sw, sh;

	if (!val)
		return -1;
	rx = r->x;
	ry = r->y;
	rw = r->width;
	rh = r->height;
	sx = sy = sw = sh = 0;
	x = y = w = h = 0;
	cext_strlcpy(buf, val, sizeof(buf));

	x = strtok_r(buf, " ", &p);
	if (x) {
		y = strtok_r(0, " ", &p);
		if (y) {
			w = strtok_r(0, " ", &p);
			if (w) {
				h = strtok_r(0, "", &p);
			}
		}
	}
	if (x && (sx = (x[0] >= '0') && (x[0] <= '9')))
		rx = cext_strtonum(x, 0, 65535, &err);
	if (y && (sy = (y[0] >= '0') && (y[0] <= '9')))
		ry = cext_strtonum(y, 0, 65535, &err);
	if (w && (sw = (w[0] >= '0') && (w[0] <= '9')))
		rw = cext_strtonum(w, 0, 65535, &err);
	if (h && (sh = (h[0] >= '0') && (h[0] <= '9')))
		rh = cext_strtonum(h, 0, 65535, &err);

	if (!sx && !sw && x && w
		&& x[0] != '-' && x[0] != '+' && w[0] != '-' && w[0] != '+') {
		Align ax, aw;
		blitz_strtoalign(&ax, x);
		blitz_strtoalign(&aw, w);
		if ((ax == CENTER) && (aw == EAST)) {
			rx = root->x + root->width / 2;
			rw = root->width / 2;
		} else {
			rx = root->x;
			if (aw == CENTER) {
				rw = root->width / 2;
			} else {
				rw = root->width;
			}
		}
	} else if (!sx && x && x[0] != '-' && x[0] != '+') {
		Align ax;
		blitz_strtoalign(&ax, x);
		if (ax == CENTER) {
			rx = root->x + (root->width / 2) - (rw / 2);
		} else if (ax == EAST) {
			rx = root->x + root->width - rw;
		} else {
			rx = root->x;
		}
	} else if (!sw && w && w[0] != '-' && w[0] != '+') {
		Align aw;
		blitz_strtoalign(&aw, w);
		if (aw == CENTER) {
			rw = (root->width / 2) - rx;
		} else {
			rw = root->width - rx;
		}
	}
	if (!sy && !sh && y && h
		&& y[0] != '-' && y[0] != '+' && h[0] != '-' && h[0] != '+') {
		Align ay, ah;
		blitz_strtoalign(&ay, y);
		blitz_strtoalign(&ah, h);
		if ((ay == CENTER) && (ah == SOUTH)) {
			ry = root->y + root->height / 2;
			rh = root->height / 2;
		} else {
			ry = root->y;
			if (ah == CENTER) {
				rh = root->height / 2;
			} else {
				rh = root->height;
			}
		}
	} else if (!sy && y && y[0] != '-' && y[0] != '+') {
		Align ay;
		blitz_strtoalign(&ay, y);
		if (ay == CENTER) {
			ry = root->y + (root->height / 2) - (rh / 2);
		} else if (ay == SOUTH) {
			ry = root->y + root->height - rh;
		} else {
			ry = root->y;
		}
	} else if (!sh && h && h[0] != '-' && h[0] != '+') {
		Align ah;
		blitz_strtoalign(&ah, h);
		if (ah == CENTER) {
			rh = (root->height / 2) - ry;
		} else {
			rh = root->height - ry;
		}
	}
	/* now do final calculations */
	if (x) {
		p = strchr(x, '-');
		if (p)
			rx -= cext_strtonum(++p, 0, 65535, &err);
		p = strchr(x, '+');
		if (p)
			rx += cext_strtonum(++p, 0, 65535, &err);
	}
	if (y) {
		p = strchr(y, '-');
		if (p)
			ry -= cext_strtonum(++p, 0, 65535, &err);
		p = strchr(y, '+');
		if (p)
			ry += cext_strtonum(++p, 0, 65535, &err);
	}
	if (w) {
		p = strchr(w, '-');
		if (p)
			rw -= cext_strtonum(++p, 0, 65535, &err);
		p = strchr(w, '+');
		if (p)
			rw += cext_strtonum(++p, 0, 65535, &err);
	}
	if (h) {
		p = strchr(h, '-');
		if (p)
			rh -= cext_strtonum(++p, 0, 65535, &err);
		p = strchr(h, '+');
		if (p)
			rh += cext_strtonum(++p, 0, 65535, &err);
	}

	if (rw < 1)
		rw = 10;
	if (rh < 1)
		rh = 10;
	r->x = rx;
	r->y = ry;
	r->width = rw;
	r->height = rh;
	return 0;
}

Bool blitz_ispointinrect(int x, int y, XRectangle * r)
{
	return (x >= r->x) && (x <= r->x + r->width) && (y >= r->y) && (y <= r->y + r->height);
}

int blitz_distance(XRectangle * origin, XRectangle * target)
{
	int ox = origin->x + origin->width / 2;
	int oy = origin->y + origin->height / 2;
	int tx = target->x + target->width / 2;
	int ty = target->y + target->height / 2;

	return (int) sqrt((double) (((ox - tx) * (ox - tx)) + ((oy - ty) * (oy - ty))));
}

void blitz_getbasegeometry(unsigned int size, unsigned int *cols, unsigned int *rows)
{
	float sq, dummy;

	sq = sqrt(size);
	if (modff(sq, &dummy) < 0.5)
		*rows = floor(sq);
	else
		*rows = ceil(sq);
	*cols = ((*rows) * (*rows)) < (size) ? *rows + 1 : *rows;
}
