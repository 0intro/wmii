/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "wm.h"
#include "layoutdef.h"

static void init_float(Layout *l, Client * clients);
static Client *deinit_float(Layout *l);
static void arrange_float(Layout *l);
static Bool attach_float(Layout *l, Client * c);
static void detach_float(Layout *l, Client * c, Bool unmap);
static void resize_float(Frame * f, XRectangle * new, XPoint * pt);
static void focus_float(Layout * l, Client * c, Bool raise);
static Frame *frames_float(Layout *l);
static Client *sel_float(Layout *l);
static Action *actions_float(Layout *l);

static void select_frame(void *obj, char *arg);

Action lfloat_acttbl[] = {
    {"select", select_frame},
    {0, 0}
};

typedef struct {
    Frame *frames;
    Frame *sel;
    size_t nframes;
} Float;

void
init_layout_float()
{
    LayoutDef *lp, *l = cext_emallocz(sizeof(LayoutDef));
    l->name = "float";
    l->init = init_float;
    l->deinit = deinit_float;
    l->arrange = arrange_float;
    l->attach = attach_float;
    l->detach = detach_float;
    l->resize = resize_float;
    l->focus = focus_float;
    l->frames = frames_float;
    l->sel = sel_float;
    l->actions = actions_float;

    for(lp = layouts; lp && lp->next; lp = lp->next);
    if(lp)
        lp->next = l;
    else
        layouts = l;
}

static void
arrange_float(Layout *l)
{
}

static void
init_float(Layout *l, Client * clients)
{
    Client *n, *c = clients;
    Float *fl = cext_emallocz(sizeof(Float));
    l->aux = fl;

    while(c) {
        n = c->next;
        attach_float(l, c);
        c = n;
    }
}

static void
attach_frame(Layout *l, Frame * new)
{
    Float *fl = l->aux;
    Frame *f;

    for(f = fl->frames; f && f->next; f = f->next);
    if(!f)
        fl->frames = new;
    else {
        f->next = new;
        new->prev = f;
    }
    attach_frame_to_layout(l, new);
    fl->nframes++;
}

static void
detach_frame(Layout *l, Frame * old)
{
    Float *fl = l->aux;

    if(old->prev)
        old->prev->next = old->next;
    else
        fl->frames = old->next;
    if(old->next)
        old->next->prev = old->prev;

    if(fl->sel == old)
        fl->sel = fl->frames;

	old->prev = old->next = nil;
    detach_frame_from_layout(old);
    fl->nframes--;
}

static Client *
deinit_float(Layout *l)
{
    Float *fl = l->aux;
    Client *res = nil, *c = nil, *cl;
    Frame *f;

    while((f = fl->frames)) {
        while((cl = f->clients)) {
            detach_client_from_frame(cl, False);
            cl->prev = cl->next = 0;
            if(!res)
                res = c = cl;
            else {
                c->next = cl;
                cl->prev = c;
                c = cl;
            }
        }
        detach_frame(l, f);
        destroy_frame(f);
    }
    free(fl);
    l->aux = nil;
    return res;
}

static Bool
attach_float(Layout *l, Client * c)
{
    Frame *f;

    /* check for tabbing? */
    if(c->rect.y < layout_rect.y)
		c->rect.y = layout_rect.y;
	if(c->rect.x < layout_rect.x)
		c->rect.x = layout_rect.x;

	f = alloc_frame(&c->rect);
	attach_frame_to_layout(l, f);
	attach_frame(l, f);
    attach_client_to_frame(f, c);
    if(l->page == selpage)
        XMapWindow(dpy, f->win);
    focus_float(l, c, True);
    return True;
}

static void
detach_float(Layout *l, Client * c, Bool unmap)
{
    Frame *f = c->frame;
    detach_client_from_frame(c, unmap);
    if(!f->clients) {
        detach_frame(l, f);
        destroy_frame(f);
    }
}

static void
resize_float(Frame * f, XRectangle * new, XPoint * pt)
{
    f->rect = *new;
}

static void
focus_float(Layout *l, Client *c, Bool raise)
{
    Float *fl = l->aux;
	Client *old = sel_client();

    c->frame->sel = c;
    fl->sel = c->frame;
    l->file[L_SEL_FRAME]->content = c->frame->file[F_PREFIX]->content;

    if(raise) {
        XRaiseWindow(dpy, c->frame->win);
    	XWarpPointer(dpy, None, c->win, 0, 0, 0, 0,
					 c->rect.width / 2, c->rect.height / 2);
    }

	focus_client(c, old);
}

static Frame *
frames_float(Layout *l)
{
    Float *fl = l->aux;
    return fl->frames;
}

static void
select_frame(void *obj, char *arg)
{
    Layout *l = obj;
    Float *fl = l->aux;
    Frame *f = fl->sel;

    if(!f || !arg)
        return;
    if(!strncmp(arg, "prev", 5)) {
        if(f->prev)
            f = f->prev;
        else
            for(f = fl->frames; f && f->next; f = f->next);
    } else if(!strncmp(arg, "next", 5)) {
        if(f->next)
            f = f->next;
        else
            f = fl->frames;
    } else {
        unsigned int i = 0, idx = blitz_strtonum(arg, 0, fl->nframes - 1);
        for(f = fl->frames; f && i != idx; f = f->next)
            i++;
    }
    if(f)
        focus_float(l, f->sel, True);
}

static Client *
sel_float(Layout *l)
{
    Float *fl = l->aux;
    return fl->sel ? fl->sel->sel : nil;
}

static Action *
actions_float(Layout *l)
{
    return lfloat_acttbl;
}
