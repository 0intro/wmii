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
    Column *column;
};

struct Column {
    Cell *sel;
    Cell *cells;
    size_t ncells;
    XRectangle rect;
    Column *prev;
    Column *next;
};

static void init_column(Layout *l, Client * clients);
static Client *deinit_column(Layout *l);
static void arrange_column(Layout *l);
static Bool attach_column(Layout *l, Client * c);
static void detach_column(Layout *l, Client * c, Bool unmap);
static void resize_column(Frame * f, XRectangle * new, XPoint * pt);
static void focus_column(Layout *l, Client *c, Bool raise);
static Frame *frames_column(Layout *l);
static Client *sel_column(Layout *l);
static Action *actions_column(Layout *l);

static void select_frame(void *obj, char *arg);
static void max_frame(void *obj, char *arg);
static void swap_frame(void *obj, char *arg);
static void new_column(void *obj, char *arg);

static Action lcol_acttbl[] = {
    {"select", select_frame},
    {"swap", swap_frame},
    {"new", new_column},
    {"max", max_frame},
    {0, 0}
};

void
init_layout_column()
{
    LayoutDef *lp, *l = cext_emallocz(sizeof(LayoutDef));
    l->name = "column";
    l->init = init_column;
    l->deinit = deinit_column;
    l->arrange = arrange_column;
    l->attach = attach_column;
    l->detach = detach_column;
    l->resize = resize_column;
    l->focus = focus_column;
    l->frames = frames_column;
    l->sel = sel_column;
    l->actions = actions_column;

    for(lp = layouts; lp && lp->next; lp = lp->next);
    if(lp)
        lp->next = l;
    else
        layouts = l;
}

