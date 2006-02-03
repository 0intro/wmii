/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <X11/keysym.h>

#include "wm.h"

/* local functions */
static void handle_buttonpress(XEvent * e);
static void handle_configurerequest(XEvent * e);
static void handle_destroynotify(XEvent * e);
static void handle_expose(XEvent * e);
static void handle_maprequest(XEvent * e);
static void handle_motionnotify(XEvent * e);
static void handle_propertynotify(XEvent * e);
static void handle_unmapnotify(XEvent * e);
static void handle_clientmessage(XEvent * e);

void (*handler[LASTEvent]) (XEvent *);

void
init_x_event_handler()
{
    int i;
    /* init event handler */
    for(i = 0; i < LASTEvent; i++) {
        handler[i] = 0;
    }
    handler[ButtonPress] = handle_buttonpress;
    handler[ConfigureRequest] = handle_configurerequest;
    handler[DestroyNotify] = handle_destroynotify;
    handler[Expose] = handle_expose;
    handler[MapRequest] = handle_maprequest;
    handler[MotionNotify] = handle_motionnotify;
    handler[PropertyNotify] = handle_propertynotify;
    handler[UnmapNotify] = handle_unmapnotify;
    handler[ClientMessage] = handle_clientmessage;
}

void
check_x_event(IXPConn *c)
{
    XEvent ev;
    while(XPending(dpy)) {      /* main evet loop */
        XNextEvent(dpy, &ev);
        if(handler[ev.type])
            (handler[ev.type]) (&ev);   /* call handler */
    }
}

static void
handle_buttonpress(XEvent * e)
{
    Client *c;
    XButtonPressedEvent *ev = &e->xbutton;
    Align align;
	static char buf[32];
	if((c = win_to_frame(ev->window))) {
		focus_client(c);
		if(ev->button == Button1) {
			align = cursor_to_align(c->frame.cursor);
			if(align == CENTER)
				mouse_move(c);
			else 
				mouse_resize(c, align);
		}
	}
	else if((c = win_to_client(ev->window))) {
		focus_client(c);
		ev->state &= valid_mask;
		if(ev->state & Mod1Mask) {
			XRaiseWindow(dpy, c->frame.win);
			switch (ev->button) {
				case Button1:
					mouse_move(c);
					break;
				case Button3:
					align = xy_to_align(&c->rect, ev->x, ev->y);
					if(align == CENTER)
						mouse_move(c);
					else
						mouse_resize(c, align);
					break;
			}
		}
	}
	if(c) {
		snprintf(buf, sizeof(buf), "Button%dPress\n", ev->button);
		do_pend_fcall(buf);
	}
}

static void
handle_configurerequest(XEvent * e)
{
    XConfigureRequestEvent *ev = &e->xconfigurerequest;
    XWindowChanges wc;
    Client *c;
    unsigned int bw = 0, bh = 0;

    c = win_to_client(ev->window);
    ev->value_mask &= ~CWSibling;

    if(c) {
        if(c->page) {
            bw = c->frame.border;
            bh = bar_height(c);
        }
        if(ev->value_mask & CWStackMode) {
            if(wc.stack_mode == Above)
                XRaiseWindow(dpy, c->win);
            else
                ev->value_mask &= ~CWStackMode;
        }
        gravitate(c, bh ? bh : bw, bw, 1);

        if(ev->value_mask & CWX)
            c->rect.x = ev->x;
        if(ev->value_mask & CWY)
            c->rect.y = ev->y;
        if(ev->value_mask & CWWidth)
            c->rect.width = ev->width;
        if(ev->value_mask & CWHeight)
            c->rect.height = ev->height;
        if(ev->value_mask & CWBorderWidth)
            c->border = ev->border_width;

        gravitate(c, bh ? bh : bw, bw, 0);

        if(c->page) {
            c->frame.rect.x = wc.x = c->rect.x - bw;
            c->frame.rect.y = wc.y = c->rect.y - (bh ? bh : bw);
            c->frame.rect.width = wc.width = c->rect.width + 2 * bw;
            c->frame.rect.height = wc.height = c->rect.height + bw + (bh ? bh : bw);
            wc.border_width = 1;
            wc.sibling = None;
            wc.stack_mode = ev->detail;
            XConfigureWindow(dpy, c->frame.win, ev->value_mask, &wc);
            configure_client(c);
        }
    }
    wc.x = ev->x;
    wc.y = ev->y;

    if(c && c->page) {
        /* if so, then bw and tabh are already initialized */
        wc.x = bw;
        wc.y = bh ? bh : bw;
    }
    wc.width = ev->width;
    if(!wc.width)
        wc.width = 1;           /* borken app fix */
    wc.height = ev->height;
    if(!wc.height)
        wc.height = 1;          /* borken app fix */
    wc.border_width = 0;
    wc.sibling = None;
    wc.stack_mode = Above;
    ev->value_mask &= ~CWStackMode;
    ev->value_mask |= CWBorderWidth;
    XConfigureWindow(dpy, e->xconfigurerequest.window, ev->value_mask, &wc);
    XSync(dpy, False);

}

