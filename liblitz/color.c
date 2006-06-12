/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <string.h>
#include <cext.h>

#include "blitz.h"

static unsigned long
xloadcolor(Blitz *blitz, char *colstr)
{
	XColor color;
	char col[8];

	cext_strlcpy(col, colstr, sizeof(col));
	col[7] = 0;
	XAllocNamedColor(blitz->display,
			DefaultColormap(blitz->display, blitz->screen), col, &color, &color);
	return color.pixel;
}

int
blitz_loadcolor(Blitz *blitz, BlitzColor *c, char *colstr)
{
	if(!colstr || strlen(colstr) != 23)
		return -1;
	c->fg = xloadcolor(blitz, &colstr[0]);
	c->bg = xloadcolor(blitz, &colstr[8]);
	c->border = xloadcolor(blitz, &colstr[16]);
	return 0;
}

