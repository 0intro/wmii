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
xdraw(BlitzWidget *w)
{
	BlitzLayout *l = BLITZLAYOUT(w);
	for(w = l->cols; w; w = w->next)
		if(w->draw)
			w->draw(w);
	for(w = l->rows; w; w = w->next)
		if(w->draw)
			w->draw(w);
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
