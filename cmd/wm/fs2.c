/* So, here's the begining of my idea of fs.c. Before you think it, yes, it's
 * still braindamaged. I'm certain that more simplicification is possible, but
 * my brain seems fried every time I try to work it out. Think of this a draft
 * in preparation for a draft. The basic idea here is to keep everything regarding
 * to directory structure in one place. Everything should be done in lookup_file,
 * including finding all files in a dir for a stat and finding files to walk to,
 * as well as associating a file with a pointer and id.
 *
 * I think that I could possibly save quite a few lines of code in lookup_file
 * with a function to create the FileId struct and push it onto the list. We'll see.
 * I need to sleep on it some more.
 *
 * Comments are, of course, welcome, but if you don't see where I'm going with this,
 * I don't know of how much help they'll be.
 *
 * This, btw, doesn't compile due to a lack of header files for cext_emallocz,
 * Client, lbar, etc.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Dirtab Dirtab;
struct Dirtab
{
	char		*name;
	unsigned int	type;
	unsigned int	perm;
};

#define nil ((void *)0)

enum { DMDIR, QTDIR, QTFILE };
enum {	FsRoot, FsDClient, FsDClients, FsDLBar,
	FsDRBar, FsDSClient, FsDTag, FsDTags,

	FsFBar, FsFCNorm, FsFCSel, FsFCctl,
	FsFCindex, FsFColRules, FsFEvent, FsFFont,
	FsFKeys, FsFRctl, FsFTagRules, FsFTctl,
	FsFTindex, FsFprops, RsFFont
};

Dirtab *dirtab[] =
{
[FsRoot]	(Dirtab [])
		{{".",		FsRoot,		0500|DMDIR },
		 {"rbar",	FsDRBar,	0700|DMDIR },
		 {"lbar",	FsDLBar,	0700|DMDIR },
		 {"client",	FsDClients,	0500|DMDIR },
		 {"tag",	FsDTags,	0500|DMDIR },
		 {"ctl",	FsFRctl,	0600 },
		 {"colrules",	FsFColRules,	0600 }, 
		 {"tagrules",	FsFTagRules,	0600 }, 
		 {"font",	FsFFont,	0600 },
		 {"keys",	FsFKeys,	0600 },
		 {"event",	FsFEvent,	0600 },
		 {"normcolors",	FsFCNorm,	0600 },
		 {"selcolors",	FsFCSel,	0600 },
		 {nil}},
[FsDRBar]	(Dirtab [])
		{{".",		FsDRBar,	0700|DMDIR },
		 {"",		FsFBar,		0600 },
		 {nil}},
[FsDLBar]	(Dirtab [])
		{{".",		FsDRBar,	0700|DMDIR },
		 {"",		FsFBar,		0600 },
		 {nil}},
[FsDClients]	(Dirtab [])
		{{".",		FsDClients,	0500|DMDIR },
		 {"",		FsDClient,	0500|DMDIR },
		 {nil}},
[FsDClient]	(Dirtab [])
		{{".",		FsDClient,	0500|DMDIR },
		 {"ctl",	FsFCctl,	0200 },
		 {"props",	FsFprops,	0400 },
		 {nil}},
[FsDSClient]	(Dirtab [])
		{{".",		FsDClient,	0500|DMDIR },
		 {"ctl",	FsFCctl,	0200 },
		 {"index",	FsFCindex,	0400 },
		 {"props",	FsFprops,	0400 },
		 {nil}},
[FsDTags]	(Dirtab [])
		{{".",		FsDTags,	0500|DMDIR },
		 {"",		FsDTag,		0500|DMDIR },
		 {nil}},
[FsDTag]	(Dirtab [])
		{{".",		FsDTag,		0500|DMDIR },
		 {"ctl",	FsFTctl,	0200 },
		 {"index",	FsFTindex,	0400 },
		 {nil}}
};

typedef struct FileId FileId;
struct FileId {
	FileId		*next;
	void		*ref;
	unsigned int	id;
	unsigned int	index;
	Dirtab		tab;
};

FileId *free_fileid = nil;

FileId *
get_file() {
	FileId *temp;
	if(!free_fileid) {
		unsigned int i = 15;
		temp = cext_emallocz(sizeof(FileId) * i);
		for(; i; i--) {
			temp->next = free_fileid;
			free_fileid = temp++;
		}
	}
	temp = free_fileid;
	free_fileid = temp->next;
	return temp;
}

void
free_file(FileId *f) {
	free(f->tab.name);
	f->next = free_fileid;
	free_fileid = f;
}

FileId *
lookup_file(FileId *parent, char *name)
{
	unsigned int i;

	if(!(parent->tab.perm & DMDIR))
		return nil;
	Dirtab *dir = dirtab[parent->tab.type];
	FileId *ret = nil, *temp, **last = &ret;

	for(; dir->name; dir++) {
		if(!name || !strcmp(name, dir->name)) {
			temp = get_file();
			*last = temp;
			last = &temp->next;
			temp->id = 0;
			temp->ref = nil;
			temp->tab = *dir;

			switch(temp->tab.type) {
			case FsDLBar:
				temp->ref = lbar;
				break;
			case FsDRBar:
				temp->ref = rbar;
				break;
			}
			if(name)
				break;
		}else
		if(!*dir->name) { /* strlen(dir->name) == 0 */
			switch(parent->tab.type) {
			case FsDClients:
				Client *c;
				if(!name || !strncmp(name, "sel", 4)) {
					if(sel && sel->sel && (c = sel->sel->sel->client)) {
						temp = get_file();
						*last = temp;
						last = &temp->next;
						temp->ref = c;
						temp->id = c.id;
						temp->index = idx_of_client(c);
						temp->tab = *dirtab[FsDSClient];
						temp->tab.name = strdup("sel");
					}
					if(name)
						break;
				}else{
					if(name) {
						i = (unsigned int)strtol(name, &name, 10);
						if(*name)
							continue;
					}

					for(c=client; c; c=c->next) {
						if(!name || c->index == i) {
							temp = get_file();
							*last = temp;
							last = &temp->next;
							temp->ref = c;
							temp->id = c.id;
							temp->tab = *dirtab[FsDClient];
							asprintf(temp->tab.name, "%d", c->index);
						}
						if(name)
							break;
					}
					if(name)
						goto End_This_Loop; /* label doesn't exist */
				}
			case FsDTags:
				/* and so forth */
			}
		}
	}
	*last = nil;
	return ret;
}
