/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "wm.h"
#include "layout.h"

#include <cext.h>

typedef struct Acme Acme;
typedef struct Column Column;

struct Acme {
	int             sel;
	Column        **column;
};

struct Column {
	int             sel;
	int             refresh;
	Frame         **frames;
	XRectangle      rect;
};

static void     init_col(Area *a);
static void     deinit_col(Area *a);
static void     arrange_col(Area *a);
static void     attach_col(Area *a, Client *c);
static void     detach_col(Area *a, Client *c, int unmapped, int destroyed);
static void     resize_col(Frame *f, XRectangle *new, XPoint * pt);

static Layout lcol = {"col", init_col, deinit_col, arrange_col, attach_col, detach_col, resize_col};

static Column   zero_column = {0};
static Acme     zero_acme = {0};

void 
init_layout_col()
{
	layouts =
	(Layout **) attach_item_end((void **) layouts, &lcol,
				    sizeof(Layout *));
}


static void 
arrange_column(Area *a, Column *col)
{
	int             i;
	int             n = count_items((void **) col->frames);
	unsigned int    height = a->rect.height / n;
	for (i = 0; col->frames && col->frames[i]; i++) {
		if (col->refresh) {
			col->frames[i]->rect = col->rect;
			col->frames[i]->rect.height = height;
			col->frames[i]->rect.y = i * height;
		}
		resize_frame(col->frames[i], &col->frames[i]->rect, 0, 1);
	}
}

static void 
arrange_col(Area *a)
{
	Acme           *acme = a->aux;
	int             i;

	if (!acme) {
		fprintf(stderr, "%s", "wmiiwm: fatal, page has no layout\n");
		exit(1);
	}
	for (i = 0; acme->column && acme->column[i]; i++)
		arrange_column(a, acme->column[i]);
}

static void 
init_col(Area *a)
{
	Acme           *acme = emalloc(sizeof(Acme));
	int             i, j, n, cols = 1;
	unsigned int    width = 1;
	Column         *col;

	*acme = zero_acme;
	a->aux = acme;

	/* processing argv */
	/*
	for (i = 1; (i < argc) && (argv[i][0] == '-'); i++) {
		switch (argv[i][1]) {
		case 'c':
			cols = _strtonum(argv[++i], 0, 32);
			if (cols < 1)
				cols = 1;
			break;
		}
	}
	*/

	width = a->rect.width / cols;
	acme->column = emalloc((cols + 1) * sizeof(Column *));
	for (i = 0; i < cols; i++) {
		acme->column[i] = emalloc(sizeof(Column));
		*acme->column[i] = zero_column;
		acme->column[i]->rect = a->rect;
		acme->column[i]->rect.x = i * width;
		acme->column[i]->rect.width = width;
		acme->column[i]->refresh = 1;
	}
	acme->column[cols] = 0;	/* null termination of array */

	/*
	 * Frame attaching strategy works as follows: 1. If more clients than
	 * column exist, then each column gets one client, except eastmost
	 * column, which gets all remaining clients. 2. If lesser clients
	 * than column exist, than filling begins from eastmost to westmost
	 * column until no more clients exist.
	 */
	n = count_items((void **) clients);
	if (n > cols) {
		/* 1st. case */
		j = 0;
		for (i = 0; i < (cols - 1); i++) {
			col = acme->column[i];
			col->frames = (Frame **) attach_item_end((void **) col->frames,
								 alloc_frame(&clients[i]->rect, 1, 1), sizeof(Frame *));
			col->frames[0]->aux = col;
			attach_frame_to_area(a, col->frames[0], 1);
			attach_Cliento_frame(col->frames[0], clients[j]);
			j++;
		}
		col = acme->column[cols - 1];
		col->frames = emalloc((n - j + 1) * sizeof(Frame *));
		for (i = 0; i + j < n; i++) {
			col->frames[i] = alloc_frame(&clients[j + i]->rect, 1, 1);
			col->frames[i]->aux = col;
			attach_Frameo_page(p, col->frames[i], 1);
			attach_Cliento_frame(col->frames[i], clients[j + i]);
		}
		col->frames[i] = 0;
	} else {
		/* 2nd case */
		j = 0;
		for (i = cols - 1; j < n; i--) {
			col = acme->column[i];
			col->frames = (Frame **) attach_item_end((void **) col->frames,
								 alloc_frame(&clients[i]->rect, 1, 1), sizeof(Frame *));
			col->frames[0]->aux = col;
			attach_Frameo_page(p, col->frames[0], 1);
			attach_Cliento_frame(col->frames[0], clients[j]);
			j++;
		}
	}
	arrange_col(p);
}

