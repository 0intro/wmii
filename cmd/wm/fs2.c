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
#include <time.h>

typedef struct Dirtab Dirtab;
struct Dirtab
{
	char		*name;
	unsigned char	qtype;
	unsigned int	type;
	unsigned int	perm;
};

#define nil ((void *)0)

#define QID(t, i) (((long long)((t)&0xFF)<<32)|((i)&0xFFFFFFFF))
#define TYPE(q) ((q)>>32&0xFF)
#define ID(q) ((q)&0xFFFFFFFF)

enum { DMDIR, DMAPPEND, QTDIR, QTFILE, QTAPPEND };
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
		{{".",		QTDIR,		FsRoot,		0500|DMDIR },
		 {"rbar",	QTDIR,		FsDRBar,	0700|DMDIR },
		 {"lbar",	QTDIR,		FsDLBar,	0700|DMDIR },
		 {"client",	QTDIR,		FsDClients,	0500|DMDIR },
		 {"tag",	QTDIR,		FsDTags,	0500|DMDIR },
		 {"ctl",	QTAPPEND,	FsFRctl,	0600|DMAPPEND },
		 {"colrules",	QTFILE,		FsFColRules,	0600 }, 
		 {"tagrules",	QTFILE,		FsFTagRules,	0600 }, 
		 {"font",	QTFILE,		FsFFont,	0600 },
		 {"keys",	QTFILE,		FsFKeys,	0600 },
		 {"event",	QTFILE,		FsFEvent,	0600 },
		 {"normcolors",	QTFILE,		FsFCNorm,	0600 },
		 {"selcolors",	QTFILE,		FsFCSel,	0600 },
		 {nil}},
[FsDRBar]	(Dirtab [])
		{{".",		QTDIR,		FsDRBar,	0700|DMDIR },
		 {"",		QTFILE,		FsFBar,		0600 },
		 {nil}},
[FsDLBar]	(Dirtab [])
		{{".",		QTDIR,		FsDRBar,	0700|DMDIR },
		 {"",		QTFILE,		FsFBar,		0600 },
		 {nil}},
[FsDClients]	(Dirtab [])
		{{".",		QTDIR,		FsDClients,	0500|DMDIR },
		 {"",		QTDIR,		FsDClient,	0500|DMDIR },
		 {nil}},
[FsDClient]	(Dirtab [])
		{{".",		QTFILE,		FsDClient,	0500|DMDIR },
		 {"ctl",	QTAPPEND,	FsFCctl,	0200|DMAPPEND },
		 {"props",	QTFILE,		FsFprops,	0400 },
		 {nil}},
[FsDSClient]	(Dirtab [])
		{{".",		QTDIR,		FsDClient,	0500|DMDIR },
		 {"ctl",	QTAPPEND,	FsFCctl,	0200|DMAPPEND },
		 {"index",	QTFILE,		FsFCindex,	0400 },
		 {"props",	QTFILE,		FsFprops,	0400 },
		 {nil}},
[FsDTags]	(Dirtab [])
		{{".",		QTDIR,		FsDTags,	0500|DMDIR },
		 {"",		QTDIR,		FsDTag,		0500|DMDIR },
		 {nil}},
