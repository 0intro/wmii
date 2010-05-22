/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"

void
freeimage(Image *img) {
	if(img == nil)
		return;

	assert(img->type == WImage);

	if(img->xft)
		XftDrawDestroy(img->xft);
	XFreePixmap(display, img->xid);
	XFreeGC(display, img->gc);
	free(img);
}
