/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <cext.h>
#include <stdlib.h>

#include "blitz.h"

/*
 * Each layout can be a widget. A layout may consist of 
 * columns or rows of widgets, or of columns and rows of widgets.
 * Rows are scaled left to right, the first widget with
 * expand == True takes the remaining space of the overall row.
 * Columns are scaled top to bottom, the first widget with
 * expand == True takes the remaining space of the overall column.
 */
static void
xscale(BlitzWidget *w)
{

}

static void
xdraww(BlitzWidget *w)
{
	switch(w->type) {
		case BLITZ_LABEL:
			BLITZLABEL(w)->draw(w);
			break;
		case BLITZ_LAYOUT:
			BLITZLAYOUT(w)->draw(w);
			break;
	}
}

static void
xdraw(BlitzWidget *w)
{
	BlitzLayout *l = BLITZLAYOUT(w);
	for(w = l->cols; w; w = w->next)
		xdraww(w);
	for(w = l->rows; w; w = w->next)
		xdraww(w);
}

BlitzLayout *
blitz_create_layout(BlitzWin *win)
{
	BlitzLayout *l;
	l = cext_emallocz(sizeof(BlitzLayout));
	l->win = win;
	l->cols = l->rows = nil;
	l->scale = xscale;
	l->draw = xdraw;
	return l;
}

void
blitz_add_widget(BlitzWidget **l, BlitzWidget *w)
{
	BlitzWidget **wt;
	for(wt = l; *wt; wt = &(*wt)->next);
	w->next = nil;
	*wt = w;
}

void
blitz_rm_widget(BlitzWidget **l, BlitzWidget *w)
{
	BlitzWidget **wt;
	for(wt = l; *wt && *wt != w; wt = &(*wt)->next);
	cext_assert(*wt == w);
	*wt = w->next;
}

int
blitz_destroy_layout(BlitzLayout *l)
{
	if(l->cols || l->rows)
		return -1;
	free(l);
	return 0;
}
