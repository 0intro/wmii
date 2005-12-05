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

static void init_dummy(Area * a);
static void deinit_dummy(Area * a);
static void arrange_dummy(Area * a);
static void attach_dummy(Area * a, Client * c);
static void detach_dummy(Area * a, Client * c, int unmapped, int destroyed);
static void resize_dummy(Frame * f, XRectangle * new, XPoint * pt);

static Layout ldummy = { "dummy", init_dummy, deinit_dummy, arrange_dummy, attach_dummy, detach_dummy, resize_dummy };

void init_layout_dummy()
{
	layouts = (Layout **) attach_item_end((void **) layouts, &ldummy, sizeof(Layout *));
}


static void arrange_dummy(Area * a)
{
}

static void init_dummy(Area * a)
{
}

static void deinit_dummy(Area * a)
{
}

static void attach_dummy(Area * a, Client * c)
{
}

static void detach_dummy(Area * a, Client * c, int unmapped, int destroyed)
{
}

static void resize_dummy(Frame * f, XRectangle * new, XPoint * pt)
{
}
