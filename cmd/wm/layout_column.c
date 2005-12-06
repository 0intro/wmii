/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "wm.h"
#include "layout.h"

typedef struct Acme Acme;
typedef struct Column Column;

struct Acme {
	int sel;
	Column **column;
};

struct Column {
	int sel;
	int refresh;
	Frame **frame;
	XRectangle rect;
};

static void init_col(Area * a);
static void deinit_col(Area * a);
static void arrange_col(Area * a);
static void attach_col(Area * a, Client * c);
static void detach_col(Area * a, Client * c);
static void resize_col(Frame * f, XRectangle * new, XPoint * pt);

static Layout lcol = { "col", init_col, deinit_col, arrange_col, attach_col, detach_col, resize_col };

static Column zero_column = { 0 };
static Acme zero_acme = { 0 };

void init_layout_column()
{
	layouts = (Layout **) attach_item_end((void **) layouts, &lcol, sizeof(Layout *));
}


static void arrange_column(Area * a, Column * col)
{
	int i;
	int n = count_items((void **) col->frame);
	unsigned int height = a->rect.height / n;
	for (i = 0; col->frame && col->frame[i]; i++) {
		if (col->refresh) {
			col->frame[i]->rect = col->rect;
			col->frame[i]->rect.height = height;
			col->frame[i]->rect.y = i * height;
		}
		resize_frame(col->frame[i], &col->frame[i]->rect, 0);
	}
}

static void arrange_col(Area * a)
{
	Acme *acme = a->aux;
	int i;

	if (!acme) {
		fprintf(stderr, "%s", "wmiiwm: fatal, page has no layout\n");
		exit(1);
	}
	for (i = 0; acme->column && acme->column[i]; i++)
		arrange_column(a, acme->column[i]);
}

static void init_col(Area * a)
{
	Acme *acme = cext_emalloc(sizeof(Acme));
	int i, j, n, cols = 1;
	unsigned int width = 1;
	Column *col;

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
	acme->column = cext_emalloc((cols + 1) * sizeof(Column *));
	for (i = 0; i < cols; i++) {
		acme->column[i] = cext_emalloc(sizeof(Column));
		*acme->column[i] = zero_column;
		acme->column[i]->rect = a->rect;
		acme->column[i]->rect.x = i * width;
		acme->column[i]->rect.width = width;
		acme->column[i]->refresh = 1;
	}
	acme->column[cols] = 0;		/* null termination of array */

	/*
	 * Frame attaching strategy works as follows: 1. If more client than
	 * column exist, then each column gets one client, except eastmost
	 * column, which gets all remaining client. 2. If lesser client
	 * than column exist, than filling begins from eastmost to westmost
	 * column until no more client exist.
	 */
	n = count_items((void **) client);
	if (n > cols) {
		/* 1st. case */
		j = 0;
		for (i = 0; i < (cols - 1); i++) {
			col = acme->column[i];
			col->frame = (Frame **) attach_item_end((void **) col->frame, alloc_frame(&client[i]->rect), sizeof(Frame *));
			col->frame[0]->aux = col;
			attach_frame_to_area(a, col->frame[0]);
			attach_client_to_frame(col->frame[0], client[j]);
			j++;
		}
		col = acme->column[cols - 1];
		col->frame = cext_emalloc((n - j + 1) * sizeof(Frame *));
		for (i = 0; i + j < n; i++) {
			col->frame[i] = alloc_frame(&client[j + i]->rect);
			col->frame[i]->aux = col;
			attach_frame_to_area(a, col->frame[i]);
			attach_client_to_frame(col->frame[i], client[j + i]);
		}
		col->frame[i] = 0;
	} else {
		/* 2nd case */
		j = 0;
		for (i = cols - 1; j < n; i--) {
			col = acme->column[i];
			col->frame = (Frame **) attach_item_end((void **) col->frame, alloc_frame(&client[i]->rect), sizeof(Frame *));
			col->frame[0]->aux = col;
			attach_frame_to_area(a, col->frame[0]);
			attach_client_to_frame(col->frame[0], client[j]);
			j++;
		}
	}
	arrange_col(a);
}

static void deinit_col(Area * a)
{
	Acme *acme = a->aux;
	int i;

	for (i = 0; acme->column && acme->column[i]; i++) {
		Column *col = acme->column[i];
		int j;
		for (j = 0; col->frame && col->frame[j]; j++) {
			Frame *f = col->frame[j];
			while (f->client && f->client[0])
				detach_client_from_frame(f->client[0]);
			detach_frame_from_area(f, 1);
			destroy_frame(f);
		}
		free(col->frame);
	}
	free(acme->column);
	free(acme);
	a->aux = 0;
}

static void attach_col(Area * a, Client * c)
{
	Acme *acme = a->aux;
	Column *col;
	Frame *f;

	col = acme->column[acme->sel];
	f = alloc_frame(&c->rect);
	col->frame = (Frame **) attach_item_end((void **) col->frame, f, sizeof(Frame *));
	f->aux = col;
	col->refresh = 1;
	attach_frame_to_area(a, f);
	attach_client_to_frame(f, c);

	arrange_col(a);
}