static void 
deinit_col(Page * p)
{
	Acme           *acme = p->aux;
	int             i;

	for (i = 0; acme->column && acme->column[i]; i++) {
		Column         *col = acme->column[i];
		int             j;
		for (j = 0; col->frames && col->frames[j]; j++) {
			Frame          *f = col->frames[j];
			while (f->clients && f->clients[0])
				detach_client_from_frame(f->clients[0], 0, 0);
			detach_frame_from_page(f, 1);
			free_frame(f);
		}
		free(col->frames);
	}
	free(acme->column);
	free(acme);
	p->aux = 0;
}

static void 
attach_col(Page * p, Client * c)
{
	Acme           *acme = p->aux;
	Column         *col;
	Frame          *f;

	col = acme->column[acme->sel];
	f = alloc_frame(&c->rect, 1, 1);
	col->frames = (Frame **) attach_item_end((void **) col->frames, f,
						 sizeof(Frame *));
	f->aux = col;
	col->refresh = 1;
	attach_Frameo_page(p, f, 1);
	attach_Cliento_frame(f, c);

	arrange_col(p);
}

static void 
detach_col(Page * p, Client * c, int unmapped, int destroyed)
{
	Frame          *f = c->frame;
	Column         *col = f->aux;

	if (!col)
		return;		/* client was not attached, maybe exit(1) in
				 * such case */
	col->frames = (Frame **) detach_item((void **) col->frames, c->frame,
					     sizeof(Frame *));
	col->refresh = 1;
	detach_client_from_frame(c, unmapped, destroyed);
	detach_frame_from_page(f, 1);
	free_frame(f);

	arrange_col(p);
}

