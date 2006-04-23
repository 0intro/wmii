/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdio.h>
#include <stdlib.h>

#include "blitz.h"

void
blitz_loadfont(Display *dpy, BlitzFont *font, char *fontstr)
{
	if(font->font)
		XFreeFont(dpy, font->font);
	font->font = XLoadQueryFont(dpy, fontstr);
	if (!font->font) {
		font->font = XLoadQueryFont(dpy, "fixed");
		if (!font->font) {
			fprintf(stderr, "%s", "liblitz: error, cannot load fixed font\n");
			exit(1);
		}
	}
}
