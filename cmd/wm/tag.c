/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include <string.h>

#include "wm.h"

Bool
istag(char *t)
{
	unsigned int i;
	for(i = 0; i < tag.size; i++)
		if(!strncmp(tag.data[i], t, strlen(t)))
			return True;
	return False;
}

static void
organize_client(View *v, Client *c)
{
	if(clienthastag(c, v->name)) {
		if(!clientofview(v, c)) {
			/*fprintf(stderr, "org attach %s?\n", c->name);*/
			attach_toview(v, c);
		}
	}
	else {
		if(clientofview(v, c)) {
			/*fprintf(stderr, "org detach %s?\n", c->name);*/
			detach_fromview(v, c);
		}
	}
}

static Vector *
tag2vector(TagVector *tv)
{
	return (Vector *) tv;
}

void
ensure_tag(char *arg)
{
	if(!istag(arg) && strncmp(arg, "*", 2))
		cext_vattach(tag2vector(&tag), strdup(arg));
}

void
update_tags()
{
	unsigned int i, j;
	while(tag.size) {
		char *p = tag.data[0];
		cext_vdetach(tag2vector(&tag), p);
		free(p);
	}

	for(i = 0; i < client.size; i++) {
		for(j = 0; j < client.data[i]->tag.size; j++)
			ensure_tag(client.data[i]->tag.data[j]);
	}

	for(i = 0; view.size && (i < client.size); i++) {
		for(j = 0; j < view.size; j++) {
			View *v = view.data[j];
			if(j == sel)
				continue;
			organize_client(v, client.data[i]);
		}
		organize_client(view.data[sel], client.data[i]);
	}

	if(view.size && !hasclient(view.data[sel]))
		destroy_view(view.data[sel]);

	if(view.size)
		focus_view(view.data[sel]);
	else if(tag.size)
		select_view(tag.data[0]);
	else
		update_bar_tags();
}

void
str2tagvector(TagVector *tv, const char *tags)
{
	unsigned int i, n;
	char buf[256];
	char *toks[16];
	char *p;

	while(tv->size) {
		p = tv->data[0];
		cext_vdetach(tag2vector(tv), p);
		free(p);
	}
	cext_strlcpy(buf, tags, sizeof(buf));
	n = cext_tokenize(toks, 16, buf, '+');
	for(i = 0; i < n; i++)
		cext_vattach(tag2vector(tv), strdup(toks[i]));
}

void
retag()
{
	unsigned int i;
	for(i = 0; i < client.size; i++)
		match_tags(client.data[i], False);
	update_tags();
}

