/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"

ulong
pixelvalue(Window *w, Color *c) {
	XColor xc;
	ulong pixel;

	if(w->visual->class != TrueColor) {
		if(c->pixel != ~0UL)
			return c->pixel;
		xc.red = c->red;
		xc.green = c->green;
		xc.blue = c->blue;
		XAllocColor(display, w->colormap, &xc);
		return c->pixel = xc.pixel;
	}
	pixel = ((ulong)(c->alpha&0xff00) << 16)
	      | ((ulong)(c->red&0xff00) << 8)
	      | ((ulong)(c->green&0xff00) << 0)
	      | ((ulong)(c->blue&0xff00) >> 8);
	if(w->depth < 32)
		pixel |= 0xffUL << 24;
	return pixel;
}

bool
parsecolor(const char *name, Color *ret) {
	ret->pixel = ~0UL;
	return XRenderParseColor(display, (char*)(uintptr_t)name, (XRenderColor*)ret);
}
