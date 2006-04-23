/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <string.h>
#include <cext.h>

#include "blitz.h"

static unsigned long
xloadcolor(Display *dpy, int mon, char *colstr)
{
	XColor color;
	char col[8];

	cext_strlcpy(col, colstr, sizeof(col));
	col[7] = 0;
	XAllocNamedColor(dpy, DefaultColormap(dpy, mon), col, &color, &color);
	return color.pixel;
}

int
blitz_loadcolor(Display *dpy, BlitzColor *c, int mon, char *colstr)
{
	if(!colstr || strlen(colstr) != 23)
		return -1;
	c->fg = xloadcolor(dpy, mon, &colstr[0]);
	c->bg = xloadcolor(dpy, mon, &colstr[8]);
	c->border = xloadcolor(dpy, mon, &colstr[16]);
	return 0;
}

