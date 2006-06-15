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
	.remove=fs_remove,
	.freefid=fs_freefid
};

#define QID(t, i) (((long long)((t)&0xFF)<<32)|((i)&0xFFFFFFFF))
/* Will I ever need these macros?
 *  I don't think so. */
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
	unsigned short	nref;
};

static void dostat(Stat *s, unsigned int len, FileId *f);
FileId *free_fileid = nil;

/* ad-hoc file tree. Empty names ("") indicate a dynamic entry to be filled
 * in by lookup_file */
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
dirtabclients[]={{".",		QTDIR,		FsDClients,	0500|DMDIR },
		 {"",		QTDIR,		FsDClient,	0500|DMDIR },
		 {nil}},
dirtabclient[]= {{".",		QTDIR,		FsDClient,	0500|DMDIR },
		 {"ctl",	QTAPPEND,	FsFCctl,	0200|DMAPPEND },
		 {"props",	QTFILE,		FsFprops,	0400 },
		 {nil}},
dirtabsclient[]={{".",		QTDIR,		FsDSClient,	0500|DMDIR },
		 {"ctl",	QTAPPEND,	FsFCctl,	0200|DMAPPEND },
		 {"index",	QTFILE,		FsFCindex,	0400 },
		 {"props",	QTFILE,		FsFprops,	0400 },
		 {nil}},
dirtabbar[]=	{{".",		QTDIR,		FsDRBar,	0700|DMDIR },
		 {"",		QTFILE,		FsFBar,		0600 },
		 {nil}},
dirtabtags[]=	{{".",		QTDIR,		FsDTags,	0500|DMDIR },
		 {"",		QTDIR,		FsDTag,		0500|DMDIR },
		 {nil}},
dirtabtag[]=	{{".",		QTDIR,		FsDTag,		0500|DMDIR },
		 {"ctl",	QTAPPEND,	FsFTctl,	0200|DMAPPEND },
		 {"index",	QTFILE,		FsFTindex,	0400 },
		 {nil}};
/* Writing the lists separately and using an array of their references
 * removes the need for casting and allows for C90 conformance,
 * since otherwise we would need to use compound literals */
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

/* get_file/free_file save and reuse old FileId structs
 * since so many of them are needed for so many
 * purposes */
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
	temp->nref = 1;
	return temp;
}

/* Convenience func: */
/* ugly, though... */
FileId *
push_file(FileId ***last) {
	FileId *ret = get_file();
	**last = ret;
	*last = &ret->next;
	return ret;
}

static void
free_file(FileId *f) {
	if(--f->nref)
		return;
	free(f->tab.name);
	f->next = free_fileid;
	free_fileid = f;
}

static void
clone_files(FileId *f) {
	for(; f; f=f->next)
		f->nref++;
}

