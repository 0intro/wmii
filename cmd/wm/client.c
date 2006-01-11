/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xatom.h>

#include "wm.h"

Client *
alloc_client(Window w)
{
    XTextProperty name;
    Client *c = (Client *) cext_emallocz(sizeof(Client));

    c->win = w;
    XGetWMName(dpy, c->win, &name);
	if(name.value) {
		cext_strlcpy(c->name, (char *)name.value, sizeof(c->name));
    	free(name.value);
	}
    return c;
}

void
focus_client(Client * c)
{
    Frame *f = 0;
	Client *old = sel_client();

	if(old && (old != c)) {
		ungrab_client(old, AnyModifier, AnyButton);
		grab_client(old, AnyModifier, AnyButton);
	}
	
    /* sel client */
	ungrab_client(c, AnyModifier, AnyButton);
    grab_client(c, Mod1Mask, Button1);
    grab_client(c, Mod1Mask, Button3);
    f = c->frame;
    f->sel = c;
    XRaiseWindow(dpy, c->win);
    XSetInputFocus(dpy, c->win, RevertToPointerRoot, CurrentTime);
	XSync(dpy, False);
    invoke_wm_event(def[WM_EVENT_CLIENT_UPDATE]);
}

void
set_client_state(Client * c, int state)
{
    long data[2];

    data[0] = (long) state;
    data[1] = (long) None;
    XChangeProperty(dpy, c->win, wm_state, wm_state, 32, PropModeReplace,
                    (unsigned char *) data, 2);
}

void
show_client(Client * c)
{
    XMapRaised(dpy, c->win);
    set_client_state(c, NormalState);
	grab_client(c, AnyModifier, AnyButton);
}

void
hide_client(Client * c)
{
    ungrab_client(c, AnyModifier, AnyButton);
    XUnmapWindow(dpy, c->win);
    set_client_state(c, WithdrawnState);
}

void
reparent_client(Client * c, Window w, int x, int y)
{
    XReparentWindow(dpy, c->win, w, x, y);
    c->ignore_unmap++;
}

void
grab_client(Client * c, unsigned long mod, unsigned int button)
{
    XGrabButton(dpy, button, mod, c->win, True,
                ButtonPressMask, GrabModeSync, GrabModeAsync, None, None);
    if((mod != AnyModifier) && num_lock_mask) {
        XGrabButton(dpy, button, mod | num_lock_mask, c->win, True,
                    ButtonPressMask, GrabModeSync, GrabModeAsync, None,
                    None);
        XGrabButton(dpy, button, mod | num_lock_mask | LockMask, c->win,
                    True, ButtonPressMask, GrabModeSync, GrabModeAsync,
                    None, None);
    }
}

void
ungrab_client(Client * c, unsigned long mod, unsigned int button)
{
    XUngrabButton(dpy, button, mod, c->win);
    if(mod != AnyModifier && num_lock_mask) {
        XUngrabButton(dpy, button, mod | num_lock_mask, c->win);
        XUngrabButton(dpy, button, mod | num_lock_mask | LockMask, c->win);
    }
}

void
configure_client(Client * c)
{
    XConfigureEvent e;
    e.type = ConfigureNotify;
    e.event = c->win;
    e.window = c->win;
    e.x = c->rect.x;
    e.y = c->rect.y;
    if(c->frame) {
        e.x += c->frame->rect.x;
        e.y += c->frame->rect.y;
    }
    e.width = c->rect.width;
    e.height = c->rect.height;
    e.border_width = c->border;
    e.above = None;
    e.override_redirect = False;

    XSelectInput(dpy, c->win, CLIENT_MASK & ~StructureNotifyMask);
    XSendEvent(dpy, c->win, False, StructureNotifyMask, (XEvent *) & e);
    XSelectInput(dpy, c->win, CLIENT_MASK);
    XSync(dpy, False);
}

void
close_client(Client * c)
{
    if(c->proto & PROTO_DEL)
        wmii_send_message(dpy, c->win, wm_protocols, wm_delete);
    else
        XKillClient(dpy, c->win);
}

void
init_client(Client * c, XWindowAttributes * wa)
{
    long msize;
    c->rect.x = wa->x;
    c->rect.y = wa->y;
    c->border = wa->border_width;
    c->rect.width = wa->width + 2 * c->border;
    c->rect.height = wa->height + 2 * c->border;
    XSetWindowBorderWidth(dpy, c->win, 0);
    c->proto = win_proto(c->win);
    XGetTransientForHint(dpy, c->win, &c->trans);
    if(!XGetWMNormalHints(dpy, c->win, &c->size, &msize) || !c->size.flags)
        c->size.flags = PSize;
    XAddToSaveSet(dpy, c->win);
}

void
handle_client_property(Client * c, XPropertyEvent * e)
{
    XTextProperty name;
    long msize;

    if(e->atom == wm_protocols) {
        /* update */
        c->proto = win_proto(c->win);
        return;
    }
    switch (e->atom) {
    case XA_WM_NAME:
        XGetWMName(dpy, c->win, &name);
        if(name.value) {
			cext_strlcpy(c->name, (char*) name.value, sizeof(c->name));
        	free(name.value);
		}
        if(c->frame)
            draw_client(c);
        invoke_wm_event(def[WM_EVENT_CLIENT_UPDATE]);
        break;
    case XA_WM_TRANSIENT_FOR:
        XGetTransientForHint(dpy, c->win, &c->trans);
        break;
    case XA_WM_NORMAL_HINTS:
        if(!XGetWMNormalHints(dpy, c->win, &c->size, &msize)
           || !c->size.flags) {
            c->size.flags = PSize;
        }
        break;
    }
}

