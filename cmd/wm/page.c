/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xatom.h>

#include "wm.h"

static void handle_after_write_page(IXPServer * s, File * file);

static void toggle_area(void *obj, char *arg);

/* action table for /?/ namespace */
Action page_acttbl[] = {
    {"toggle", toggle_area},
    {0, 0}
};

Page *
alloc_page()
{
    Page *p, *new = cext_emallocz(sizeof(Page));
    char buf[MAX_BUF], buf2[16];

    snprintf(buf2, sizeof(buf2), "%ul", npages);
    snprintf(buf, sizeof(buf), "/%ul", npages);
    new->file[P_PREFIX] = ixp_create(ixps, buf);
    snprintf(buf, sizeof(buf), "/%ul/name", npages);
    new->file[P_NAME] = wmii_create_ixpfile(ixps, buf, buf2);
    snprintf(buf, sizeof(buf), "/%ul/layout/", npages);
    new->file[P_AREA_PREFIX] = ixp_create(ixps, buf);
    snprintf(buf, sizeof(buf), "/%ul/layout/sel", npages);
    new->file[P_SEL_AREA] = ixp_create(ixps, buf);
    new->file[P_SEL_AREA]->bind = 1;    /* mount point */
    snprintf(buf, sizeof(buf), "/%ul/ctl", npages);
    new->file[P_CTL] = ixp_create(ixps, buf);
    new->file[P_CTL]->after_write = handle_after_write_page;
    new->floating = alloc_area(new, "float");
    new->sel = new->managed = alloc_area(new, def[WM_LAYOUT]->content);
    for(p = pages; p && p->next; p = p->next);
    if(!p) {
        pages = new;
        new->index = 0;
    }
    else {
        new->prev = p;
        p->next = new;
        new->index = p->index + 1;
    }
    selpage = new;
    def[WM_SEL_PAGE]->content = new->file[P_PREFIX]->content;
    invoke_wm_event(def[WM_EVENT_PAGE_UPDATE]);
    npages++;
    XChangeProperty(dpy, root, net_atoms[NET_NUMBER_OF_DESKTOPS], XA_CARDINAL, 32, PropModeReplace, (unsigned char *) &npages, 1);
    focus_page(new);
    return new;
}

void
destroy_page(Page * p)
{
    destroy_area(p->floating);
    destroy_area(p->managed);
    def[WM_SEL_PAGE]->content = 0;
    ixp_remove_file(ixps, p->file[P_PREFIX]);
    if(p == selpage) {
        if(p->prev)
            selpage = p->prev;
        else
            selpage = nil;
    }

    if(p == pages) {
        if(p->next)
            p->next->prev = nil;
        pages = p->next;
        pages->index = 0;
    } else {
        p->prev->next = p->next;
        if(p->next)
            p->next->prev = p->prev;
    }

    free(p); 

    /* update page indexes */
    for (p = pages; p && p->next; p = p->next) {
      if (p->prev && p->prev->index + 1 != p->index) /* if page index difference is not one */
        --(p->index);
    }
 
    if(!selpage)
        selpage = pages;
    npages--;
    XChangeProperty(dpy, root, net_atoms[NET_NUMBER_OF_DESKTOPS], XA_CARDINAL, 32, PropModeReplace, (unsigned char *) &npages, 1);

    if(selpage)
        focus_page(selpage);
}

void
focus_page(Page * p)
{
    if(selpage != p)
        hide_page(selpage);
    selpage = p;
    show_page(p);
    def[WM_SEL_PAGE]->content = p->file[P_PREFIX]->content;
    invoke_wm_event(def[WM_EVENT_PAGE_UPDATE]);
    focus_area(sel_area());
    XChangeProperty(dpy, root, net_atoms[NET_CURRENT_DESKTOP], XA_CARDINAL, 32, PropModeReplace, (unsigned char *) &(selpage->index), 1);
}

XRectangle *
rectangles(unsigned int *num)
{
    XRectangle *result = 0;
    int i, j = 0;
    Window d1, d2;
    Window *wins;
    XWindowAttributes wa;
    XRectangle r;

    if(XQueryTree(dpy, root, &d1, &d2, &wins, num)) {
        result = cext_emallocz(*num * sizeof(XRectangle));
        for(i = 0; i < *num; i++) {
            if(!XGetWindowAttributes(dpy, wins[i], &wa))
                continue;
            if(wa.override_redirect && (wa.map_state == IsViewable)) {
                r.x = wa.x;
                r.y = wa.y;
                r.width = wa.width;
                r.height = wa.height;
                result[j++] = r;
            }
        }
    }
    if(wins) {
        XFree(wins);
    }
    *num = j;
    return result;
}

void
hide_page(Page * p)
{
    hide_area(p->managed);
    hide_area(p->floating);
}

void
show_page(Page * p)
{
    show_area(p->managed, False);
    show_area(p->floating, False);
}

static void
handle_after_write_page(IXPServer * s, File * file)
{
    Page *p;
    for(p = pages; p; p = p->next) {
        if(file == p->file[P_CTL]) {
            run_action(file, p, page_acttbl);
            return;
        }
    }
}

static void
toggle_area(void *obj, char *arg)
{
    Page *p = obj;

    if(p->sel == p->managed)
        p->sel = p->floating;
    else
        p->sel = p->managed;

    focus_area(p->sel);
    invoke_wm_event(def[WM_EVENT_PAGE_UPDATE]);
}

Page *
pageat(unsigned int idx)
{
    unsigned int i = 0;
    Page *p;
    for(p = pages; p && i != idx; p = p->next)
		i++;
    return p;
}
