/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "wm.h"
#include "layoutdef.h"

typedef struct Acme Acme;
typedef struct Column Column;
typedef struct Cell Cell;

struct Acme {
    Column *sel;
    Column *columns;
    size_t ncolumns;
    Frame *frames;
    size_t nframes;
};

struct Cell {
    Frame *frame;
    Cell *next;
    Cell *prev;
    Column *col;
};

struct Column {
    Cell *sel;
    Cell *cells;
    size_t ncells;
    XRectangle rect;
    Column *prev;
    Column *next;
};

static void init_col(Layout *l, Client * clients);
static Client *deinit_col(Layout *l);
static void arrange_col(Layout *l);
static Bool attach_col(Layout *l, Client * c);
static void detach_col(Layout *l, Client * c, Bool unmap);
static void resize_col(Frame * f, XRectangle * new, XPoint * pt);
static void focus_col(Layout *l, Client *c, Bool raise);
static Frame *frames_col(Layout *l);
static Client *sel_col(Layout *l);
static Action *actions_col(Layout *l);

static void select_frame(void *obj, char *arg);
static void swap_frame(void *obj, char *arg);
static void new_col(void *obj, char *arg);

static Action lcol_acttbl[] = {
    {"select", select_frame},
    {"swap", swap_frame},
    {"new", new_col},
    {0, 0}
};

void
init_layout_column()
{
    LayoutDef *lp, *l = cext_emallocz(sizeof(LayoutDef));
    l->name = "column";
    l->init = init_col;
    l->deinit = deinit_col;
    l->arrange = arrange_col;
    l->attach = attach_col;
    l->detach = detach_col;
    l->resize = resize_col;
    l->focus = focus_col;
    l->frames = frames_col;
    l->sel = sel_col;
    l->actions = actions_col;

    for(lp = layouts; lp && lp->next; lp = lp->next);
    if(lp)
        lp->next = l;
    else
        layouts = l;
}

static void
arrange_column(Column * col)
{
    Cell *cell;
    unsigned int i = 0, h = layout_rect.height / col->ncells;
    for(cell = col->cells; cell; cell = cell->next) {
        Frame *f = cell->frame;
        f->rect = col->rect;
        f->rect.y = layout_rect.y + i * h;
        if(!cell->next)
            f->rect.height = layout_rect.height - f->rect.y + layout_rect.y;
        else
            f->rect.height = h;
        resize_frame(f, &f->rect, 0);
        i++;
    }
}

static void
arrange_col(Layout *l)
{
    Acme *acme = l->aux;
    Column *col;
    for(col = acme->columns; col; col = col->next)
        arrange_column(col);
    XSync(dpy, False);
}

static void
init_col(Layout *l, Client * clients)
{
    Client *n, *c = clients;

    l->aux = cext_emallocz(sizeof(Acme));
    while(c) {
        n = c->next;
        attach_col(l, c);
        c = n;
    }
}

static void
attach_frame(Layout *l, Column * col, Frame * new)
{
    Acme *acme = l->aux;
    Cell *c, *cell = cext_emallocz(sizeof(Cell));
    Frame *f;

    cell->frame = new;
    cell->col = col;
    new->aux = cell;
    for(c = col->cells; c && c->next; c = c->next);
    if(!c)
        col->cells = cell;
    else {
        c->next = cell;
        cell->prev = c;
    }
    col->sel = cell;
    col->ncells++;

    for(f = acme->frames; f && f->next; f = f->next);
    if(!f)
        acme->frames = new;
    else {
        f->next = new;
        new->prev = f;
    }
    attach_frame_to_layout(l, new);
    acme->nframes++;
}

