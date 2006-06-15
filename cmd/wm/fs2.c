#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "wm.h"
P9Srv p9srv = {
	.open=	fs_open,
	.walk=	fs_walk,
	.read=	fs_read,
	.stat=	fs_stat,
	.write=	fs_write,
	.attach=fs_attach,
	.create=fs_create,
	.remove=fs_remove
};

#define QID(t, i) (((long long)((t)&0xFF)<<32)|((i)&0xFFFFFFFF))
#define TYPE(q) ((q)>>32&0xFF)
#define ID(q) ((q)&0xFFFFFFFF)

static char Enoperm[] = "permission denied";
static char Enofile[] = "file not found";
//static char Efidinuse[] = "fid in use";
//static char Enomode[] = "mode not supported";
//static char Enofunc[] = "function not supported";
//static char Enocommand[] = "command not supported";
//static char Ebadvalue[] = "bad value";

enum {	FsRoot, FsDClient, FsDClients, FsDLBar,
	FsDRBar, FsDSClient, FsDTag, FsDTags,

	FsFBar, FsFCNorm, FsFCSel, FsFCctl,
	FsFCindex, FsFColRules, FsFEvent, FsFFont,
	FsFKeys, FsFRctl, FsFTagRules, FsFTctl,
	FsFTindex, FsFprops, RsFFont
};

typedef struct Dirtab Dirtab;
struct Dirtab
{
	char		*name;
	unsigned char	qtype;
	unsigned int	type;
	unsigned int	perm;
};


typedef struct FileId FileId;
struct FileId {
	FileId		*next;
	void		*ref;
	unsigned int	id;
	unsigned int	index;
	Dirtab		tab;
};

static void dostat(Stat *s, unsigned int len, FileId *f);
FileId *free_fileid = nil;

static Dirtab
dirtabroot[]=	{{".",		QTDIR,		FsRoot,		0500|DMDIR },
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
dirtabclient[]= {{".",		QTFILE,		FsDClient,	0500|DMDIR },
		 {"ctl",	QTAPPEND,	FsFCctl,	0200|DMAPPEND },
		 {"props",	QTFILE,		FsFprops,	0400 },
		 {nil}},
dirtabsclient[]={{".",		QTDIR,		FsDClient,	0500|DMDIR },
		 {"ctl",	QTAPPEND,	FsFCctl,	0200|DMAPPEND },
		 {"index",	QTFILE,		FsFCindex,	0400 },
		 {"props",	QTFILE,		FsFprops,	0400 },
		 {nil}},
dirtabbar[]=	{{".",		QTDIR,		FsDRBar,	0700|DMDIR },
		 {"",		QTFILE,		FsFBar,		0600 },
		 {nil}},
dirtabclients[]={{".",		QTDIR,		FsDClients,	0500|DMDIR },
		 {"",		QTDIR,		FsDClient,	0500|DMDIR },
		 {nil}},
dirtabtags[]=	{{".",		QTDIR,		FsDTags,	0500|DMDIR },
		 {"",		QTDIR,		FsDTag,		0500|DMDIR },
		 {nil}},
dirtabtag[]=	{{".",		QTDIR,		FsDTag,		0500|DMDIR },
		 {"ctl",	QTAPPEND,	FsFTctl,	0200|DMAPPEND },
		 {"index",	QTFILE,		FsFTindex,	0400 },
		 {nil}};
static Dirtab *dirtab[] = {
	[FsRoot]	dirtabroot,
	[FsDRBar]	dirtabbar,
	[FsDLBar]	dirtabbar,
	[FsDClients]	dirtabclients,
	[FsDClient]	dirtabclient,
	[FsDSClient]	dirtabsclient,
	[FsDTags]	dirtabtags,
	[FsDTag]	dirtabtag
};

static FileId *
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

static void
free_file(FileId *f) {
	free(f->tab.name);
	f->next = free_fileid;
	free_fileid = f;
}

static FileId *
lookup_file(FileId *parent, char *name)
{
	unsigned int i, id;
	Client *c;
	View *v;
	Bar *b;

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
				temp->ref = vrule;
				break;
			case FsFTagRules:
				temp->ref = trule;
				break;
			}
			if(name)
				break;
		}else
		if(!*dir->name) { /* strlen(dir->name) == 0 */
			switch(parent->tab.type) {
			case FsDClients:
				if(!name || !strncmp(name, "sel", 4)) {
					if((c = sel_client())) {
						temp = get_file();
						*last = temp;
						last = &temp->next;
						temp->ref = c;
						temp->id = c->id;
						temp->index = idx_of_client(c);
						temp->tab = *dirtab[FsDSClient];
						temp->tab.name = strdup("sel");
					}
				}else{
					if(name) {
						id = (unsigned int)strtol(name, &name, 10);
						if(*name)
							continue;
					}

					for(c=client; c; c=c->next) {
						if(name && c->id != id)
							continue;
						temp = get_file();
						*last = temp;
						last = &temp->next;
						temp->ref = c;
						temp->id = c->id;
						temp->tab = *dirtab[FsDClient];
						asprintf(&temp->tab.name, "%d", i);
						if(name)
							goto LastItem;
					}
				}
			case FsDTags:
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
				}else{
					for(v=view; v; v=v->next) {
						if(name && strcmp(name, v->name))
							continue;
						temp = get_file();
						*last = temp;
						last = &temp->next;
						temp->ref = v;
						temp->id = v->id;
						temp->tab.name = strdup(v->name);
					}
				}
			case FsDRBar:
			case FsDLBar:
				for(b=parent->ref; b; b=b->next) {
					if(!name || strcmp(name, b->name)) {
						temp = get_file();
						*last = temp;
						last = &temp->next;
						temp->ref = b;
						temp->id = b->id;
						temp->tab = dirtab[FsDRBar][1];
						temp->tab.name = strdup(b->name);
					}
				}
			}
			if(name)
				goto LastItem;
		}
	}
