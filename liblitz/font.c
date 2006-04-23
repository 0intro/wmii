/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <cext.h>

#include "blitz.h"

void
blitz_loadfont(Display *dpy, BlitzFont *font, char *fontstr)
{
	if(font->xfont)
		XFreeFont(dpy, font->xfont);

	font->xfont = XLoadQueryFont(dpy, fontstr);
	if (!font->xfont)
		font->xfont = XLoadQueryFont(dpy, "fixed");

	if (!font->xfont) {
		fprintf(stderr, "%s", "liblitz: error, cannot load 'fixed' font\n");
		exit(1);
	}
}