static void
detach_frame(Layout *l, Frame * old)
{
    Acme *acme = l->aux;
    Cell *cell = old->aux;
    Column *col = cell->col;

    if(col->sel == cell) {
        if(cell->prev)
            col->sel = cell->prev;
        else
            col->sel = nil;
    }
    if(cell->prev)
        cell->prev->next = cell->next;
    else
        col->cells = cell->next;
    if(cell->next)
        cell->next->prev = cell->prev;
    if(!col->sel)
        col->sel = col->cells;
    free(cell);
    col->ncells--;

    if(old->prev)
        old->prev->next = old->next;
    else
        acme->frames = old->next;
    if(old->next)
        old->next->prev = old->prev;
    old->aux = nil;
    detach_frame_from_layout(old);
    acme->nframes--;
}

static Client *
deinit_col(Layout *l)
{
    Acme *acme = l->aux;
    Frame *f;
    Client *cl, *res = nil, *c = nil;
    Column *col = acme->columns;

    while((f = acme->frames)) {
        while((cl = f->clients)) {
            detach_client_from_frame(cl, False);
            cl->prev = cl->next = 0;
            if(!c)
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
    while((col = acme->columns)) {
        acme->columns = col->next;
        free(col);
    }
    free(acme);
    l->aux = 0;
    return res;
}

static Bool
attach_col(Layout *l, Client * c)
{
    Acme *acme = l->aux;
    Column *col = acme->sel;
    Frame *f = nil;

    if(!col) {
        col = cext_emallocz(sizeof(Column));
        col->rect = layout_rect;
        acme->columns = acme->sel = col;
        acme->ncolumns++;
    }

    f = alloc_frame(&c->rect);
    attach_frame(l, col, f);
    attach_client_to_frame(f, c);
    arrange_column(col);
    if(l->page == selpage)
        XMapWindow(dpy, f->win);
    focus_col(l, c, True);
    return True;
}

static void
update_column_width(Layout *l)
{
    Acme *acme = l->aux;
    unsigned int i = 0, width;
    Column *col;

    if(!acme->ncolumns)
        return;

    width = layout_rect.width / acme->ncolumns;
    for(col = acme->columns; col; col = col->next) {
        col->rect = layout_rect;
        col->rect.x = i * width;
        col->rect.width = width;
        i++;
    }
    arrange_col(l);
}

static void
detach_col(Layout *l, Client * c, Bool unmap)
{
    Acme *acme = l->aux;
    Frame *f = c->frame;
    Cell *cell = f->aux;
    Column *col = cell->col;

    detach_client_from_frame(c, unmap);
    if(!f->clients) {
        detach_frame(l, f);
        destroy_frame(f);
    } else
        return;
    if(col->cells)
        arrange_column(col);
    else {
        if(acme->sel == col) {
            if(col->prev)
                acme->sel = col->prev;
            else
                acme->sel = nil;
        }
        if(col->prev)
            col->prev->next = col->next;
        else
            acme->columns = col->next;
        if(col->next)
            col->next->prev = col->prev;
        if(!acme->sel)
            acme->sel = acme->columns;
        acme->ncolumns--;
        free(col);
        update_column_width(l);
    }
}

static void
match_frame_horiz(Column * col, XRectangle * r)
{
    Cell *cell;
    for(cell = col->cells; cell; cell = cell->next) {
        Frame *f = cell->frame;
        f->rect.x = r->x;
        f->rect.width = r->width;
        resize_frame(f, &f->rect, nil);
    }
}

static void
drop_resize(Frame * f, XRectangle * new)
{
    Column *west = nil, *east = nil, *col = nil;
    Cell *north = nil, *south = nil;
    Cell *cell = f->aux;

    col = cell->col;
    west = col->prev;
    east = col->next;
    north = cell->prev;
    south = cell->next;

    /* horizontal resize */
    if(west && (new->x != f->rect.x)) {
        west->rect.width = new->x - west->rect.x;
        col->rect.width += f->rect.x - new->x;
        col->rect.x = new->x;
        match_frame_horiz(west, &west->rect);
        match_frame_horiz(col, &col->rect);
    }
    if(east && (new->x + new->width != f->rect.x + f->rect.width)) {
        east->rect.width -= new->x + new->width - east->rect.x;
        east->rect.x = new->x + new->width;
        col->rect.x = new->x;
        col->rect.width = new->width;
        match_frame_horiz(col, &col->rect);
        match_frame_horiz(east, &east->rect);
    }

    /* vertical resize */
    if(north && (new->y != f->rect.y)) {
        north->frame->rect.height = new->y - north->frame->rect.y;
        f->rect.height += f->rect.y - new->y;
        f->rect.y = new->y;
        resize_frame(north->frame, &north->frame->rect, nil);
        resize_frame(f, &f->rect, nil);
    }
    if(south && (new->y + new->height != f->rect.y + f->rect.height)) {
        south->frame->rect.height -=
            new->y + new->height - south->frame->rect.y;
        south->frame->rect.y = new->y + new->height;
        f->rect.y = new->y;
        f->rect.height = new->height;
        resize_frame(f, &f->rect, nil);
        resize_frame(south->frame, &south->frame->rect, nil);
    }
}

static void
drop_moving(Frame * f, XRectangle * new, XPoint * pt)
{
    Layout *l = f->layout;
    Cell *cell = f->aux;
    Column *src = cell->col, *tgt = 0;
    Acme *acme = l->aux;

    if(!pt)
        return;

    for(tgt = acme->columns;
        tgt && !blitz_ispointinrect(pt->x, pt->y, &tgt->rect);
        tgt = tgt->next);
    if(tgt) {
        if(tgt != src) {
            if(src->ncells < 2)
                return;
            detach_frame(l, f);
            attach_frame(l, tgt, f);
            arrange_column(src);
            arrange_column(tgt);
            focus_col(l, f->sel, True);
        } else {
            Cell *c;
            for(c = src->cells;
                c && !blitz_ispointinrect(pt->x, pt->y, &c->frame->rect);
                c = c->next);
            if(c && c != cell) {
                Frame *tmp = c->frame;
                c->frame = f;
                cell->frame = tmp;
                arrange_column(src);
                focus_col(l, f->sel, True);
            }
        }
    }
}

static void
resize_col(Frame * f, XRectangle * new, XPoint * pt)
{
    if((f->rect.width == new->width)
       && (f->rect.height == new->height))
        drop_moving(f, new, pt);
    else
        drop_resize(f, new);
}

static void
focus_col(Layout *l, Client *c, Bool raise)
{
    Acme *acme = l->aux;
	Client *old = sel_col(l);
	Cell *cell = c->frame->aux;

	if(old != c)
		unfocus_client(old);
    acme->sel = cell->col;
	cell->col->sel = cell;
    c->frame->file[L_SEL_FRAME]->content = c->frame->file[F_PREFIX]->content;
    if(raise)
    	XWarpPointer(dpy, None, c->win, 0, 0, 0, 0,
					 c->rect.width / 2, c->rect.height / 2);
    focus_client(c);
	if(old && old != c)
    	draw_frame(old->frame);
    draw_frame(c->frame);
}

static Frame *
frames_col(Layout *l)
{
    Acme *acme = l->aux;
    return acme->frames;
}

static Client *
sel_col(Layout *l)
{
    Acme *acme = l->aux;
    Column *col = acme->sel;

    if(col && col->sel)
        return col->sel->frame->sel;
    return nil;
}

static Action *
actions_col(Layout *l)
{
    return lcol_acttbl;
}

static void
select_frame(void *obj, char *arg)
{
    Layout *l = obj;
    Acme *acme = l->aux;
    Column *c, *col = acme->sel;
    Cell *cell;

    if(!col)
        return;

    cell = col->sel;
    if(!cell || !arg)
        return;
    if(!strncmp(arg, "prev", 5)) {
        if(cell->prev)
            cell = cell->prev;
        else
            for(cell = col->cells; cell && cell->next; cell = cell->next);
    } else if(!strncmp(arg, "next", 5)) {
        if(cell->next)
            cell = cell->next;
        else
            cell = col->cells;
    } else if(!strncmp(arg, "west", 5)) {
        if(col->prev)
            cell = col->prev->sel;
        else
            for(c = acme->columns; c && c->next; c = c->next);
    } else if(!strncmp(arg, "east", 5)) {
        if(col->next)
            cell = col->next->sel;
        else
            cell = acme->columns->sel;
    } else {
        unsigned int i = 0, idx = blitz_strtonum(arg, 0, col->ncells - 1);
        for(cell = col->cells; cell && i != idx; cell = cell->next)
            i++;
    }
    if(cell)
        focus_col(l, cell->frame->sel, True);
}

static void
swap_frame(void *obj, char *arg)
{
    Layout *l = obj;
    Acme *acme = l->aux;
    Column *west = nil, *east = nil, *col = acme->sel;
    Cell *north = nil, *south = nil, *cell = col->sel;
    Frame *f;
    XRectangle r;

    if(!col || !cell || !arg)
        return;

    west = col->prev;
    east = col->next;
    north = cell->prev;
    south = cell->next;

    if(!strncmp(arg, "north", 6) && north) {
        r = north->frame->rect;
        north->frame->rect = cell->frame->rect;
        cell->frame->rect = r;
        f = north->frame;
        north->frame = cell->frame;
        cell->frame = f;
        cell->frame->aux = cell;
        north->frame->aux = north;
        resize_frame(cell->frame, &cell->frame->rect, nil);
        resize_frame(north->frame, &north->frame->rect, nil);
        focus_col(l, cell->frame->sel, True);
    } else if(!strncmp(arg, "south", 6) && south) {
        r = south->frame->rect;
        south->frame->rect = cell->frame->rect;
        cell->frame->rect = r;
        f = south->frame;
        south->frame = cell->frame;
        cell->frame = f;
        cell->frame->aux = cell;
        south->frame->aux = south;
        resize_frame(cell->frame, &cell->frame->rect, nil);
        resize_frame(south->frame, &south->frame->rect, nil);
        focus_col(l, cell->frame->sel, True);
    } else if(!strncmp(arg, "west", 5) && west && (col->ncells > 1)) {
        Cell *other = west->sel;
        r = other->frame->rect;
        other->frame->rect = cell->frame->rect;
        cell->frame->rect = r;
        f = other->frame;
        other->frame = cell->frame;
        cell->frame = f;
        other->frame->aux = other;
        cell->frame->aux = cell;
        resize_frame(cell->frame, &cell->frame->rect, nil);
        resize_frame(other->frame, &other->frame->rect, nil);
        focus_col(l, other->frame->sel, True);
    } else if(!strncmp(arg, "east", 5) && east && (col->ncells > 1)) {
        Cell *other = east->sel;
        r = other->frame->rect;
        other->frame->rect = cell->frame->rect;
        cell->frame->rect = r;
        f = other->frame;
        other->frame = cell->frame;
        cell->frame = f;
        other->frame->aux = other;
        cell->frame->aux = cell;
        resize_frame(cell->frame, &cell->frame->rect, nil);
        resize_frame(other->frame, &other->frame->rect, nil);
        focus_col(l, other->frame->sel, True);
    }
}

static void
new_col(void *obj, char *arg)
{
    Layout *l = obj;
    Acme *acme = l->aux;
    Column *c, *new, *col = acme->sel;
    Frame *f;

    if(!col || col->ncells < 2)
        return;

    f = col->sel->frame;
    for(c = acme->columns; c && c->next; c = c->next);

    new = cext_emallocz(sizeof(Column));
    new->rect = layout_rect;
    new->prev = c;
    c->next = new;
    acme->ncolumns++;
    acme->sel = new;

    detach_frame(l, f);
    attach_frame(l, new, f);

    update_column_width(l);
    focus_col(l, f->sel, True);
}