/* All lookups and directory organization should be performed through
 * lookup_file, mostly through the dirtabs[] tree. */
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
	FileId *ret = nil, *file, **last = &ret;

	for(; dir->name; dir++) {
		/* Dynamic dirs */
		if(!*dir->name) { /* strlen(dir->name) == 0 */
			switch(parent->tab.type) {
			case FsDClients:
				if(!name || !strncmp(name, "sel", 4)) {
					if((c = sel_client())) {
						file = push_file(&last);
						file->ref = c;
						file->id = c->id;
						file->index = idx_of_client(c);
						file->tab = *dirtab[FsDSClient];
						file->tab.name = strdup("sel");
					}if(name) goto LastItem;
				}
				if(name) {
					id = (unsigned int)strtol(name, &name, 10);
					if(*name) goto NextItem;
				}

				i=0;
				for(c=client; c; c=c->next, i++) {
					if(!name || i == id) {
						file = push_file(&last);
						file->ref = c;
						file->id = c->id;
						file->tab = *dir;
						asprintf(&file->tab.name, "%d", i);
						if(name) goto LastItem;
					}
				}
				break;
			case FsDTags:
				if(!name || !strncmp(name, "sel", 4)) {
					if(sel) {
						file = push_file(&last);
						file->ref = sel;
						file->id = sel->id;
						file->tab = *dir;
						file->tab.name = strdup("sel");
					}if(name) goto LastItem;
				}
				for(v=view; v; v=v->next) {
					if(!name || !strcmp(name, v->name)) {
						file = push_file(&last);
						file->ref = v;
						file->id = v->id;
						file->tab = *dir;
						file->tab.name = strdup(v->name);
						if(name) goto LastItem;
					}
				}
				break;
			case FsDRBar:
			case FsDLBar:
				for(b=parent->ref; b; b=b->next) {
					if(!name || !strcmp(name, b->name)) {
						file = push_file(&last);
						file->ref = b;
						file->id = b->id;
						file->tab = *dir;
						file->tab.name = strdup(b->name);
						if(name) goto LastItem;
					}
				}
				break;
			}
		}else /* Static dirs */
		if(!name || !strcmp(name, dir->name)) {
			file = push_file(&last);
			file->id = 0;
			file->ref = nil;
			file->tab = *dir;

			/* Special considerations: */
			switch(file->tab.type) {
			case FsDLBar:
				file->ref = lbar;
				break;
			case FsDRBar:
				file->ref = rbar;
				break;
			case FsFColRules:
				file->ref = vrule;
				break;
			case FsFTagRules:
				file->ref = trule;
				break;
			}
			if(name) goto LastItem;
		}
	NextItem:
		continue;
	}
LastItem:
	*last = nil;
	return ret;
}

void
fs_walk(Req *r) {
	FileId *f = r->fid->aux, *nf;
	int i;

	clone_files(f);
	for(i=0; i < r->ifcall.nwname; i++) {
		if(!strncmp(r->ifcall.wname[i], "..", 3)) {
			if(f->next) {
				nf=f;
				f=f->next;
				free_file(nf);
			}
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
	/* XXX: This will not be necessary once a free_fid
	 * function is implemented */
	if(i < r->ifcall.nwname) {
		while((nf = f)) {
			f=f->next;
			free_file(nf);
		}
		return respond(r, Enofile);
	}

	/* Remove refs for r->fid if no new fid */
	/* If Fids were ref counted, this could be
	 * done in their decref function */
	if(r->ifcall.fid == r->ifcall.newfid) {
		nf=r->fid->aux;
		r->fid->aux = f;
		for(; nf; nf=f) {
			f = nf->next;
			free_file(nf);
		}
	}

	r->newfid->aux = f;
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

/* This should probably be factored out like lookup_file
 * so we can use it to get size for stats and not write
 * data anywhere. -KM */
/* This is obviously not a priority, however. -KM */
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

		tf = f = lookup_file(f, nil);
		/* Note: f->tab.name == "."; goto next */
		for(f=f->next; f; f=f->next) {
			dostat(&s, 0, f);
			n = ixp_sizeof_stat(&s);
			if(offset >= r->ifcall.offset) {
				if(size < n)
					break;
				ixp_pack_stat(&buf, &size, &s);
			}
			offset += n;
		}

		while((f = tf)) {
			tf = tf->next;
			free_file(f);
		}

		r->ofcall.count = r->ifcall.count - size;
		respond(r, nil);
	}else{
		/* Read normal files */
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

/* fs_* functions below here are yet to be properly implemented */

void
fs_open(Req *r) {
	if(!r->ifcall.mode == OREAD)
		respond(r, Enoperm);
	r->fid->omode = OREAD;
	respond(r, nil);
}

void
fs_freefid(Fid *f) {
	FileId *id = f->aux, *tid;
	while((tid = id)) {
		id = id->next;
		free_file(tid);
	}
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
fs_create(Req *r) {
	respond(r, "not implemented");
}

/* XXX: Shuts up the linker, but is yet to be written */
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
