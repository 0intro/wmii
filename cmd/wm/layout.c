/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <string.h>

#include "wm.h"

static int comp_layout_name(void *name, void *layout)
{
	Layout *l = layout;
	size_t len = strlen(l->name);

	return !strncmp(name, l->name, len);
}

Layout *get_layout(char *name)
{
	return cext_find_item(&layouts, name, comp_layout_name);
}