static void
handle_destroynotify(XEvent * e)
{
    XDestroyWindowEvent *ev = &e->xdestroywindow;
    Client *c = win_to_client(ev->window);
    if(c) {
        c->destroyed = True;
        detach_client(c, False);
    }
}

static void
handle_expose(XEvent * e)
{
    static Client *c;
    if(e->xexpose.count == 0) {
        if((c = win_to_frame(e->xbutton.window)))
            draw_client(c);
    }
}

static void
handle_maprequest(XEvent * e)
{
    XMapRequestEvent *ev = &e->xmaprequest;
    static XWindowAttributes wa;
    static Client *c;

    if(!XGetWindowAttributes(dpy, ev->window, &wa))
        return;
    if(wa.override_redirect) {
        XSelectInput(dpy, ev->window,
                     (StructureNotifyMask | PropertyChangeMask));
        return;
    }

	/* attach heuristic support */
	if(aqsz && aq[0]) {
		focus_page(0);
		cext_array_detach((void **)aq, aq[0], &aqsz);
	}

    /* there're client which send map requests twice */
    c = win_to_client(ev->window);
    if(!c)
        c = alloc_client(ev->window, &wa);
    if(!c->page)
        attach_client(c);
}

static void
handle_motionnotify(XEvent * e)
{
    Client *c = win_to_frame(e->xmotion.window);
    if(c) {
    	Cursor cursor = cursor_for_motion(c, e->xmotion.x, e->xmotion.y);
        if(cursor != c->frame.cursor) {
            c->frame.cursor = cursor;
            XDefineCursor(dpy, c->frame.win, cursor);
        }
    }
}

static void
handle_propertynotify(XEvent * e)
{
    XPropertyEvent *ev = &e->xproperty;
    Client *c;

    if(ev->state == PropertyDelete)
        return;                 /* ignore */

    if((c = win_to_client(ev->window)))
        handle_client_property(c, ev);
}

static void
handle_unmapnotify(XEvent * e)
{
    XUnmapEvent *ev = &e->xunmap;
    Client *c;
    if((c = win_to_client(ev->window))) {
        if(!c->ignore_unmap)
            detach_client(c, True);
        else
            c->ignore_unmap--;
    }
}

static void handle_clientmessage(XEvent *e)
{
    XClientMessageEvent *ev = &e->xclient;

    if (ev->message_type == net_atoms[NET_NUMBER_OF_DESKTOPS] && ev->format == 32)
        return; /* ignore */
    else if (ev->message_type == net_atoms[NET_CURRENT_DESKTOP] && ev->format == 32) {
		focus_page(page[ev->data.l[0]]);
        return;
    }
}

