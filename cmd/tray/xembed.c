/* Copyright ©2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "dat.h"
#include "fns.h"

#define DEAD ~0UL

enum { XEmbedVersion = 0 };

enum XEmbedMessage {
	XEmbedEmbeddedNotify,
	XEmbedWindowActivate,
	XEmbedWindowDeactivate,
	XEmbedRequestFocus,
	XEmbedFocusIn,
	XEmbedFocusOut,
	XEmbedFocusNext,
	XEmbedFocusPrev,
	XEmbedModalityOn = 10,
	XEmbedModalityOff,
	XEmbedRegisterAccelerator,
	XEmbedUnregisterAccelerator,
	XEmbedActivateAccelerator,
};

enum XEmbedFocusDetail {
	XEmbedFocusCurrent,
	XEmbedFocusFirst,
	XEmbedFocusLast,
};

static Handlers	handlers;

static void	xembed_updateinfo(XEmbed*);
static void	xembed_sendmessage(XEmbed*, long, long, long, long);

XEmbed*
xembed_swallow(Window *parent, Window *client, void (*cleanup)(XEmbed*)) {
	XEmbed *xembed;

	xembed = emallocz(sizeof *xembed);
	xembed->w = client;
	xembed->owner = parent;
	xembed->cleanup = cleanup;
	selectinput(client, client->eventmask | PropertyChangeMask | StructureNotifyMask);
	pushhandler(client, &handlers, xembed);

	reparentwindow(client, parent, ZP);
	xembed_updateinfo(xembed);
	xembed_sendmessage(xembed, XEmbedEmbeddedNotify, 0, parent->xid, min(XEmbedVersion, xembed->version));
	return xembed;
}

void
xembed_disown(XEmbed *xembed) {

	pophandler(xembed->w, &handlers);
	if(xembed->flags != DEAD) {
		reparentwindow(xembed->w, &scr.root, ZP);
		unmapwin(xembed->w);
	}
	if(xembed->cleanup)
		xembed->cleanup(xembed);
	free(xembed);
}

static void
xembed_updateinfo(XEmbed *xembed) {
	ulong *res;
	int n;

	n = getprop_ulong(xembed->w, "_XEMBED_INFO", "_XEMBED_INFO", 0, &res, 2);
	xembed->flags = 0UL;
	if(n >= 2) {
		xembed->version = res[0];
		xembed->flags = res[1];
	}
	free(res);

	if(xembed->flags & XEmbedMapped)
		mapwin(xembed->w);
	else
		unmapwin(xembed->w);
}

static void
xembed_sendmessage(XEmbed *xembed, long message, long detail, long data1, long data2) {

	traperrors(true);
	sendmessage(xembed->w, "_XEMBED", event_xtime, message, detail, data1, data2);
	traperrors(false);
}

static bool
destroy_event(Window *w, void *aux, XDestroyWindowEvent *ev) {
	XEmbed *xembed;

	xembed = aux;
	xembed->flags = DEAD;
	xembed_disown(xembed);
	return false;
}

static bool
property_event(Window *w, void *aux, XPropertyEvent *ev) {
	XEmbed *xembed;

	Dprint("property_event(%W, %p, %A)\n",
	       w, aux, ev->atom);
	xembed = aux;
	if(ev->atom == xatom("_XEMBED_INFO"))
		xembed_updateinfo(xembed);
	return false;
}

static bool
reparent_event(Window *w, void *aux, XReparentEvent *ev) {
	XEmbed *xembed;

	xembed = aux;
	if(ev->parent != xembed->owner->xid) {
		xembed->flags = DEAD;
		xembed_disown(xembed);
	}
	return false;
}

static Handlers handlers = {
	.destroy = destroy_event,
	.property = property_event,
	.reparent = reparent_event,
};
