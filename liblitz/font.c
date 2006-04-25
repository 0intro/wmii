/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cext.h>

#include "blitz.h"

unsigned int
blitz_textwidth(Display *dpy, BlitzFont *font, char *text)
{
	XRectangle r;
	if(font->set) {
		XmbTextExtents(font->set, text, strlen(text), nil, &r);
		return r.width;
	}
	return XTextWidth(font->xfont, text, strlen(text));
}

void
blitz_loadfont(Display *dpy, BlitzFont *font, char *fontstr)
{
	char *fontname = fontstr;
	char **missing = nil, *def = "?";
	int nmissing;
	if(font->set)
		XFreeFontSet(dpy, font->set);
	if(font->xfont)
		XFreeFont(dpy, font->xfont);
	font->xfont = XLoadQueryFont(dpy, fontname);
	if (!font->xfont) {
		fontname = "fixed";
		font->xfont = XLoadQueryFont(dpy, fontname);
	}
	if (!font->xfont) {
		fprintf(stderr, "%s", "liblitz: error, cannot load 'fixed' font\n");
		exit(1);
	}
	font->set = XCreateFontSet(dpy, fontname, &missing, &nmissing, &def);

	if(missing) {
		 while(nmissing--)
			 fprintf(stderr, "liblitz: missing fontset: %s\n", missing[nmissing]);
		XFreeStringList(missing);
	}
}
