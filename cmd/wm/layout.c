/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "wm.h"

static void handle_after_write_layout(IXPServer * s, File * file);

Layout *
alloc_layout(Page * p, char *layout)
{
    char buf[MAX_BUF], *name;
    Layout *l = (Layout *) cext_emallocz(sizeof(Layout));

    l->page = p;
    if(strncmp(layout, "float", 6))
        name = "managed";
    else
        name = "float";
    snprintf(buf, MAX_BUF, "/%s/layout/%s", p->file[P_PREFIX]->name, name);
    l->file[L_PREFIX] = ixp_create(ixps, buf);
    snprintf(buf, MAX_BUF, "/%s/layout/%s/frame", p->file[P_PREFIX]->name,
             name);
    l->file[L_FRAME_PREFIX] = ixp_create(ixps, buf);
    snprintf(buf, MAX_BUF, "/%s/layout/%s/frame/sel",
             p->file[P_PREFIX]->name, name);
    l->file[L_SEL_FRAME] = ixp_create(ixps, buf);
    l->file[L_SEL_FRAME]->bind = 1;
    snprintf(buf, MAX_BUF, "/%s/layout/%s/ctl", p->file[P_PREFIX]->name,
             name);
    l->file[L_CTL] = ixp_create(ixps, buf);
    l->file[L_CTL]->after_write = handle_after_write_layout;
    snprintf(buf, MAX_BUF, "/%s/layout/%s/name", p->file[P_PREFIX]->name,
             name);
    l->file[L_NAME] = wmii_create_ixpfile(ixps, buf, layout);
    l->file[L_NAME]->after_write = handle_after_write_layout;
    l->def = match_layout_def(layout);
    l->def->init(l, nil);
    p->file[P_SEL_LAYOUT]->content = l->file[L_PREFIX]->content;
    return l;
}

void
destroy_layout(Layout *l)
{
    l->def->deinit(l);
    ixp_remove_file(ixps, l->file[L_PREFIX]);
    free(l);
}

void
focus_layout(Layout *l)
{
    Page *p = l->page;
    Client *c;
    p->sel = l;
    p->file[P_SEL_LAYOUT]->content = l->file[L_PREFIX]->content;
    if((c = l->def->sel(l)))
        l->def->focus(l, c, False);
}

void
hide_layout(Layout *l)
{
    Frame *f;
    for(f = l->def->frames(l); f; f = f->next)
        XMoveWindow(dpy, f->win, 2 * rect.width, 2 * rect.height);
}

void
show_layout(Layout *l, Bool raise)
{
    Frame *f;
    for(f = l->def->frames(l); f; f = f->next) {
        XMoveWindow(dpy, f->win, f->rect.x, f->rect.y);
        if(raise)
            XRaiseWindow(dpy, f->win);
    }
}

Layout *
sel_layout()
{
    return selpage ? selpage->sel : nil;
}

void
attach_frame_to_layout(Layout *l, Frame * f)
{
    wmii_move_ixpfile(f->file[F_PREFIX], l->file[L_FRAME_PREFIX]);
    l->file[L_SEL_FRAME]->content = f->file[F_PREFIX]->content;
    f->layout = l;
}

void
detach_frame_from_layout(Frame * f)
{
    f->layout->file[L_SEL_FRAME]->content = 0;
    f->layout = 0;
}

static void
handle_after_write_layout(IXPServer * s, File * file)
{
    Page *p;
    for(p = pages; p; p = p->next) {
        if(file == p->managed->file[L_CTL]) {
            run_action(file, p->managed,
                       p->managed->def->actions(p->managed));
            return;
        } else if(file == p->floating->file[L_CTL]) {
            run_action(file, p->floating,
                       p->floating->def->actions(p->floating));
            return;
        } else if(file == p->managed->file[L_NAME]) {
            LayoutDef *l = match_layout_def(file->content);
            if(l) {
                Client *clients = p->managed->def->deinit(p->managed);
                p->managed->def = l;
                p->managed->def->init(p->managed, clients);
                invoke_wm_event(def[WM_EVENT_PAGE_UPDATE]);
            }
        }
    }
}

LayoutDef *
match_layout_def(char *name)
{
    LayoutDef *l;
    for(l = layouts; l; l = l->next)
        if(!strncmp(name, l->name, strlen(l->name)))
            return l;
    return nil;
}
