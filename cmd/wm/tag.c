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
	unsigned int i;
	Bool hastag = False;

	for(i = 0; i < v->ntag; i++) {
		if(clienthastag(c, v->tag[i]))
			hastag = True;
		break;
	}

	if(hastag) {
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
	if(!istag(arg)) {
		cext_vattach(tag2vector(&tag), strdup(arg));
	}
}

void
update_tags()
{
	unsigned int i, j;
	for(i = 0; i < tag.size; i++) {
		free(tag.data[i]);
		tag.data[i] = nil;
	}
	tag.size = 0;

	for(i = 0; i < client.size; i++) {
		for(j = 0; j < client.data[i]->ntag; j++) {
			ensure_tag(client.data[i]->tag[j]);
		}
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

	if(view.size && !hasclient(view.data[sel])) {
		destroy_view(view.data[sel]);
		if(view.size)
			focus_view(view.data[sel]);
		else
			update_bar_tags();
	}

	if(!view.size && tag.size)
		select_view(tag.data[0]);
	else
		update_bar_tags();
}

unsigned int
str2tags(char tags[MAX_TAGS][MAX_TAGLEN], const char *stags)
{
	unsigned int i, n;
	char buf[256];
	char *toks[MAX_TAGS];

	cext_strlcpy(buf, stags, sizeof(buf));
	n = cext_tokenize(toks, MAX_TAGS, buf, '+');
	for(i = 0; i < n; i++)
		cext_strlcpy(tags[i], toks[i], MAX_TAGLEN);
	return n;
}

void
tags2str(char *stags, unsigned int stagsz,
		 char tags[MAX_TAGS][MAX_TAGLEN], unsigned int ntags)
{
	unsigned int i, len = 0, l;

	stags[0] = 0;
	for(i = 0; i < ntags; i++) {
		l = strlen(tags[i]);
		if(len + l + 1 >= stagsz)
			return;
		if(len)
			stags[len++] = '+';
		memcpy(stags + len, tags[i], l);
		len += l;
		stags[len] = 0;
	}
}
