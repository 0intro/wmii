/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <assert.h>
#include <stdio.h>

#include "cext.h"

static void iter_print_container(void *item, void *aux)
{
	printf("%d\n", *(int *)item);
}

int main(int argc, char *argv[])
{

	Container c = {0};
	int i;

	printf("--------------------------------\n");
	{
		int *e = cext_emallocz(sizeof(int));
		cext_attach_item(&c, e);
		cext_iterate(&c, nil, iter_print_container);
		cext_detach_item(&c, e);
		cext_iterate(&c, nil, iter_print_container);
	}
	printf("--------------------------------\n");

	printf("--------------------------------\n");
	for (i = 0; i < 10; i++) {
		int *e = cext_emallocz(sizeof(int));
		*e = i;
		cext_attach_item(&c, e);
	}
	cext_iterate(&c, nil, iter_print_container);
	printf("--------------------------------\n");

	printf("--------------------------------\n");
	for (i = 0; i < 10; i++) {
		int *e = cext_list_get_item(&c, i);
		printf("%d\n", *e);
	}
	printf("--------------------------------\n");

	printf("--------------------------------\n");
	{
		CItem *itm = c.stack;
		for (; itm; itm = itm->down)
			printf("%d\n", *(int *)itm->item);
	}
	printf("--------------------------------\n");

	printf("--------------------------------\n");
	{
		int *e = cext_list_get_item(&c, 5);
		cext_stack_top_item(&c, e);
		e = cext_stack_get_top_item(&c);
		printf("%d (5)\n", *e);
		e = cext_stack_get_up_item(&c, cext_stack_get_top_item(&c));
		printf("%d (0)\n", *e);
		e = cext_stack_get_down_item(&c, cext_stack_get_top_item(&c));
		printf("%d (9)\n", *e);
	}
	printf("--------------------------------\n");

	printf("--------------------------------\n");
	{
		CItem *itm = c.stack;
		for (; itm; itm = itm->down)
			printf("%d\n", *(int *)itm->item);
	}
	printf("--------------------------------\n");

	printf("--------------------------------\n");
	{
		int *e = cext_list_get_item(&c, 5);
		cext_detach_item(&c, e);	
		cext_iterate(&c, nil, iter_print_container);
	}
	printf("--------------------------------\n");
	
	printf("--------------------------------\n");
	{
		CItem *itm = c.stack;
		for (; itm; itm = itm->down)
			printf("%d\n", *(int *)itm->item);
	}
	printf("--------------------------------\n");

	printf("--------------------------------\n");
	{
		int *e = cext_list_get_item(&c, 4);
		cext_detach_item(&c, e);	
		cext_iterate(&c, nil, iter_print_container);
	}
	printf("--------------------------------\n");
	
	printf("--------------------------------\n");
	{
		CItem *itm = c.stack;
		for (; itm; itm = itm->down)
			printf("%d\n", *(int *)itm->item);
	}
	printf("--------------------------------\n");
	return TRUE;
}