static void detach_col(Area * a, Client * c)
{
	Frame *f = c->frame;
	Column *col = f->aux;

	if (!col)
		return;					/* client was not attached, maybe exit(1) in
								 * such case */
	col->frame = (Frame **) detach_item((void **) col->frame, c->frame, sizeof(Frame *));
	col->refresh = 1;
	detach_client_from_frame(c);
	detach_frame_from_area(f, 1);
	destroy_frame(f);

	arrange_col(a);
}

static void drop_resize(Frame * f, XRectangle * new)
{
	Column *col = f->aux;
	Acme *acme = f->area->aux;
	int i, idx, n = 0;

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
			Column *west = acme->column[idx - 1];
			west->rect.width = new->x - west->rect.x;
			col->rect.width += f->rect.x - new->x;
			col->rect.x = new->x;

			for (i = 0; west->frame && west->frame[i]; i++) {
				Frame *f = west->frame[i];
				f->rect.x = west->rect.x;
				f->rect.width = west->rect.width;
				resize_frame(f, &f->rect, 0);
			}
			for (i = 0; col->frame && col->frame[i]; i++) {
				Frame *f = col->frame[i];
				f->rect.x = col->rect.x;
				f->rect.width = col->rect.width;
				resize_frame(f, &f->rect, 0);
			}
		}
	}
	if (new->x + new->width > f->rect.x + f->rect.width) {
		if ((idx + 1 < n) &&
			(new->x + new->width < acme->column[idx + 1]->rect.x
			 + acme->column[idx + 1]->rect.width)) {
			Column *east = acme->column[idx + 1];
			east->rect.width -= new->x + new->width - east->rect.x;
			east->rect.x = new->x + new->width;
			col->rect.x = new->x;
			col->rect.width = new->width;

			for (i = 0; col->frame && col->frame[i]; i++) {
				Frame *f = col->frame[i];
				f->rect.x = col->rect.x;
				f->rect.width = col->rect.width;
				resize_frame(f, &f->rect, 0);
			}
			for (i = 0; east->frame && east->frame[i]; i++) {
				Frame *f = east->frame[i];
				f->rect.x = east->rect.x;
				f->rect.width = east->rect.width;
				resize_frame(f, &f->rect, 0);
			}
		}
	}
	/* vertical stuff */
	n = 0;
	for (i = 0; col->frame && col->frame[i]; i++) {
		if (col->frame[i] == f)
			idx = i;
		n++;
	}

	if (new->y < f->rect.y) {
		if (idx && new->y > col->frame[idx - 1]->rect.y) {
			Frame *north = col->frame[idx - 1];
			north->rect.height = new->y - north->rect.y;
			f->rect.width += new->y - north->rect.y;
			f->rect.y = new->y;
			resize_frame(north, &north->rect, 0);
			resize_frame(f, &f->rect, 0);
		}
	}
	if (new->y + new->height > f->rect.y + f->rect.height) {
		if ((idx + 1 < n) &&
			(new->y + new->height < col->frame[idx + 1]->rect.y
			 + col->frame[idx + 1]->rect.height)) {
			Frame *south = col->frame[idx + 1];
			south->rect.width -= new->x + new->width - south->rect.x;
			south->rect.x = new->x + new->width;
			f->rect.x = new->x;
			f->rect.width = new->width;
			resize_frame(f, &f->rect, 0);
			resize_frame(south, &south->rect, 0);
		}
	}
}

static void _drop_move(Frame * f, XRectangle * new, XPoint * pt)
{
	Column *tgt = 0, *src = f->aux;
	Acme *acme = f->area->aux;
	int i;

	if (!src) {
		fprintf(stderr, "%s",
				"wmii: fatal: frame has no associated column\n");
		exit(1);
	}
	for (i = 0; acme->column && acme->column[i]; i++) {
		Column *colp = acme->column[i];
		if (blitz_ispointinrect(pt->x, pt->y, &colp->rect)) {
			tgt = colp;
			break;
		}
	}

	/* use pointer as fixpoint */
	if (tgt == src) {
		/* only change order within column */
		for (i = 0; tgt->frame && tgt->frame[i]; i++) {
			Frame *fp = tgt->frame[i];
			if (blitz_ispointinrect(pt->x, pt->y, &fp->rect)) {
				if (fp == f)
					return;		/* just ignore */
				else {
					int idxf = index_item((void **) tgt->frame, f);
					int idxfp = index_item((void **) tgt->frame, fp);
					Frame *tmpf = f;
					XRectangle tmpr = f->rect;

					f->rect = fp->rect;
					fp->rect = tmpr;
					tgt->frame[idxf] = tgt->frame[idxfp];
					tgt->frame[idxfp] = tmpf;
					resize_frame(f, &f->rect, 0);
					resize_frame(fp, &fp->rect, 0);
				}
				return;
			}
		}
	} else {
		/* detach, attach and change order in target column */
		src->frame = (Frame **) detach_item((void **) src->frame, f, sizeof(Frame *));
		tgt->frame =
			(Frame **) attach_item_end((void **) tgt->frame, f,
									   sizeof(Frame *));
		tgt->refresh = 1;
		arrange_column(f->area, tgt);

		/* TODO: implement a better target placing strategy */
	}
}

static void resize_col(Frame * f, XRectangle * new, XPoint * pt)
{
	if ((f->rect.width == new->width)
		&& (f->rect.height == new->height))
		_drop_move(f, new, pt);
	else
		drop_resize(f, new);
}
