/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <cext.h>

#include "blitz.h"

unsigned int
blitz_textwidth(Display *dpy, BlitzFont *font, char *text)
{
	if(font->set) {
		XRectangle r;
		Xi18nTextExtents(font->set, text, strlen(text), nil, &r);
		return r.width;
	}
	return XTextWidth(font->xfont, text, strlen(text));
}

void
blitz_loadfont(Display *dpy, BlitzFont *font, char *fontstr)
{
	char *fontname = fontstr;
	char **missing = nil, *def = "?";
	char *loc = setlocale(LC_ALL, "");
	int n;

	if(font->set)
		XFreeFontSet(dpy, font->set);
	font->set = nil;
	if(!loc || !strncmp(loc, "C", 2) || !strncmp(loc, "POSIX", 6)|| !XSupportsLocale()) {
		font->set = XCreateFontSet(dpy, fontname, &missing, &n, &def);
		if(missing) {
			while(n--)
				fprintf(stderr, "liblitz: missing fontset: %s\n", missing[n]);
			XFreeStringList(missing);
		}
	}
	if(font->set) {
		XFontSetExtents *font_extents;
		XFontStruct **xfonts;
		char **font_names;
		unsigned int i;

		font->ascent = font->descent = 0;
		font_extents = XExtentsOfFontSet(font->set);
		n = XFontsOfFontSet(font->set, &xfonts, &font_names);
		for(i = 0, font->ascent = 0, font->descent = 0; i < n; i++) {
			if(font->ascent < (*xfonts)->ascent)
				font->ascent = (*xfonts)->ascent;
			if(font->descent < (*xfonts)->descent)
				font->descent = (*xfonts)->descent;
			xfonts++;
		}
	}
	else {
		if(font->xfont)
			XFreeFont(dpy, font->xfont);
		font->xfont = nil;
		font->xfont = XLoadQueryFont(dpy, fontname);
		if (!font->xfont) {
			fontname = "fixed";
			font->xfont = XLoadQueryFont(dpy, fontname);
		}
		if (!font->xfont) {
			fprintf(stderr, "%s", "liblitz: error, cannot load 'fixed' font\n");
			exit(1);
		}
		font->ascent = font->xfont->ascent;
		font->descent = font->xfont->descent;
	}
}