[FsDTag]	(Dirtab [])
		{{".",		QTDIR,		FsDTag,		0500|DMDIR },
		 {"ctl",	QTAPPEND,	FsFTctl,	0200|DMAPPEND },
		 {"index",	QTFILE,		FsFTindex,	0400 },
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
			case FsFColRules:
				temp->ref = crules;
				break;
			case FsFTagRules:
				temp->ref = trules;
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
						temp->id = c->id;
						temp->index = idx_of_client(c);
						temp->tab = *dirtab[FsDSClient];
						temp->tab.name = strdup("sel");
					}
					if(name)
						goto LastItem;
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
							temp->id = c->id;
							temp->tab = *dirtab[FsDClient];
							asprintf(&temp->tab.name, "%d", c->index);
							if(name)
								goto LastItem;
						}
					}
					if(name)
						goto LastItem; /* label doesn't exist */
				}
			case FsDTags:
				View *v;
				if(!name || !strncmp(name, "sel", 4)) {
					if(sel) {
						temp = get_file();
						*last = temp;
						last = &temp->next;
						temp->ref = sel;
						temp->id = sel->id;
						temp->tab = *dirtab[FsDTag];
						temp->tab.name = strdup("sel");
					}
					if(name)
						goto LastItem;
				}else{
					for(v=view; v; v=v->next) {
						if(!name || !strcmp(name, v->name)) {
							temp = get_file();
							*last = temp;
							last = &temp->next;
							temp->rev = v;
							temp->id = v->id;
							temp->name = strdup(v->name);
							if(name)
								goto LastItem;
						}
					}
				}
			case FsDRBar:
			case FsDLBar:
				Bar *b;
				for(b=parent->ref; b; b=b->next) {
					if(!name || strcmp(name, b->name)) {
						temp = get_file();
						*last = temp;
						last = &temp->next;
						temp->ref = b;
						temp->id = b->id;
						temp->tab = dirtab[FsDRBar][1];
						temp->tab.name = strdup(bar->name);
					}
				}
			}
		}
	}
LastItem:
	*last = nil;
	return ret;
}

void
fs_walk(Req *r) {
	FileId *f = r->fid->aux, *nf, **fi;
	int i;

	for(i=0; i < r->ifcall.nwname; i++) {
		if(!strncmp(r->ifcall.wname[i], "..", 3)) {
			if(f->next) f=f->next;
		}else{
			nf = lookup_file(f, r->ircall.wname[i]);
			if(!nf)
				break;
			nf->next = f;
			f = nf;
		}
		r->ofcall.wqid[i] = QID(f->tab.type, f->id);
	}
	if(i < r->ifcall.nwname) {
		for(; f != r->fid->aux; f=nf) {
			nf=f->next;
			free_file(f);
		}
		respond(r, Enofile);
	}

	if(r->ifcall.fid != r->ifcall.newfid) {
		r->newfid.aux = f;
		for(fi=&r->newfid.aux; *fi != r->fid.aux; fi=&(*fi)->next);
		for(; *fi; fi=&(*fi)->next) {
			nf = get_file();
			*nf = **fi;
			*fi = nf;
		}
	}else
		r->fid.aux = f;

	r->fid.qid = r->ofcall.qid = r->ofcall.nwqid[i-1];
	r->ofcall.nwqid = i;
	r->fid.aux = f;
	respond(r);
}

/* All of this stat stuf is ugly. */
void
fs_stat(Req *r) {
	Stat s;
	int size = IXP_MAX_STAT;
	void *buf = cext_emallocz(size);
	r->ofcall.stat = buf;

	dostat(&s, 0, r->fid->aux);
	ixp_pack_stat(&buf, &size, &s);
	r->ofcall.nstat = IXP_MAX_STAT - size;
	respond(r);
}

void
fs_read(Req *r) {
	void *buf;
	unsigned int n, offset = 0;
	int size;
	FileId *f = r->fid->aux, *tf;

	if(f->tab.perm & DMDIR) {
		Stat s;
		offset = 0;
		size = r->ifcall->count;
		buf = cext_emallocz(size);
		f.ofcall.data = buf;

		f = lookup_file(f, nil);
		for(; f; f=f->next) {
			dostat(&s, 0, f);
			n = ixp_sizeof_stat(s);
			offset += n;
			if(offset >= f->ifcall.offset) {
				if(size < n)
					break;
				ixp_pack_stat(&buf, &size, &s);
			}
		}

		while((tf = f)) {
			f = f->next;
			free_file(tf);
		}

		f.ofcall.size = f.ifcall.size - size;
		respond(f, nil);
	}else{
	}
}

void
dostat(Stat *s, unsigned int len, FileId *f) {
	s->type = 0;
	s->dev = 0;
	s->qid.path = QID(f->tab.type, f->id);
	s->qid.vers = 0;
	s->qid.type = f->tab.qtype;
	s->mode = f->tab.perm;
	s->atime = clock;
	s->mtime = clock;
	s->length = len;
	s->name = f->tab.name;
	s->uid = user;
	s->gid = user;
	s->muid = user;
}
