/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xatom.h>

#include "wm.h"

Bool
istag(char **tags, unsigned int ntags, char *tag)
{
	unsigned int i;
	for(i = 0; i < ntags; i++)
		if(!strncmp(tags[i], tag, strlen(tag)))
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

void
update_tags()
{
	unsigned int i, j;
	char buf[256];
	char **newtag = nil;
	unsigned int newtagsz = 0, nnewtag = 0;

	for(i = 0; i < nclient; i++) {
		for(j = 0; j < client[i]->ntag; j++) {
			if(!strncmp(client[i]->tag[j], "~", 2)) /* magic floating tag */
				continue;
			if(!istag(newtag, nnewtag, client[i]->tag[j])) {
				newtag = (char **)cext_array_attach((void **)newtag, strdup(client[i]->tag[j]),
							sizeof(char *), &newtagsz);
				nnewtag++;
			}
		}
	}

	for(i = 0; i < nview; i++)
		if(hasclient(view[i])) {
		   	tags2str(buf, sizeof(buf), view[i]->tag, view[i]->ntag);
			if(!istag(newtag, nnewtag, buf)) {
				newtag = (char **)cext_array_attach((void **)newtag, strdup(buf),
							sizeof(char *), &newtagsz);
				nnewtag++;
			}
		}

	/* propagate tagging events */
	for(i = 0; i < nnewtag; i++)
		if(!istag(tag, ntag, newtag[i])) {
			snprintf(buf, sizeof(buf), "NewTag %s\n", newtag[i]);
			write_event(buf, True);
		}
	for(i = 0; i < ntag; i++) {
		if(!istag(newtag, nnewtag, tag[i])) {
			snprintf(buf, sizeof(buf), "RemoveTag %s\n", tag[i]);
			write_event(buf, True);
		}
		free(tag[i]);
	}

	free(tag);
	tag = newtag;
	ntag = nnewtag;
	tagsz = newtagsz;

	for(i = 0; nview && (i < nclient); i++) {
		for(j = 0; j < nview; j++) {
			View *v = view[j];
			if(j == sel)
				continue;
			organize_client(v, client[i]);
		}
		organize_client(view[sel], client[i]);
	}

	if(!nview && ntag)
		select_view(tag[0]);
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
