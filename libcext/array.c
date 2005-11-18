/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>

#include "cext.h"

void          **
attach_item_begin(void **old, void *item, size_t size_item)
{
	int             i, size_old;
	void          **result = 0;
	for (size_old = 0; old && old[size_old]; size_old++);
	result = emalloc(size_item * (size_old + 2));
	result[0] = item;
	for (i = 0; old && old[i]; i++)
		result[i + 1] = old[i];
	result[i + 1] = 0;
	if (old)
		free(old);
	return result;
}

void          **
attach_item_end(void **old, void *item, size_t size_item)
{
	int             i, size_old;
	void          **result = 0;
	for (size_old = 0; old && old[size_old]; size_old++);
	result = emalloc(size_item * (size_old + 2));
	for (i = 0; old && old[i]; i++)
		result[i] = old[i];
	result[i++] = item;
	result[i] = 0;
	if (old)
		free(old);
	return result;
}

void          **
detach_item(void **old, void *item, size_t size_item)
{
	int             size_old, i, j = 0;
	void          **result = 0;
	for (size_old = 0; old && old[size_old]; size_old++);
	if (size_old != 1) {
		result = emalloc(size_item * size_old);
		for (i = 0; old[i]; i++)
			if (old[i] != item)
				result[j++] = old[i];
		result[j] = 0;
	}
	if (old)
		free(old);
	return result;
}

int
index_item(void **items, void *item)
{
	int             i = 0;
	for (i = 0; items && items[i] && (items[i] != item); i++);
	return items[i] ? i : -1;
}

int
count_items(void **items)
{
	int             i;
	for (i = 0; items && items[i]; i++);
	return i;
}

int
index_next_item(void **items, void *item)
{
	int             idx = index_item(items, item);
	if (idx == -1)
		return idx;
	if (idx == count_items(items) - 1)
		return 0;
	else
		return idx + 1;
}

int
index_prev_item(void **items, void *item)
{
	int             idx = index_item(items, item);
	if (idx == -1)
		return idx;
	if (idx == 0)
		return count_items(items) - 1;
	else
		return idx - 1;
}
