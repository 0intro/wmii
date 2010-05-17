/* Copyright ©2008-2010 Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */
#include "dat.h"
#include <string.h>
#include "fns.h"

void
restrut(void) {
	enum { Left, Right, Top, Bottom };
	Rectangle strut[4];
	Rectangle r;

	r = frame.r;
	memset(strut, 0, sizeof strut);
	if(Dx(r) < Dx(scr.rect)/2) {
		if(r.min.x <= scr.rect.min.x) {
			strut[Left] = r;
			strut[Left].min.x = 0;
			strut[Left].max.x -= scr.rect.min.x;
		}
		if(r.max.x >= scr.rect.max.x) {
			strut[Right] = r;
			strut[Right].min.x -= scr.rect.max.x;
			strut[Right].max.x = 0;
		}
	}
	if(Dy(r) < Dy(scr.rect)/2) {
		if(r.min.y <= scr.rect.min.y) {
			strut[Top] = r;
			strut[Top].min.y = 0;
			strut[Top].max.y -= scr.rect.min.y;
		}
		if(r.max.y >= scr.rect.max.y) {
			strut[Bottom] = r;
			strut[Bottom].min.y -= scr.rect.max.y;
			strut[Bottom].max.y = 0;
		}
	}

#define pstrut(name) \
	if(!eqrect(strut[name], ZR)) \
		fprint(2, "strut["#name"] = %R\n", strut[name])
	/* Choose the struts which take up the least space.
	 * Not ideal.
	 */
	if(Dy(strut[Top])) {
		if(Dx(strut[Left]))
			if(Dy(strut[Top]) < Dx(strut[Left]))
				strut[Left] = ZR;
			else
				strut[Top] = ZR;
		if(Dx(strut[Right]))
			if(Dy(strut[Top]) < Dx(strut[Right]))
				strut[Right] = ZR;
			else
				strut[Top] = ZR;
	}
	if(Dy(strut[Bottom])) {
		if(Dx(strut[Left]))
			if(Dy(strut[Bottom]) < Dx(strut[Left]))
				strut[Left] = ZR;
			else
				strut[Bottom] = ZR;
		if(Dx(strut[Right]))
			if(Dy(strut[Bottom]) < Dx(strut[Right]))
				strut[Right] = ZR;
			else
				strut[Bottom] = ZR;
	}

#if 0
	pstrut(Left);
	pstrut(Right);
	pstrut(Top);
	pstrut(Bottom);
#endif

	ewmh_setstrut(&win, strut);
}

static void
config(Window *w, XConfigureEvent *ev) {

	USED(w);

	frame.r = rectaddpt(Rect(0, 0, ev->width, ev->height),
			    Pt(ev->x+ev->border_width, ev->y+ev->border_width));
	restrut();
}

static void
destroy(Window *w, XDestroyWindowEvent *ev) {
	USED(w, ev);
	running = false;
}

Handlers handlers = {
	.config = config,
	.destroy = destroy,
};