static void 
drop_resize(Frame * f, XRectangle * new)
{
	Column         *col = f->aux;
	Acme           *acme = f->page->aux;
	int             i, idx, n = 0;

	if (!col) {
		fprintf(stderr, "%s",
			"wmii: fatal: frame has no associated column\n");
		exit(1);
	}
	for (i = 0; acme->column && acme->column[i]; i++) {
		if (acme->column[i] == col)
			idx = i;
		n++;
	}

	/* horizontal resizals are */
	if (new->x < f->rect.x) {
		if (idx && new->x > acme->column[idx - 1]->rect.x) {
			Column         *west = acme->column[idx - 1];
			west->rect.width = new->x - west->rect.x;
			col->rect.width += f->rect.x - new->x;
			col->rect.x = new->x;

			for (i = 0; west->frames && west->frames[i]; i++) {
				Frame          *f = west->frames[i];
				f->rect.x = west->rect.x;
				f->rect.width = west->rect.width;
				resize_frame(f, &f->rect, 0, 1);
			}
			for (i = 0; col->frames && col->frames[i]; i++) {
				Frame          *f = col->frames[i];
				f->rect.x = col->rect.x;
				f->rect.width = col->rect.width;
				resize_frame(f, &f->rect, 0, 1);
			}
		}
	}
	if (new->x + new->width > f->rect.x + f->rect.width) {
		if ((idx + 1 < n) &&
		    (new->x + new->width < acme->column[idx + 1]->rect.x
		     + acme->column[idx + 1]->rect.width)) {
			Column         *east = acme->column[idx + 1];
			east->rect.width -= new->x + new->width - east->rect.x;
			east->rect.x = new->x + new->width;
			col->rect.x = new->x;
			col->rect.width = new->width;

			for (i = 0; col->frames && col->frames[i]; i++) {
				Frame          *f = col->frames[i];
				f->rect.x = col->rect.x;
				f->rect.width = col->rect.width;
				resize_frame(f, &f->rect, 0, 1);
			}
			for (i = 0; east->frames && east->frames[i]; i++) {
				Frame          *f = east->frames[i];
				f->rect.x = east->rect.x;
				f->rect.width = east->rect.width;
				resize_frame(f, &f->rect, 0, 1);
			}
		}
	}
	/* vertical stuff */
	n = 0;
	for (i = 0; col->frames && col->frames[i]; i++) {
		if (col->frames[i] == f)
			idx = i;
		n++;
	}

	if (new->y < f->rect.y) {
		if (idx && new->y > col->frames[idx - 1]->rect.y) {
			Frame          *north = col->frames[idx - 1];
			north->rect.height = new->y - north->rect.y;
			f->rect.width += new->y - north->rect.y;
			f->rect.y = new->y;
			resize_frame(north, &north->rect, 0, 1);
			resize_frame(f, &f->rect, 0, 1);
		}
	}
	if (new->y + new->height > f->rect.y + f->rect.height) {
		if ((idx + 1 < n) &&
		    (new->y + new->height < col->frames[idx + 1]->rect.y
		     + col->frames[idx + 1]->rect.height)) {
			Frame          *south = col->frames[idx + 1];
			south->rect.width -= new->x + new->width - south->rect.x;
			south->rect.x = new->x + new->width;
			f->rect.x = new->x;
			f->rect.width = new->width;
			resize_frame(f, &f->rect, 0, 1);
			resize_frame(south, &south->rect, 0, 1);
		}
	}
}

static void 
_drop_move(Frame * f, XRectangle * new, XPoint * pt)
{
	Column         *tgt = 0, *src = f->aux;
	Acme           *acme = f->page->aux;
	int             i;

	if (!src) {
		fprintf(stderr, "%s",
			"wmii: fatal: frame has no associated column\n");
		exit(1);
	}
	for (i = 0; acme->column && acme->column[i]; i++) {
		Column         *colp = acme->column[i];
		if (blitz_ispointinrect(pt->x, pt->y, &colp->rect)) {
			tgt = colp;
			break;
		}
	}

	/* use pointer as fixpoint */
	if (tgt == src) {
		/* only change order within column */
		for (i = 0; tgt->frames && tgt->frames[i]; i++) {
			Frame          *fp = tgt->frames[i];
			if (blitz_ispointinrect(pt->x, pt->y, &fp->rect)) {
				if (fp == f)
					return;	/* just ignore */
				else {
					int             idxf = index_item((void **) tgt->frames, f);
					int             idxfp = index_item((void **) tgt->frames, fp);
					Frame          *tmpf = f;
					XRectangle      tmpr = f->rect;

					f->rect = fp->rect;
					fp->rect = tmpr;
					tgt->frames[idxf] = tgt->frames[idxfp];
					tgt->frames[idxfp] = tmpf;
					resize_frame(f, &f->rect, 0, 1);
					resize_frame(fp, &fp->rect, 0, 1);
				}
				return;
			}
		}
	} else {
		/* detach, attach and change order in target column */
		src->frames = (Frame **) detach_item((void **) src->frames,
						     f, sizeof(Frame *));
		tgt->frames =
			(Frame **) attach_item_end((void **) tgt->frames, f,
						   sizeof(Frame *));
		tgt->refresh = 1;
		arrange_column(f->page, tgt);

		/* TODO: implement a better target placing strategy */
	}
}

static void 
resize_col(Frame * f, XRectangle * new, XPoint * pt)
{
	if ((f->rect.width == new->width)
	    && (f->rect.height == new->height))
		_drop_move(f, new, pt);
	else
		drop_resize(f, new);
}


static void 
aux_col(Frame * f, char *what)
{
}
