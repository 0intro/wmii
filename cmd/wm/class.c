/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <string.h>
#include <stdlib.h>

#include "wm.h"

TClass *
alloc_class(char *name, char *tags)
{
	TClass *tc;
	char buf[256];
	char *p;
	static unsigned int id = 1;

 	cext_strlcpy(buf, name, sizeof(buf));	
	if(!(p = strchr(name, ':')))
		return nil;

	tc = cext_emallocz(sizeof(TClass));
	tc->id = id++;
	cext_strlcpy(tc->class, buf, sizeof(tc->class));
	cext_strlcpy(tc->instance, p, sizeof(tc->instance));
	cext_strlcpy(tc->tags, tags, sizeof(tc->tags));

	class = (TClass **)cext_array_attach((void **)class, tc, sizeof(TClass *), &classsz);
	nclass++;

	return tc;
}

void
destroy_class(TClass *tclass)
{
	cext_array_detach((void **)class, tclass, &classsz);
	nclass--;
	free(tclass);
}

int
classid2index(unsigned short id)
{
	int i;
	for(i = 0; i < nclass; i++)
		if(class[i]->id == id)
			return i;
	return -1;
}

static TClass *
classinst2class(char *sclass, char *sinst)
{
	unsigned int i;
	for(i = 0; i < nclass; i++) {
		TClass *tc = class[i];
		if((!strncmp(tc->class, sclass, sizeof(tc->class)) ||
			!strncmp(tc->class, "*", sizeof(tc->class)))
				&&
		   (!strncmp(tc->instance, sinst, sizeof(tc->instance)) ||
			!strncmp(tc->instance, "*", sizeof(tc->instance))))
				return tc;
	}
	return nil;
}

TClass *
name2class(char *name)
{
	char buf[256];
	char *p;

 	cext_strlcpy(buf, name, sizeof(buf));	
	if(!(p = strchr(name, ':')))
		return nil;

	*p = 0;
	p++;
	return classinst2class(buf, p);
}

TClass *
client2class(Client *c)
{
	return classinst2class(c->class, c->instance);
}
