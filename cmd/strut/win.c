/* Copyright ©2008-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "dat.h"
#include <string.h>
#include "fns.h"

void
restrut(Window *frame) {
	enum { Left, Right, Top, Bottom };
	Rectangle strut[4];
	Rectangle r;

	r = frame->r;
	memset(strut, 0, sizeof strut);
	if(Dx(r) < Dx(scr.rect)/2 && direction != DVertical) {
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
	if(Dy(r) < Dy(scr.rect)/2 && direction != DHorizontal) {
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
#define pstrut(name) \
	if(!eqrect(strut[name], ZR)) \
		fprint(2, "strut["#name"] = %R\n", strut[name])
	pstrut(Left);
	pstrut(Right);
	pstrut(Top);
	pstrut(Bottom);
#endif

	ewmh_setstrut(frame->aux, strut);
}

static void
config_event(Window *frame, void *aux, XConfigureEvent *ev) {

	frame->r = rectaddpt(Rect(ev->x, ev->y, ev->width, ev->height),
			     Pt(ev->border_width, ev->border_width));
	restrut(frame);
}

static void
destroy_event(Window *w, void *aux, XDestroyWindowEvent *ev) {

	USED(ev);
	sethandler(w, nil);
	event_looprunning = windowmap.nmemb > 0;
}

Handlers handlers = {
	.config = config_event,
	.destroy = destroy_event,
};

