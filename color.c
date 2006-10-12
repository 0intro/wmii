/* (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */
#include "wm.h"
#include <string.h>

static unsigned long
xloadcolor(Blitz *blitz, char *colstr) {
	XColor color;
	char col[8];

	strncpy(col, colstr, sizeof(col));
	col[7] = 0;
	XAllocNamedColor(blitz->dpy,
			DefaultColormap(blitz->dpy, blitz->screen), col, &color, &color);
	return color.pixel;
}

int
loadcolor(Blitz *blitz, BlitzColor *c) {
	if(!c->colstr || strlen(c->colstr) != 23)
		return -1;
	c->fg = xloadcolor(blitz, &c->colstr[0]);
	c->bg = xloadcolor(blitz, &c->colstr[8]);
	c->border = xloadcolor(blitz, &c->colstr[16]);
	return 0;
}
