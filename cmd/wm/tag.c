/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include <string.h>

#include "wm.h"

void
update_tags(Client *c)
{
	unsigned int i;

	if(c) {
		char buf[256];
		char *toks[16];
		unsigned int j, n;
		Bool match;

		fprintf(stderr, "tags: %s\n", c->tags);
		cext_strlcpy(buf, c->tags, sizeof(buf));
		n = cext_tokenize(toks, 16, buf, '+');

		for(i = 0; i < n; i++) {
			View *v = get_view(toks[i]);
			if(!clientofview(v, c))
				attach_toview(v, c);
		}

		for(i = 0; i < c->view.size; i++) {
			View *v = c->view.data[i];
			match = False;
			for(j = 0; j < n; j++) {
				if(!strncmp(v->name, toks[j], sizeof(v->name)))
					match = True;
			}
			if(!match)
				detach_fromview(v, c);
		}
	}

	for(i = 0; i < view.size; i++)
		if(!hasclient(view.data[i]))
			destroy_view(view.data[i]);

	if(view.size)
		focus_view(view.data[sel]);
	else
		update_bar_tags();
}

void
retag()
{
	unsigned int i;
	for(i = 0; i < client.size; i++) {
		match_tags(client.data[i], False);
		update_tags(client.data[i]);
	}
}

