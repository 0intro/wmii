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
static bool	randr_event_p(XEvent *e);

typedef void (*EvHandler)(XEvent*);
static EvHandler	randr_handlers[RRNumberEvents];

bool	have_RandR;
int	randr_eventbase;

static void
handle(XEvent *e, EvHandler ha[], int base) {

	if(ha[e->type-base])
		ha[e->type-base](e);
}

void
xext_init(void) {
	randr_init();
}

void
xext_event(XEvent *e) {

	if(randr_event_p(e))
		handle(e, randr_handlers, randr_eventbase);
}

void
randr_init(void) {
	int errorbase, major, minor;

	have_RandR = XRRQueryExtension(display, &randr_eventbase, &errorbase);
	if(have_RandR)
		if(XRRQueryVersion(display, &major, &minor) && major < 1)
			have_RandR = false;
	if(have_RandR)
		XRRSelectInput(display, scr.root.w, RRScreenChangeNotifyMask);
}

static bool
randr_event_p(XEvent *e) {
	return have_RandR
	    && (uint)e->type - randr_eventbase < RRNumberEvents;
}

static void
randr_screenchange(XRRScreenChangeNotifyEvent *ev) {
	View *v;
	Point d;

	XRRUpdateConfiguration((XEvent*)ev);
	if(ev->rotation+90 % 180)
		scr.rect = Rect(0, 0, ev->width, ev->height);
	else
		scr.rect = Rect(0, 0, ev->height, ev->width);

	d.x = Dx(scr.rect) - Dx(screen->r);
	d.y = Dy(scr.rect) - Dy(screen->r);
	for(v=view; v; v=v->next) {
		v->r.max.x += d.x;
		v->r.max.y += d.y;
	}
	init_screen(screen);
	bar_resize(screen);
}

static EvHandler randr_handlers[] = {
	[RRScreenChangeNotify] = (EvHandler)randr_screenchange,
};