void
destroy_client(Client * c)
{
    detach_detached(c);
    free(c);
}

/* speed reasoned function for client property change */
void
draw_client(Client * client)
{
    Frame *f = client->frame;
    unsigned int i = 0, tw, tabh = tab_height(f);
    Draw d = { 0 };
    Client *c;

    if(!tabh)
        return;

    tw = f->rect.width / f->nclients;
    for(c = f->clients; c && c != client; c = c->next)
        i++;

    d.drawable = f->win;
    d.gc = f->gc;
    d.rect.x = i * tw;
    d.rect.y = 0;
    d.rect.width = tw;
    if(i && (i == f->nclients - 1))
        d.rect.width = f->rect.width - d.rect.x;
    d.rect.height = tabh;
    d.data = c->name;
    d.font = font;

    if((f == sel_frame()) && (c == f->sel)) {
        d.bg =
            blitz_loadcolor(dpy, screen_num,
                            def[WM_SEL_BG_COLOR]->content);
        d.fg =
            blitz_loadcolor(dpy, screen_num,
                            def[WM_SEL_FG_COLOR]->content);
        d.border =
            blitz_loadcolor(dpy, screen_num,
                            def[WM_SEL_BORDER_COLOR]->content);
    } else {
        d.bg =
            blitz_loadcolor(dpy, screen_num,
                            def[WM_NORM_BG_COLOR]->content);
        d.fg =
            blitz_loadcolor(dpy, screen_num,
                            def[WM_NORM_FG_COLOR]->content);
        d.border =
            blitz_loadcolor(dpy, screen_num,
                            def[WM_NORM_BORDER_COLOR]->content);
    }
    blitz_drawlabel(dpy, &d);
    XSync(dpy, False);
}

void
draw_clients(Frame * f)
{
    Client *c;
    for(c = f->clients; c; c = c->next)
        draw_client(c);
}

void
gravitate(Client * c, unsigned int tabh, unsigned int bw, int invert)
{
    int dx = 0, dy = 0;
    int gravity = NorthWestGravity;

    if(c->size.flags & PWinGravity) {
        gravity = c->size.win_gravity;
    }
    /* y */
    switch (gravity) {
    case StaticGravity:
    case NorthWestGravity:
    case NorthGravity:
    case NorthEastGravity:
        dy = tabh;
        break;
    case EastGravity:
    case CenterGravity:
    case WestGravity:
        dy = -(c->rect.height / 2) + tabh;
        break;
    case SouthEastGravity:
    case SouthGravity:
    case SouthWestGravity:
        dy = -c->rect.height;
        break;
    default:                   /* don't care */
        break;
    }

    /* x */
    switch (gravity) {
    case StaticGravity:
    case NorthWestGravity:
    case WestGravity:
    case SouthWestGravity:
        dx = bw;
        break;
    case NorthGravity:
    case CenterGravity:
    case SouthGravity:
        dx = -(c->rect.width / 2) + bw;
        break;
    case NorthEastGravity:
    case EastGravity:
    case SouthEastGravity:
        dx = -(c->rect.width + bw);
        break;
    default:                   /* don't care */
        break;
    }

    if(invert) {
        dx = -dx;
        dy = -dy;
    }
    c->rect.x += dx;
    c->rect.y += dy;
}

void
attach_client(Client * c)
{
    Area *a = 0;
    Page *p = selpage;

    if(!p)
        p = alloc_page();
    /* transient stuff */
    a = p->sel;
    if(c && c->trans) {
        Client *t = win_to_client(c->trans);
        if(t && t->frame)
            a = p->floating;
    }
    a->layout->attach(a, c);
    invoke_wm_event(def[WM_EVENT_PAGE_UPDATE]);
}

void
detach_client(Client * c, Bool unmap)
{
    Frame *f = c->frame;
    Area *a = f ? f->area : nil;
    if(a) {
        a->layout->detach(a, c, unmap);
    }
    if(c->destroyed)
        destroy_client(c);
    if(selpage)
        focus_page(selpage);
}

Client *
sel_client()
{
    Frame *f = sel_frame();
    return f ? f->sel : nil;
}

Client *
clientat(Client * clients, size_t idx)
{
    size_t i = 0;
    Client *c = clients;
    for(; c && i != idx; c = c->next)
        i++;
    return c;
}

void
detach_detached(Client * c)
{
    if(detached == c) {
        if(c->next)
            c->next->prev = nil;
        detached = c->next;
    } else {
        if(c->next)
            c->next->prev = c->prev;
        if(c->prev)
            c->prev->next = c->next;
    }
    ndetached--;
}

void
attach_detached(Client * c)
{
    c->prev = nil;
    c->next = detached;
    if(detached)
        detached->prev = c;
    detached = c;
}
