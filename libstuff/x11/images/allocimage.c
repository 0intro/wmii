/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"

Image*
allocimage(int w, int h, int depth) {
	Image *img;

	img = emallocz(sizeof *img);
	img->type = WImage;
	img->xid = XCreatePixmap(display, scr.root.xid, w, h, depth);
	img->gc = XCreateGC(display, img->xid, 0, nil);
	img->colormap = scr.colormap;
	img->visual = scr.visual;
	if(depth == 32)
		img->visual = scr.visual32;
	img->depth = depth;
	img->r = Rect(0, 0, w, h);
	return img;
}
