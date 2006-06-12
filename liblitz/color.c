/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <string.h>
#include <cext.h>

#include "blitz.h"

static unsigned long
xloadcolor(char *colstr)
{
	XColor color;
	char col[8];

	cext_strlcpy(col, colstr, sizeof(col));
	col[7] = 0;
	XAllocNamedColor(__blitz.display,
			DefaultColormap(__blitz.display, __blitz.screen), col, &color, &color);
	return color.pixel;
}

int
blitz_loadcolor(BlitzColor *c, char *colstr)
{
	if(!colstr || strlen(colstr) != 23)
		return -1;
	c->fg = xloadcolor(&colstr[0]);
	c->bg = xloadcolor(&colstr[8]);
	c->border = xloadcolor(&colstr[16]);
	return 0;
}

