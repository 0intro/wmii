/* (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */
#include "wmii.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned int
textwidth_l(BlitzFont *font, char *text, unsigned int len) {
	if(font->set) {
		XRectangle r;
		XmbTextExtents(font->set, text, len, &r, nil);
		return r.width - 1;
	}
	return XTextWidth(font->xfont, text, len);
}

unsigned int
textwidth(BlitzFont *font, char *text) {
	return textwidth_l(font, text, strlen(text));
}

void
loadfont(Blitz *blitz, BlitzFont *font) {
	char *fontname = font->fontstr;
	char **missing = nil, *def = "?";
	int n;

	if(font->set)
		XFreeFontSet(blitz->dpy, font->set);
	font->set = XCreateFontSet(blitz->dpy, fontname, &missing, &n, &def);
	if(missing) {
		while(n--)
			fprintf(stderr, "wmiiwm: missing fontset for '%s': %s\n",
					fontname, missing[n]);
		XFreeStringList(missing);
	}
	if(font->set) {
		XFontSetExtents *font_extents;
		XFontStruct **xfonts;
		char **font_names;

		font->ascent = font->descent = 0;
		font_extents = XExtentsOfFontSet(font->set);
		XFontsOfFontSet(font->set, &xfonts, &font_names);
		font->ascent = xfonts[0]->ascent;
		font->descent = xfonts[0]->descent;
	}
	else {
		if(font->xfont)
			XFreeFont(blitz->dpy, font->xfont);
		font->xfont = nil;
		font->xfont = XLoadQueryFont(blitz->dpy, fontname);
		if (!font->xfont) {
			if(!strncmp(fontname, BLITZ_FONT, sizeof(BLITZ_FONT)))
				ixp_eprint("wmiiwm: error, cannot load '%s' font\n",
						BLITZ_FONT);
			free(font->fontstr);
			font->fontstr = ixp_estrdup(BLITZ_FONT);
			return loadfont(blitz, font);
		}
		font->ascent = font->xfont->ascent;
		font->descent = font->xfont->descent;
	}
	font->height = font->ascent + font->descent;
}

unsigned int
labelh(BlitzFont *font) {
	return font->height + 2;
}

void
draw_tile(BlitzBrush *b) {
	drawbg(b->blitz->dpy, b->drawable, b->gc, &b->rect,
			b->color, True, b->border);
}

void
draw_border(BlitzBrush *b) {
	drawbg(b->blitz->dpy, b->drawable, b->gc, &b->rect,
			b->color, False, True);
}

void
draw_label(BlitzBrush *b, char *text) {
	unsigned int x, y, w, h, len;
	Bool shortened = False;
	static char buf[2048];
	XRectangle r = {0};
	XGCValues gcv;

	draw_tile(b);
	if(!text)
		return;
	shortened = 0;
	strncpy(buf, text, sizeof(buf));
	len = strlen(buf);
	gcv.foreground = b->color.fg;
	gcv.background = b->color.bg;
	h = b->font->ascent + b->font->descent;
	y = b->rect.y + b->rect.height / 2 - h / 2 + b->font->ascent;
	/* shorten text if necessary */
	while(len
	  && (w = textwidth(b->font, buf)) > b->rect.width - (b->font->height & ~1)) {
		buf[--len] = 0;
		shortened = True;
	}
	if(w > b->rect.width)
		return;
	/* mark shortened info in the string */
	if(shortened) {
		if (len > 3)
			buf[len - 3] = '.';
		if (len > 2)
			buf[len - 2] = '.';
		if (len > 1)
			buf[len - 1] = '.';
	}

	if(b->font->set) {
		XmbTextExtents(b->font->set, text, len, &r, nil);
	}

	switch (b->align) {
	case EAST:
		x = b->rect.x + b->rect.width - (w + (b->font->height / 2));
		break;
	default:
		x = b->rect.x + (b->font->height / 2) - r.x;
		break;
	}
	if(b->font->set) {
		XChangeGC(b->blitz->dpy, b->gc, GCForeground | GCBackground, &gcv);
		XmbDrawImageString(b->blitz->dpy, b->drawable, b->font->set, b->gc,
				x, y, buf, len);
	}
	else {
		gcv.font = b->font->xfont->fid;
		XChangeGC(b->blitz->dpy, b->gc, GCForeground | GCBackground | GCFont, &gcv);
		XDrawImageString(b->blitz->dpy, b->drawable, b->gc, x, y, buf, len);
	}
}

void
drawbg(Display *dpy, Drawable drawable, GC gc, XRectangle *rect,
			BlitzColor c, Bool fill, Bool border)
{
	if(fill) {
		XSetForeground(dpy, gc, c.bg);
		XFillRectangles(dpy, drawable, gc, rect, 1);
	}
	if(!border)
		return;
	XSetLineAttributes(dpy, gc, 0, LineSolid, CapButt, JoinMiter);
	XSetForeground(dpy, gc, c.border);
	XDrawRectangle(dpy, drawable, gc, rect->x, rect->y, rect->width - 1, rect->height - 1);
}

void
drawcursor(Display *dpy, Drawable drawable, GC gc,
				int x, int y, unsigned int h, BlitzColor c)
{
	XSegment s[5];

	XSetForeground(dpy, gc, c.fg);
	XSetLineAttributes(dpy, gc, 1, LineSolid, CapButt, JoinMiter);
	s[0].x1 = x - 1;
	s[0].y1 = s[0].y2 = y;
	s[0].x2 = x + 2;
	s[1].x1 = x - 1;
	s[1].y1 = s[1].y2 = y + 1;
	s[1].x2 = x + 2;
	s[2].x1 = s[2].x2 = x;
	s[2].y1 = y;
	s[2].y2 = y + h;
	s[3].x1 = x - 1;
	s[3].y1 = s[3].y2 = y + h;
	s[3].x2 = x + 2;
	s[4].x1 = x - 1;
	s[4].y1 = s[4].y2 = y + h - 1;
	s[4].x2 = x + 2;
	XDrawSegments(dpy, drawable, gc, s, 5);
}

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

char *
parse_colors(char **buf, int *buflen, BlitzColor *col) {
	unsigned int i;
	if(*buflen < 23 || 3 != sscanf(*buf, "#%06x #%06x #%06x", &i,&i,&i))
		return "bad value";
	(*buflen) -= 23;
	bcopy(*buf, col->colstr, 23);
	loadcolor(&blz, col);

	(*buf) += 23;
	if(**buf == '\n' || **buf == ' ') {
		(*buf)++;
		(*buflen)--;
	}
	return nil;
}
