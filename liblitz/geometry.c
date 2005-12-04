/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <math.h>
#include <string.h>
#include <stdlib.h>

#include "blitz.h"
#include <cext.h>

static int strtoalign(Align * result, char *val)
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
		return FALSE;
	return TRUE;
}

/**
 * Basic Syntax: <x>,<y>,<width>,<height>
 * Each component can be of following format:
 * <...> = [+|-]0..n|<alignment>[[+|-]0..n]
 */
int
blitz_strtorect(Display * dpy, XRectangle * root, XRectangle * r,
				char *val)
{
	char buf[64];
	char *x, *y, *w, *h;
	char *p;
	int sx, sy, sw, sh;

	if (!val)
		return FALSE;
	sx = sy = sw = sh = 0;
	x = y = w = h = 0;
	_strlcpy(buf, val, sizeof(buf));

	x = strtok_r(buf, ",", &p);
	if (x) {
		y = strtok_r(0, ",", &p);
		if (y) {
			w = strtok_r(0, ",", &p);
			if (w) {
				h = strtok_r(0, "", &p);
			}
		}
	}
	if (x && (sx = (x[0] >= '0') && (x[0] <= '9')))
		r->x = _strtonum(x, 0, 65535);
	if (y && (sy = (y[0] >= '0') && (y[0] <= '9')))
		r->y = _strtonum(y, 0, 65535);
	if (w && (sw = (w[0] >= '0') && (w[0] <= '9')))
		r->width = _strtonum(w, 0, 65535);
	if (h && (sh = (h[0] >= '0') && (h[0] <= '9')))
		r->height = _strtonum(h, 0, 65535);

	if (!sx && !sw && x && w
		&& x[0] != '-' && x[0] != '+' && w[0] != '-' && w[0] != '+') {
		Align ax, aw;
		strtoalign(&ax, x);
		strtoalign(&aw, w);
		if ((ax == CENTER) && (aw == EAST)) {
			r->x = root->x + root->width / 2;
			r->width = root->width / 2;
		} else {
			r->x = root->x;
			if (aw == CENTER) {
				r->width = root->width / 2;
			} else {
				r->width = root->width;
			}
		}
	} else if (!sx && x && x[0] != '-' && x[0] != '+') {
		Align ax;
		strtoalign(&ax, x);
		if (ax == CENTER) {
			r->x = root->x + (root->width / 2) - (r->width / 2);
		} else if (ax == EAST) {
			r->x = root->x + root->width - r->width;
		} else {
			r->x = root->x;
		}
	} else if (!sw && w && w[0] != '-' && w[0] != '+') {
		Align aw;
		strtoalign(&aw, w);
		if (aw == CENTER) {
			r->width = (root->width / 2) - r->x;
		} else {
			r->width = root->width - r->x;
		}
	}
	if (!sy && !sh && y && h
		&& y[0] != '-' && y[0] != '+' && h[0] != '-' && h[0] != '+') {
		Align ay, ah;
		strtoalign(&ay, y);
		strtoalign(&ah, h);
		if ((ay == CENTER) && (ah == SOUTH)) {
			r->y = root->y + root->height / 2;
			r->height = root->height / 2;
		} else {
			r->y = root->y;
			if (ah == CENTER) {
				r->height = root->height / 2;
			} else {
				r->height = root->height;
			}
		}
	} else if (!sy && y && y[0] != '-' && y[0] != '+') {
		Align ay;
		strtoalign(&ay, y);
		if (ay == CENTER) {
			r->y = root->y + (root->height / 2) - (r->height / 2);
		} else if (ay == SOUTH) {
			r->y = root->y + root->height - r->height;
		} else {
			r->y = root->y;
		}
	} else if (!sh && h && h[0] != '-' && h[0] != '+') {
		Align ah;
		strtoalign(&ah, h);
		if (ah == CENTER) {
			r->height = (root->height / 2) - r->y;
		} else {
			r->height = root->height - r->y;
		}
	}
	/* now do final calculations */
	if (x) {
		p = strchr(x, '-');
		if (p)
			r->x -= _strtonum(++p, 0, 65535);
		p = strchr(x, '+');
		if (p)
			r->x += _strtonum(++p, 0, 65535);
	}
	if (y) {
		p = strchr(y, '-');
		if (p)
			r->y -= _strtonum(++p, 0, 65535);
		p = strchr(y, '+');
		if (p)
			r->y += _strtonum(++p, 0, 65535);
	}
	if (w) {
		p = strchr(w, '-');
		if (p)
			r->width -= _strtonum(++p, 0, 65535);
		p = strchr(w, '+');
		if (p)
			r->width += _strtonum(++p, 0, 65535);
	}
	if (h) {
		p = strchr(h, '-');
		if (p)
			r->height -= _strtonum(++p, 0, 65535);
		p = strchr(h, '+');
		if (p)
			r->height += _strtonum(++p, 0, 65535);
	}
	return TRUE;
}

int blitz_ispointinrect(int x, int y, XRectangle * r)
{
	return (x >= r->x) && (x <= r->x + r->width)
		&& (y >= r->y) && (y <= r->y + r->height);
}

int blitz_distance(XRectangle * origin, XRectangle * target)
{
	int ox = origin->x + origin->width / 2;
	int oy = origin->y + origin->height / 2;
	int tx = target->x + target->width / 2;
	int ty = target->y + target->height / 2;

	return (int) sqrt((double) (((ox - tx) * (ox - tx)) +
								((oy - ty) * (oy - ty))));
}

void
blitz_getbasegeometry(void **items, unsigned int *size,
					  unsigned int *cols, unsigned int *rows)
{
	float sq, dummy;

	*size = count_items((void **) items);
	sq = sqrt(*size);
	if (modff(sq, &dummy) < 0.5)
		*rows = floor(sq);
	else
		*rows = ceil(sq);
	*cols = ((*rows) * (*rows)) < (*size) ? *rows + 1 : *rows;
}