LastItem:
	*last = nil;
	return ret;
}

/* XXX This leaks FileIds */
void
fs_walk(Req *r) {
	FileId *f = r->fid->aux, *nf, **fi;
	int i;

	for(i=0; i < r->ifcall.nwname; i++) {
		if(!strncmp(r->ifcall.wname[i], "..", 3)) {
			if(f->next) f=f->next;
		}else{
			nf = lookup_file(f, r->ifcall.wname[i]);
			if(!nf)
				break;
			nf->next = f;
			f = nf;
		}
		r->ofcall.wqid[i].type = f->tab.qtype;
		r->ofcall.wqid[i].path = QID(f->tab.type, f->id);
	}
	if(i < r->ifcall.nwname) {
		for(; f != r->fid->aux; f=nf) {
			nf=f->next;
			free_file(f);
		}
		respond(r, Enofile);
	}

	r->newfid->aux = f;
	if(r->ifcall.fid != r->ifcall.newfid) {
		for(fi=(void *)&r->newfid->aux;
		    *fi != r->fid->aux;
		    fi=&(*fi)->next);
		for(; *fi; fi=&(*fi)->next) {
			nf = get_file();
			*nf = **fi;
			*fi = nf;
		}
	}

	if(r->ifcall.nwname == 0)
		r->newfid->qid = r->fid->qid;
	else
		r->newfid->qid = r->ofcall.wqid[i-1];
	r->ofcall.nwqid = i;
	respond(r, nil);
}

/* All of this stat stuf is ugly. */
void
fs_stat(Req *r) {
	Stat s;
	int size;
	unsigned char *buf;

	dostat(&s, 0, r->fid->aux);
	r->ofcall.nstat = size = ixp_sizeof_stat(&s);
	buf = cext_emallocz(size);
	r->ofcall.stat = buf;

	ixp_pack_stat(&buf, &size, &s);
	respond(r, nil);
}

void
fs_read(Req *r) {
	unsigned char *buf;
	unsigned int n, offset = 0;
	int size;
	FileId *f = r->fid->aux, *tf;

	if(f->tab.perm & DMDIR) {
		Stat s;
		offset = 0;
		size = r->ifcall.count;
		buf = cext_emallocz(size);
		r->ofcall.data = buf;

		f = lookup_file(f, nil);
		/* f->tab.name == "."; goto next */
		for(f=f->next; f; f=f->next) {
			dostat(&s, 0, f);
			n = ixp_sizeof_stat(&s);
			offset += n;
			if(offset >= r->ifcall.offset) {
				if(size < n)
					break;
				ixp_pack_stat(&buf, &size, &s);
			}
		}

		while((tf = f)) {
			f = f->next;
			free_file(tf);
		}

		r->ofcall.count = r->ifcall.count - size;
		respond(r, nil);
	}else{
	}
}

void
fs_attach(Req *r) {
	FileId *f = cext_emallocz(sizeof(FileId));
	f->tab = dirtab[FsRoot][0];
	f->tab.name = strdup("/");
	r->fid->aux = f;
	r->fid->qid.type = f->tab.qtype;
	r->fid->qid.path = QID(f->tab.type, 0);
	r->ofcall.qid = r->fid->qid;
	respond(r, nil);
}

void
fs_remove(Req *r) {
	respond(r, "not implemented");
}

void
fs_write(Req *r) {
	respond(r, "not implemented");
}

void
fs_open(Req *r) {
	if(!r->ifcall.mode == OREAD)
		respond(r, Enoperm);
	r->fid->omode = OREAD;
	respond(r, nil);
}

void
fs_create(Req *r) {
	respond(r, "not implemented");
}

void
write_event(char *buf) {
	return;
}

static void
dostat(Stat *s, unsigned int len, FileId *f) {
	s->type = 0;
	s->dev = 0;
	s->qid.path = QID(f->tab.type, f->id);
	s->qid.version = 0;
	s->qid.type = f->tab.qtype;
	s->mode = f->tab.perm;
	s->atime = time(nil);
	s->mtime = time(nil);
	s->length = len;
	s->name = f->tab.name;
	/* XXX This genenv should be called once */
	s->uid = getenv("USER");
	s->gid = getenv("USER");
	s->muid = getenv("USER");
}