static void
xarrange_column(Column * column)
{
    Cell *cell;
    unsigned int i = 0, h = layout_rect.height / column->ncells;
    for(cell = column->cells; cell; cell = cell->next) {
        Frame *f = cell->frame;
        f->rect = column->rect;
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
arrange_column(Layout *l)
{
    Acme *acme = l->aux;
    Column *column;
    for(column = acme->columns; column; column = column->next)
        xarrange_column(column);
    XSync(dpy, False);
}

static void
init_column(Layout *l, Client * clients)
{
    Client *n, *c = clients;

    l->aux = cext_emallocz(sizeof(Acme));
    while(c) {
        n = c->next;
        attach_column(l, c);
        c = n;
    }
}

static void
attach_frame(Layout *l, Column * column, Frame * new)
{
    Acme *acme = l->aux;
    Cell *c, *cell = cext_emallocz(sizeof(Cell));
    Frame *f;

    cell->frame = new;
    cell->column = column;
    new->aux = cell;
    for(c = column->cells; c && c->next; c = c->next);
    if(!c)
        column->cells = cell;
    else {
        c->next = cell;
        cell->prev = c;
    }
    column->ncells++;

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
    Column *column = cell->column;

    if(cell->prev)
        cell->prev->next = cell->next;
    else
        column->cells = cell->next;
    if(cell->next)
        cell->next->prev = cell->prev;

    if(column->sel == cell)
        column->sel = column->cells;

    free(cell);
    column->ncells--;

    if(old->prev)
        old->prev->next = old->next;
    else
        acme->frames = old->next;
    if(old->next)
        old->next->prev = old->prev;
	old->prev = old->next = nil;
    old->aux = nil;
    detach_frame_from_layout(old);
    acme->nframes--;
}

static Client *
deinit_column(Layout *l)
{
    Acme *acme = l->aux;
    Frame *f;
    Client *cl, *res = nil, *c = nil;
    Column *column = acme->columns;

    while((f = acme->frames)) {
		cl = f->client;
		detach_client_from_frame(cl, False);
		cl->prev = cl->next = 0;
		if(!c)
			res = c = cl;
		else {
			c->next = cl;
			cl->prev = c;
			c = cl;
		}
        detach_frame(l, f);
        destroy_frame(f);
    }
    while((column = acme->columns)) {
        acme->columns = column->next;
        free(column);
    }
    free(acme);
    l->aux = 0;
    return res;
}

static Bool
attach_column(Layout *l, Client * c)
{
    Acme *acme = l->aux;
    Column *column = acme->sel;
    Frame *f = nil;

    if(!column) {
        column = cext_emallocz(sizeof(Column));
        column->rect = layout_rect;
        acme->columns = acme->sel = column;
        acme->ncolumns++;
    }

    f = alloc_frame(&c->rect);
    attach_frame(l, column, f);
    attach_client_to_frame(f, c);
    xarrange_column(column);
    if(l->page == selpage)
        XMapWindow(dpy, f->win);
    focus_column(l, c, True);
    return True;
}

static void
update_column_width(Layout *l)
{
    Acme *acme = l->aux;
    unsigned int i = 0, width;
    Column *column;

    if(!acme->ncolumns)
        return;

    width = layout_rect.width / acme->ncolumns;
    for(column = acme->columns; column; column = column->next) {
        column->rect = layout_rect;
        column->rect.x = i * width;
        column->rect.width = width;
        i++;
    }
    arrange_column(l);
}

static void
detach_column(Layout *l, Client * c, Bool unmap)
{
    Acme *acme = l->aux;
    Frame *f = c->frame;
    Cell *old = f->aux;
    Column *column = old->column;

    detach_client_from_frame(c, unmap);
    detach_frame(l, f);
    destroy_frame(f);
    if(column->cells) {
		if(column->sel == old)
			column->sel = column->cells;
        xarrange_column(column);
	}
    else {
        if(acme->sel == column) {
            if(column->prev)
                acme->sel = column->prev;
            else
                acme->sel = nil;
        }
        if(column->prev)
            column->prev->next = column->next;
        else
            acme->columns = column->next;
        if(column->next)
            column->next->prev = column->prev;
        if(!acme->sel)
            acme->sel = acme->columns;
        acme->ncolumns--;
        free(column);
        update_column_width(l);
    }
}

static void
match_frame_horiz(Column * column, XRectangle * r)
{
    Cell *cell;
    for(cell = column->cells; cell; cell = cell->next) {
        Frame *f = cell->frame;
        f->rect.x = r->x;
        f->rect.width = r->width;
        resize_frame(f, &f->rect, nil);
    }
}

static void
drop_resize(Frame * f, XRectangle * new)
{
    Column *west = nil, *east = nil, *column = nil;
    Cell *north = nil, *south = nil;
    Cell *cell = f->aux;

    column = cell->column;
    west = column->prev;
    east = column->next;
    north = cell->prev;
    south = cell->next;

    /* horizontal resize */
    if(west && (new->x != f->rect.x)) {
        west->rect.width = new->x - west->rect.x;
        column->rect.width += f->rect.x - new->x;
        column->rect.x = new->x;
        match_frame_horiz(west, &west->rect);
        match_frame_horiz(column, &column->rect);
    }
    if(east && (new->x + new->width != f->rect.x + f->rect.width)) {
        east->rect.width -= new->x + new->width - east->rect.x;
        east->rect.x = new->x + new->width;
        column->rect.x = new->x;
        column->rect.width = new->width;
        match_frame_horiz(column, &column->rect);
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
    Column *src = cell->column, *tgt = 0;
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
            xarrange_column(src);
            xarrange_column(tgt);
            focus_column(l, f->sel, True);
        } else {
            Cell *c;
            for(c = src->cells;
                c && !blitz_ispointinrect(pt->x, pt->y, &c->frame->rect);
                c = c->next);
            if(c && c != cell) {
                Frame *tmp = c->frame;
                c->frame = f;
                cell->frame = tmp;
                xarrange_column(src);
                focus_column(l, f->sel, True);
            }
        }
    }
}

static void
resize_column(Frame * f, XRectangle * new, XPoint * pt)
{
    if((f->rect.width == new->width)
       && (f->rect.height == new->height))
        drop_moving(f, new, pt);
    else
        drop_resize(f, new);
}

static void
focus_column(Layout *l, Client *c, Bool raise)
{
    Acme *acme = l->aux;
	Cell *cell = c->frame->aux;
	Client *old = sel_client();

	c->frame->sel = c;
    acme->sel = cell->column;
	cell->column->sel = cell;
    l->file[L_SEL_FRAME]->content = c->frame->file[F_PREFIX]->content;

    if(raise)
    	XWarpPointer(dpy, None, c->win, 0, 0, 0, 0,
					 c->rect.width / 2, c->rect.height / 2);

	focus_client(c, old);
}

static Frame *
frames_column(Layout *l)
{
    Acme *acme = l->aux;
    return acme->frames;
}

static Client *
sel_column(Layout *l)
{
    Acme *acme = l->aux;
    Column *column = acme->sel;

    if(column && column->sel)
        return column->sel->frame->sel;
    return nil;
}

static Action *
actions_column(Layout *l)
{
    return lcol_acttbl;
}

static void
max_frame(void *obj, char *arg)
{
    Layout *l = obj;
    Acme *acme = l->aux;
    Column *c = acme->sel;
	Cell *cell;
    Frame *f;
	
    if(!c)
        return;

    cell = c->sel;
    if(!cell)
        return;

	f = cell->frame;
	if(f->maximized) {
		f->rect = f->old;
		resize_frame(f, &f->old, nil);
		f->maximized = False;
	}
	else {
		f->old = f->rect;
		f->rect = c->rect;
		XRaiseWindow(dpy, f->win);
		resize_frame(f, &c->rect, nil);
		f->maximized = True;
	}
}

static void
select_frame(void *obj, char *arg)
{
    Layout *l = obj;
    Acme *acme = l->aux;
    Column *c, *column = acme->sel;
    Cell *cell;

    if(!column)
        return;

    cell = column->sel;
    if(!cell || !arg)
        return;
    if(!strncmp(arg, "prev", 5)) {
        if(cell->prev)
            cell = cell->prev;
        else
            for(cell = column->cells; cell && cell->next; cell = cell->next);
    } else if(!strncmp(arg, "next", 5)) {
        if(cell->next)
            cell = cell->next;
        else
            cell = column->cells;
    } else if(!strncmp(arg, "west", 5)) {
        if(column->prev)
            cell = column->prev->sel;
        else {
            for(c = acme->columns; c && c->next; c = c->next);
			cell = c->sel;
		}
    } else if(!strncmp(arg, "east", 5)) {
        if(column->next)
            cell = column->next->sel;
        else
            cell = acme->columns->sel;
    } else {
        unsigned int i = 0, idx = blitz_strtonum(arg, 0, column->ncells - 1);
        for(cell = column->cells; cell && i != idx; cell = cell->next)
            i++;
    }
    if(cell && cell != column->sel)
        focus_column(l, cell->frame->sel, True);
}

static void
swap_frame(void *obj, char *arg)
{
    Layout *l = obj;
    Acme *acme = l->aux;
    Column *west = nil, *east = nil, *column = acme->sel;
    Cell *north = nil, *south = nil, *cell = column->sel;
    Frame *f;
    XRectangle r;

    if(!column || !cell || !arg)
        return;

    west = column->prev;
    east = column->next;
    north = cell->prev;
    south = cell->next;

	if(!west)
		west = east;
	if(!east)
		east = west;

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
        focus_column(l, cell->frame->sel, True);
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
        focus_column(l, cell->frame->sel, True);
    } else if(!strncmp(arg, "west", 5) && west && column->ncells && west->ncells) {
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
        focus_column(l, other->frame->sel, True);
    } else if(!strncmp(arg, "east", 5) && east && column->ncells && east->ncells) {
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
        focus_column(l, other->frame->sel, True);
    }
}

static void
new_column(void *obj, char *arg)
{
    Layout *l = obj;
    Acme *acme = l->aux;
    Column *c, *new, *column = acme->sel;
    Frame *f;

    if(!column || column->ncells < 2)
        return;

    f = column->sel->frame;
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
    focus_column(l, f->sel, True);
}
