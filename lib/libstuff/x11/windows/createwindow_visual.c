/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "../x11.h"
#include <string.h>
#include <unistd.h>

static char hostname[HOST_NAME_MAX + 1];

Window*
createwindow_visual(Window *parent, Rectangle r,
		    int depth, Visual *vis, uint class,
		    WinAttr *wa, int valmask) {
	Window *w;
	WinAttr wa_empty;

	assert(parent->type == WWindow);

	if(wa == nil)
		wa = &wa_empty;

	w = emallocz(sizeof *w);
	w->visual = vis;
	w->type = WWindow;
	w->parent = parent;
	if(valmask & CWColormap)
		w->colormap = wa->colormap;
	if(valmask & CWEventMask)
		w->eventmask = wa->event_mask;

	w->xid = XCreateWindow(display, parent->xid, r.min.x, r.min.y, Dx(r), Dy(r),
				0 /* border */, depth, class, vis, valmask, wa);
#if 0
	print("createwindow_visual(%W, %R, %d, %p, %ud, %p, %x) = %W\n",
			parent, r, depth, vis, class, wa, valmask, w);
#endif
	if(class != InputOnly)
		w->gc = XCreateGC(display, w->xid, 0, nil);

	changeprop_ulong(w, "_NET_WM_PID", "CARDINAL", (ulong[1]){ getpid() }, 1);
	if(!hostname[0])
		gethostname(hostname, sizeof(hostname) - 1);
	if(hostname[0])
		changeprop_char(w, "WM_CLIENT_MACHINE", "STRING", hostname, strlen(hostname));

	w->r = r;
	w->depth = depth;
	return w;
}
