/* Copyright ©2008 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#define _X11_VISIBLE
#include "dat.h"
#include <X11/extensions/Xrender.h>
#include <X11/extensions/Xinerama.h>
#include "fns.h"

#if RANDR_MAJOR < 1
#  error XRandR versions less than 1.0 are not supported
#endif

static void	randr_screenchange(XRRScreenChangeNotifyEvent*);
static bool	randr_event_p(XEvent *e);
static void	randr_init(void);
static void	render_init(void);
static void	xinerama_init(void);

typedef void (*EvHandler)(XEvent*);
static EvHandler	randr_handlers[RRNumberEvents];

bool	have_RandR;
bool	have_render;
bool	have_xinerama;
int	randr_eventbase;

static void
handle(XEvent *e, EvHandler h[], int base) {

	if(h[e->type-base])
		h[e->type-base](e);
}

void
xext_init(void) {
	randr_init();
	render_init();
	xinerama_init();
}

void
xext_event(XEvent *e) {

	if(randr_event_p(e))
		handle(e, randr_handlers, randr_eventbase);
}

static void
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

	XRRUpdateConfiguration((XEvent*)ev);
	if(ev->rotation+90 % 180)
		scr.rect = Rect(0, 0, ev->width, ev->height);
	else
		scr.rect = Rect(0, 0, ev->height, ev->width);
	init_screens();
}

static EvHandler randr_handlers[] = {
	[RRScreenChangeNotify] = (EvHandler)randr_screenchange,
};

/* Ripped most graciously from ecore_x. XRender documentation
 * is sparse.
 */
static void
render_init(void) {
	XVisualInfo *vip;
	XVisualInfo vi;
	int base, i, n;

	have_render = XRenderQueryExtension(display, &base, &base);
	if(!have_render)
		return;

	vi.class = TrueColor;
	vi.depth = 32;
	vi.screen = scr.screen;
	vip = XGetVisualInfo(display, VisualClassMask
				    | VisualDepthMask
				    | VisualScreenMask,
				    &vi, &n);
	for(i=0; i < n; i++)
		if(render_argb_p(vip[i].visual)) {
			render_visual = vip[i].visual;
			break;
		}
	XFree(vip);
}

bool
render_argb_p(Visual *v) {
	XRenderPictFormat *f;

	if(!have_render)
		return false;
	f = XRenderFindVisualFormat(display, v);
	return f
	    && f->type == PictTypeDirect
	    && f->direct.alphaMask;
}

static void
xinerama_init(void) {
	int base;

	have_xinerama = XineramaQueryExtension(display, &base, &base);
}

static bool
xinerama_active(void) {
	return have_xinerama && XineramaIsActive(display);
}

Rectangle*
xinerama_screens(int *np) {
	static Rectangle *rects;
	XineramaScreenInfo *res;
	int i, n;

	if(!xinerama_active()) {
		*np = 1;
		return &scr.rect;
	}

	free(rects);
	res = XineramaQueryScreens(display, &n);
	rects = emalloc(n * sizeof *rects);
	for(i=0; i < n; i++) {
		rects[i].min.x = res[i].x_org;
		rects[i].min.y = res[i].y_org;
		rects[i].max.x = res[i].x_org + res[i].width;
		rects[i].max.y = res[i].y_org + res[i].height;
	}
	XFree(res);

	*np = n;
	return rects;
}

