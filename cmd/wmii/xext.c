/* Copyright ©2008 Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */
#define _X11_VISIBLE
#include "dat.h"
#include "fns.h"

#if RANDR_MAJOR < 1
#  error XRandR versions less than 1.0 are not supported
#endif

static void	randr_screenchange(XRRScreenChangeNotifyEvent*);

bool	have_RandR;
int	randr_eventbase;

void
xext_init(void) {
	randr_init();
}

void
xext_event(XEvent *e) {

	if(have_RandR && (ulong)(e->type - randr_eventbase) < RRNumberEvents)
		randr_event(e);
}

void
randr_init(void) {
	int errorbase, major, minor;

	have_RandR = XRRQueryExtension(display, &randr_eventbase, &errorbase);
	if(have_RandR)
		if(XRRQueryVersion(display, &major, &minor) && major < 1)
			have_RandR = false;
	if(!have_RandR)
		return;
	XRRSelectInput(display, scr.root.w, RRScreenChangeNotifyMask);
}

static void
randr_screenchange(XRRScreenChangeNotifyEvent *ev) {
	View *v;
	Point d;

	XRRUpdateConfiguration((XEvent*)ev);
	
	d.x = ev->width - Dx(screen->r);
	d.y = ev->height - Dy(screen->r);
	for(v=view; v; v=v->next) {
		v->r.max.x += d.x;
		v->r.max.y += d.y;
	}
	screen->r = Rect(0, 0, ev->width, ev->height);
	bar_resize(screen);
}

void
randr_event(XEvent *e) {

	switch(e->type-randr_eventbase) {
	default:
		break;
	case RRScreenChangeNotify: /* Yuck. */
		randr_screenchange((XRRScreenChangeNotifyEvent*)e);
		break;
	}
}

