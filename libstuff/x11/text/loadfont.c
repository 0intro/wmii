/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"

Font*
loadfont(char *name) {
	XFontStruct **xfonts;
	char **missing, **font_names;
	Biobuf *b;
	Font *f;
	int n, i;

	missing = nil;
	f = emallocz(sizeof *f);
	f->name = estrdup(name);
	if(!strncmp(f->name, "xft:", 4)) {
		f->type = FXft;

		f->font.xft = XftFontOpenXlfd(display, scr.screen, f->name + 4);
		if(!f->font.xft)
			f->font.xft = XftFontOpenName(display, scr.screen, f->name + 4);
		if(!f->font.xft)
			goto error;

		f->ascent = f->font.xft->ascent;
		f->descent = f->font.xft->descent;
	}else {
		f->font.set = XCreateFontSet(display, name, &missing, &n, nil);
		if(missing) {
			if(false) {
				b = Bfdopen(dup(2), O_WRONLY);
				Bprint(b, "%s: note: missing fontset%s for '%s':", argv0,
						(n > 1 ? "s" : ""), name);
				for(i = 0; i < n; i++)
					Bprint(b, "%s %s", (i ? "," : ""), missing[i]);
				Bprint(b, "\n");
				Bterm(b);
			}
			freestringlist(missing);
		}

		if(f->font.set) {
			f->type = FFontSet;
			XFontsOfFontSet(f->font.set, &xfonts, &font_names);
			f->ascent = xfonts[0]->ascent;
			f->descent = xfonts[0]->descent;
		}else {
			f->type = FX11;
			f->font.x11 = XLoadQueryFont(display, name);
			if(!f->font.x11)
				goto error;

			f->ascent = f->font.x11->ascent;
			f->descent = f->font.x11->descent;
		}
	}
	f->height = f->ascent + f->descent;
	return f;

error:
	fprint(2, "%s: cannot load font: %s\n", argv0, name);
	f->type = 0;
	freefont(f);
	return nil;
}
