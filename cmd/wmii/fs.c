/* Copyright ©2006-2008 Kris Maglione <fbsdaemon at gmail dot com>
 * See LICENSE file for license details.
 */
#include "dat.h"
#include <ctype.h>
#include <stdarg.h>
#include <time.h>
#include "fns.h"


/* Datatypes: */
typedef struct Dirtab Dirtab;
typedef struct FileId FileId;
typedef struct PLink PLink;
typedef struct Pending Pending;
typedef struct RLink RLink;
typedef struct Queue Queue;

struct PLink {
	PLink*		next;
	PLink*		prev;
	IxpFid*		fid;
	Queue*		queue;
	Pending*	pending;
};

struct RLink {
	RLink*		next;
	RLink*		prev;
	Ixp9Req*	req;
};

struct Pending {
	RLink	req;
	PLink	fids;
};

struct Queue {
	Queue*	link;
	char*	dat;
	long	len;
};

static Pending	events;
static Pending	pdebug[NDebugOpt];

struct Dirtab {
	char*	name;
	uchar	qtype;
	uint	type;
	uint	perm;
	uint	flags;
};

struct FileId {
	FileId*	next;
	union {
		Bar*		bar;
		Bar**		bar_p;
		CTuple*		col;
		Client*		client;
		PLink*		p;
		Ruleset*	rule;
		View*		view;
		char*		buf;
		void*		ref;
	} p;
	bool	pending;
	uint	id;
	uint	index;
	Dirtab	tab;
	ushort	nref;
	uchar	volatil;
};

enum {
	FLHide = 1,
};

/* Constants */
enum {	/* Dirs */
	FsDBars,
	FsDClient,
	FsDClients,
	FsDDebug,
	FsDTag,
	FsDTags,
	FsRoot,
	/* Files */
	FsFBar,
	FsFCctl,
	FsFClabel,
	FsFColRules,
	FsFCtags,
	FsFDebug,
	FsFEvent,
	FsFKeys,
	FsFRctl,
	FsFTagRules,
	FsFTctl,
	FsFTindex,
	FsFprops,
};

/* Error messages */
static char
	Enoperm[] = "permission denied",
	Enofile[] = "file not found",
	Ebadvalue[] = "bad value",
	Einterrupted[] = "interrupted";

/* Macros */
#define QID(t, i) (((vlong)((t)&0xFF)<<32)|((i)&0xFFFFFFFF))

/* Global Vars */
/***************/
FileId *free_fileid;

Ixp9Srv p9srv = {
	.open=	fs_open,
	.walk=	fs_walk,
	.read=	fs_read,
	.stat=	fs_stat,
	.write=	fs_write,
	.clunk=	fs_clunk,
	.flush=	fs_flush,
	.attach=fs_attach,
	.create=fs_create,
	.remove=fs_remove,
	.freefid=fs_freefid
};

/* ad-hoc file tree. Empty names ("") indicate dynamic entries to be filled
 * in by lookup_file 
 */
static Dirtab
dirtab_root[]=	 {{".",		QTDIR,		FsRoot,		0500|DMDIR },
		  {"rbar",	QTDIR,		FsDBars,	0700|DMDIR },
		  {"lbar",	QTDIR,		FsDBars,	0700|DMDIR },
		  {"debug",	QTDIR,		FsDDebug,	0500|DMDIR, FLHide },
		  {"client",	QTDIR,		FsDClients,	0500|DMDIR },
		  {"tag",	QTDIR,		FsDTags,	0500|DMDIR },
		  {"ctl",	QTAPPEND,	FsFRctl,	0600|DMAPPEND },
		  {"colrules",	QTFILE,		FsFColRules,	0600 }, 
		  {"event",	QTFILE,		FsFEvent,	0600 },
		  {"keys",	QTFILE,		FsFKeys,	0600 },
		  {"tagrules",	QTFILE,		FsFTagRules,	0600 }, 
		  {nil}},
dirtab_clients[]={{".",		QTDIR,		FsDClients,	0500|DMDIR },
		  {"",		QTDIR,		FsDClient,	0500|DMDIR },
		  {nil}},
dirtab_client[]= {{".",		QTDIR,		FsDClient,	0500|DMDIR },
		  {"ctl",	QTAPPEND,	FsFCctl,	0600|DMAPPEND },
		  {"label",	QTFILE,		FsFClabel,	0600 },
		  {"tags",	QTFILE,		FsFCtags,	0600 },
		  {"props",	QTFILE,		FsFprops,	0400 },
		  {nil}},
dirtab_debug[]=  {{".",		QTDIR,		FsDDebug,	0500|DMDIR, FLHide },
		  {"",		QTFILE,		FsFDebug,	0400 },
		  {nil}},
dirtab_bars[]=	 {{".",		QTDIR,		FsDBars,	0700|DMDIR },
		  {"",		QTFILE,		FsFBar,		0600 },
		  {nil}},
dirtab_tags[]=	 {{".",		QTDIR,		FsDTags,	0500|DMDIR },
		  {"",		QTDIR,		FsDTag,		0500|DMDIR },
		  {nil}},
dirtab_tag[]=	 {{".",		QTDIR,		FsDTag,		0500|DMDIR },
		  {"ctl",	QTAPPEND,	FsFTctl,	0600|DMAPPEND },
		  {"index",	QTFILE,		FsFTindex,	0400 },
		  {nil}};
static Dirtab* dirtab[] = {
	[FsRoot] = dirtab_root,
	[FsDBars] = dirtab_bars,
	[FsDClients] = dirtab_clients,
	[FsDClient] = dirtab_client,
	[FsDDebug] = dirtab_debug,
	[FsDTags] = dirtab_tags,
	[FsDTag] = dirtab_tag,
};

/* Utility Functions */
static FileId *
get_file(void) {
	FileId *temp;
	if(!free_fileid) {
		uint i = 15;
		temp = emallocz(sizeof(FileId) * i);
		for(; i; i--) {
			temp->next = free_fileid;
			free_fileid = temp++;
		}
	}
	temp = free_fileid;
	free_fileid = temp->next;
	temp->volatil = 0;
	temp->nref = 1;
	temp->next = nil;
	temp->pending = false;
	return temp;
}

static void
free_file(FileId *f) {
	if(--f->nref)
		return;
	free(f->tab.name);
	f->next = free_fileid;
	free_fileid = f;
}

/* Increase the reference counts of the FileId list */
static void
clone_files(FileId *f) {
	for(; f; f=f->next)
		assert(f->nref++);
}

/* This should be moved to libixp */
static void
write_buf(Ixp9Req *r, char *buf, uint len) {
	if(r->ifcall.offset >= len)
		return;

	len -= r->ifcall.offset;
	if(len > r->ifcall.count)
		len = r->ifcall.count;
	r->ofcall.data = emalloc(len);
	memcpy(r->ofcall.data, buf + r->ifcall.offset, len);
	r->ofcall.count = len;
}

/* This should be moved to libixp */
static void
write_to_buf(Ixp9Req *r, char **buf, uint *len, uint max) {
	FileId *f;
	char *p;
	uint offset, count;

	f = r->fid->aux;

	offset = r->ifcall.offset;
	if(f->tab.perm & DMAPPEND)
		offset = *len;

	if(offset > *len || r->ifcall.count == 0) {
		r->ofcall.count = 0;
		return;
	}

	count = r->ifcall.count;
	if(max && (offset + count > max))
		count = max - offset;

	*len = offset + count;
	if(max == 0)
		*buf = erealloc(*buf, *len + 1);
	p = *buf;

	memcpy(p+offset, r->ifcall.data, count);
	r->ofcall.count = count;
	p[offset+count] = '\0';
}

/* This should be moved to libixp */
static void
data_to_cstring(Ixp9Req *r) {
	char *p;
	uint i;

	i = r->ifcall.count;
	p = r->ifcall.data;
	if(p[i - 1] == '\n')
		i--;

	r->ifcall.data = toutf8n(p, i);
	assert(r->ifcall.data);
	free(p);
}

typedef char* (*MsgFunc)(void*, IxpMsg*);

static char*
message(Ixp9Req *r, MsgFunc fn) {
	char *err, *s, *p, c;
	FileId *f;
	IxpMsg m;

	f = r->fid->aux;

	data_to_cstring(r);
	s = r->ifcall.data;

	err = nil;
	c = *s;
	while(c != '\0') {
		while(*s == '\n')
			s++;
		p = s;
		while(*p != '\0' && *p != '\n')
			p++;
		c = *p;
		*p = '\0';

		m = ixp_message(s, p-s, 0);
		s = fn(f->p.ref, &m);
		if(s)
			err = s;
		s = p + 1;
	}
	return err;
}

/* FIXME: Move to external lib */
static void
pending_respond(Ixp9Req *r) {
	FileId *f;
	PLink *p;
	RLink *rl;
	Queue *q;

	f = r->fid->aux;
	p = f->p.p;
	assert(f->pending);
	if(p->queue) {
		q = p->queue;
		p->queue = q->link;
		r->ofcall.data = q->dat;
		r->ofcall.count = q->len;
		if(r->aux) {
			rl = r->aux;
			rl->next->prev = rl->prev;
			rl->prev->next = rl->next;
			free(rl);
		}
		respond(r, nil);
		free(q);
	}else {
		rl = emallocz(sizeof *rl);
		rl->req = r;
		rl->next = &p->pending->req;
		rl->prev = rl->next->prev;
		rl->next->prev = rl;
		rl->prev->next = rl;
		r->aux = rl;
	}
}

static void
pending_write(Pending *p, char *dat, long n) {
	RLink rl;
	Queue **qp, *q;
	PLink *pp;
	RLink *rp;

	if(n == 0)
		return;

	if(p->req.next == nil) {
		p->req.next = &p->req;
		p->req.prev = &p->req;
		p->fids.prev = &p->fids;
		p->fids.next = &p->fids;
	}

	for(pp=p->fids.next; pp != &p->fids; pp=pp->next) {
		for(qp=&pp->queue; *qp; qp=&qp[0]->link)
			;
		q = emallocz(sizeof *q);
		q->dat = emalloc(n);
		memcpy(q->dat, dat, n);
		q->len = n;
		*qp = q;
	}
	rl.next = &rl;
	rl.prev = &rl;
	if(p->req.next != &p->req) {
		rl.next = p->req.next;
		rl.prev = p->req.prev;
		p->req.prev = &p->req;
		p->req.next = &p->req;
	}
	rl.prev->next = &rl;
	rl.next->prev = &rl;
	while((rp = rl.next) != &rl)
		pending_respond(rp->req);
}

static void
pending_pushfid(Pending *p, IxpFid *f) {
	PLink *pl;
	FileId *fi;

	if(p->req.next == nil) {
		p->req.next = &p->req;
		p->req.prev = &p->req;
		p->fids.prev = &p->fids;
		p->fids.next = &p->fids;
	}

	fi = f->aux;
	pl = emallocz(sizeof *pl);
	pl->fid = f;
	pl->pending = p;
	pl->next = &p->fids;
	pl->prev = pl->next->prev;
	pl->next->prev = pl;
	pl->prev->next = pl;
	fi->pending = true;
	fi->p.p = pl;
}

void
event(const char *format, ...) {
	va_list ap;

	va_start(ap, format);
	vsnprint(buffer, sizeof(buffer), format, ap);
	va_end(ap);

	pending_write(&events, buffer, strlen(buffer));
}

static int dflags;
bool
setdebug(int flag) {
	dflags = flag;
	return true;
}

void
vdebug(int flag, const char *fmt, va_list ap) {
	char *s;
	int i, len;

	if(flag == 0)
		flag = dflags;

	s = vsmprint(fmt, ap);
	len = strlen(s);

	if(debugflag&flag)
		print("%s", s);
	if(debugfile&flag)
	for(i=0; i < nelem(pdebug); i++)
		if(flag & (1<<i))
			pending_write(pdebug+i, s, len);
	free(s);
}

void
debug(int flag, const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	vdebug(flag, fmt, ap);
	va_end(ap);
}

void
dprint(const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	vdebug(0, fmt, ap);
	va_end(ap);
}

static void
dostat(Stat *s, uint len, FileId *f) {
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
	s->uid = user;
	s->gid = user;
	s->muid = user;
}

/* All lookups and directory organization should be performed through
 * lookup_file, mostly through the dirtabs[] tree. */
static FileId *
lookup_file(FileId *parent, char *name)
{
	FileId *ret, *file, **last;
	Dirtab *dir;
	Client *c;
	View *v;
	Bar *b;
	uint id;
	int i;

	if(!(parent->tab.perm & DMDIR))
		return nil;
	dir = dirtab[parent->tab.type];
	last = &ret;
	ret = nil;
	for(; dir->name; dir++) {
		/* Dynamic dirs */
		if(dir->name[0] == '\0') {
			switch(parent->tab.type) {
			case FsDClients:
				if(!name || !strcmp(name, "sel")) {
					if((c = selclient())) {
						file = get_file();
						*last = file;
						last = &file->next;
						file->volatil = true;
						file->p.client = c;
						file->id = c->w.w;
						file->index = c->w.w;
						file->tab = *dir;
						file->tab.name = estrdup("sel");
					}if(name) goto LastItem;
				}
				SET(id);
				if(name) {
					id = (uint)strtol(name, &name, 16);
					if(*name) goto NextItem;
				}
				for(c=client; c; c=c->next) {
					if(!name || c->w.w == id) {
						file = get_file();
						*last = file;
						last = &file->next;
						file->volatil = true;
						file->p.client = c;
						file->id = c->w.w;
						file->index = c->w.w;
						file->tab = *dir;
						file->tab.name = smprint("%C", c);
						assert(file->tab.name);
						if(name) goto LastItem;
					}
				}
				break;
			case FsDDebug:
				for(i=0; i < nelem(pdebug); i++)
					if(!name || !strcmp(name, debugtab[i])) {
						file = get_file();
						*last = file;
						last = &file->next;
						file->id = i;
						file->tab = *dir;
						file->tab.name = estrdup(debugtab[i]);
						if(name) goto LastItem;
					}
				break;
			case FsDTags:
				if(!name || !strcmp(name, "sel")) {
					if(screen->sel) {
						file = get_file();
						*last = file;
						last = &file->next;
						file->volatil = true;
						file->p.view = screen->sel;
						file->id = screen->sel->id;
						file->tab = *dir;
						file->tab.name = estrdup("sel");
					}if(name) goto LastItem;
				}
				for(v=view; v; v=v->next) {
					if(!name || !strcmp(name, v->name)) {
						file = get_file();
						*last = file;
						last = &file->next;
						file->volatil = true;
						file->p.view = v;
						file->id = v->id;
						file->tab = *dir;
						file->tab.name = estrdup(v->name);
						if(name) goto LastItem;
					}
				}
				break;
			case FsDBars:
				for(b=*parent->p.bar_p; b; b=b->next) {
					if(!name || !strcmp(name, b->name)) {
						file = get_file();
						*last = file;
						last = &file->next;
						file->volatil = true;
						file->p.bar = b;
						file->id = b->id;
						file->tab = *dir;
						file->tab.name = estrdup(b->name);
						if(name) goto LastItem;
					}
				}
				break;
			}
		}else /* Static dirs */
		if(!name && !(dir->flags & FLHide) || name && !strcmp(name, dir->name)) {
			file = get_file();
			*last = file;
			last = &file->next;
			file->id = 0;
			file->p.ref = parent->p.ref;
			file->index = parent->index;
			file->tab = *dir;
			file->tab.name = estrdup(file->tab.name);
			/* Special considerations: */
			switch(file->tab.type) {
			case FsDBars:
				if(!strcmp(file->tab.name, "lbar"))
					file->p.bar_p = &screen[0].bar[BLeft];
				else
					file->p.bar_p = &screen[0].bar[BRight];
				break;
			case FsFColRules:
				file->p.rule = &def.colrules;
				break;
			case FsFTagRules:
				file->p.rule = &def.tagrules;
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

static bool
verify_file(FileId *f) {
	FileId *nf;
	int ret;

	if(!f->next)
		return true;

	ret = false;
	if(verify_file(f->next)) {
		nf = lookup_file(f->next, f->tab.name);
		if(nf) {
			if(!nf->volatil || nf->p.ref == f->p.ref)
				ret = true;
			free_file(nf);
		}
	}
	return ret;
}

/* Service Functions */
void
fs_attach(Ixp9Req *r) {
	FileId *f = get_file();
	f->tab = dirtab[FsRoot][0];
	f->tab.name = estrdup("/");
	f->p.ref = nil;
	r->fid->aux = f;
	r->fid->qid.type = f->tab.qtype;
	r->fid->qid.path = QID(f->tab.type, 0);
	r->ofcall.qid = r->fid->qid;
	respond(r, nil);
}

void
fs_walk(Ixp9Req *r) {
	FileId *f, *nf;
	int i;

	f = r->fid->aux;
	clone_files(f);
	for(i=0; i < r->ifcall.nwname; i++) {
		if(!strcmp(r->ifcall.wname[i], "..")) {
			if(f->next) {
				nf=f;
				f=f->next;
				free_file(nf);
			}
		}else{
			nf = lookup_file(f, r->ifcall.wname[i]);
			if(!nf)
				break;
			assert(!nf->next);
			if(strcmp(r->ifcall.wname[i], ".")) {
				nf->next = f;
				f = nf;
			}
		}
		r->ofcall.wqid[i].type = f->tab.qtype;
		r->ofcall.wqid[i].path = QID(f->tab.type, f->id);
	}
	/* There should be a way to do this on freefid() */
	if(i < r->ifcall.nwname) {
		while((nf = f)) {
			f=f->next;
			free_file(nf);
		}
		respond(r, Enofile);
		return;
	}
	/* Remove refs for r->fid if no new fid */
	if(r->ifcall.fid == r->ifcall.newfid) {
		nf = r->fid->aux;
		r->fid->aux = f;
		while((f = nf)) {
			nf = nf->next;
			free_file(f);
		}
	}else
		r->newfid->aux = f;
	r->ofcall.nwqid = i;
	respond(r, nil);
}

static uint
fs_size(FileId *f) {
	switch(f->tab.type) {
	default:
		return 0;
	case FsFColRules:
	case FsFTagRules:
		return f->p.rule->size;
	case FsFKeys:
		return def.keyssz;
	case FsFCtags:
		return strlen(f->p.client->tags);
	case FsFClabel:
		return strlen(f->p.client->name);
	case FsFprops:
		return strlen(f->p.client->props);
	}
}

void
fs_stat(Ixp9Req *r) {
	IxpMsg m;
	Stat s;
	int size;
	char *buf;
	FileId *f;
	
	f = r->fid->aux;

	if(!verify_file(f)) {
		respond(r, Enofile);
		return;
	}

	dostat(&s, fs_size(f), f);
	r->ofcall.nstat = size = ixp_sizeof_stat(&s);
	buf = emallocz(size);

	m = ixp_message(buf, size, MsgPack);
	ixp_pstat(&m, &s);

	r->ofcall.stat = (uchar*)m.data;
	respond(r, nil);
}

void
fs_read(Ixp9Req *r) {
	char *buf;
	FileId *f, *tf;
	int n, offset;
	ulong size;

	f = r->fid->aux;

	if(!verify_file(f)) {
		respond(r, Enofile);
		return;
	}

	if(f->tab.perm & DMDIR && f->tab.perm & 0400) {
		Stat s;
		IxpMsg m;

		offset = 0;
		size = r->ifcall.count;
		if(size > r->fid->iounit)
			size = r->fid->iounit;
		buf = emallocz(size);
		m = ixp_message(buf, size, MsgPack);

		tf = f = lookup_file(f, nil);
		/* Note: f->tab.name == "." so we skip it */
		for(f=f->next; f; f=f->next) {
			dostat(&s, fs_size(f), f);
			n = ixp_sizeof_stat(&s);
			if(offset >= r->ifcall.offset) {
				if(size < n)
					break;
				ixp_pstat(&m, &s);
				size -= n;
			}
			offset += n;
		}
		while((f = tf)) {
			tf=tf->next;
			free_file(f);
		}
		r->ofcall.count = r->ifcall.count - size;
		r->ofcall.data = (char*)m.data;
		respond(r, nil);
		return;
	}
	else{
		if(f->pending) {
			pending_respond(r);
			return;
		}
		switch(f->tab.type) {
		case FsFprops:
			write_buf(r, f->p.client->props, strlen(f->p.client->props));
			respond(r, nil);
			return;
		case FsFColRules:
		case FsFTagRules:
			write_buf(r, f->p.rule->string, f->p.rule->size);
			respond(r, nil);
			return;
		case FsFKeys:
			write_buf(r, def.keys, def.keyssz);
			respond(r, nil);
			return;
		case FsFCtags:
			write_buf(r, f->p.client->tags, strlen(f->p.client->tags));
			respond(r, nil);
			return;
		case FsFClabel:
			write_buf(r, f->p.client->name, strlen(f->p.client->name));
			respond(r, nil);
			return;
		case FsFBar:
			write_buf(r, f->p.bar->buf, strlen(f->p.bar->buf));
			respond(r, nil);
			return;
		case FsFRctl:
			buf = readctl_root();
			write_buf(r, buf, strlen(buf));
			respond(r, nil);
			return;
		case FsFCctl:
			if(r->ifcall.offset) {
				respond(r, nil);
				return;
			}
			r->ofcall.data = smprint("%C", f->p.client);
			r->ofcall.count = strlen(r->ofcall.data); /* will die if nil */
			respond(r, nil);
			return;
		case FsFTindex:
			buf = view_index(f->p.view);
			n = strlen(buf);
			write_buf(r, buf, n);
			respond(r, nil);
			return;
		case FsFTctl:
			buf = readctl_view(f->p.view);
			n = strlen(buf);
			write_buf(r, buf, n);
			respond(r, nil);
			return;
		}
	}
	/* 
	 * This is an assert because this should this should not be called if
	 * the file is not open for reading.
	 */
	die("Read called on an unreadable file");
}

void
fs_write(Ixp9Req *r) {
	FileId *f;
	char *errstr;
	char *p;
	uint i;

	if(r->ifcall.count == 0) {
		respond(r, nil);
		return;
	}
	f = r->fid->aux;

	if(!verify_file(f)) {
		respond(r, Enofile);
		return;
	}

	switch(f->tab.type) {
	case FsFColRules:
	case FsFTagRules:
		write_to_buf(r, &f->p.rule->string, &f->p.rule->size, 0);
		respond(r, nil);
		return;
	case FsFKeys:
		write_to_buf(r, &def.keys, &def.keyssz, 0);
		respond(r, nil);
		return;
	case FsFClabel:
		data_to_cstring(r);
		utfecpy(f->p.client->name, f->p.client->name+sizeof(client->name), r->ifcall.data);
		frame_draw(f->p.client->sel);
		update_class(f->p.client);
		r->ofcall.count = r->ifcall.count;
		respond(r, nil);
		return;
	case FsFCtags:
		data_to_cstring(r);
		apply_tags(f->p.client, r->ifcall.data);
		r->ofcall.count = r->ifcall.count;
		respond(r, nil);
		return;
	case FsFBar:
		i = strlen(f->p.bar->buf);
		p = f->p.bar->buf;
		write_to_buf(r, &p, &i, 279);
		r->ofcall.count = i - r->ifcall.offset;
		respond(r, nil);
		return;
	case FsFCctl:
		errstr = message(r, (MsgFunc)message_client);
		r->ofcall.count = r->ifcall.count;
		respond(r, errstr);
		return;
	case FsFTctl:
		errstr = message(r, (MsgFunc)message_view);
		r->ofcall.count = r->ifcall.count;
		respond(r, errstr);
		return;
	case FsFRctl:
		errstr = message(r, (MsgFunc)message_root);
		r->ofcall.count = r->ifcall.count;
		respond(r, errstr);
		return;
	case FsFEvent:
		if(r->ifcall.data[r->ifcall.count-1] == '\n')
			event("%.*s", (int)r->ifcall.count, r->ifcall.data);
		else
			event("%.*s\n", (int)r->ifcall.count, r->ifcall.data);
		r->ofcall.count = r->ifcall.count;
		respond(r, nil);
		return;
	}
	/*
	 * This is an assert because this function should not be called if
	 * the file is not open for writing.
	 */
	die("Write called on an unwritable file");
}

void
fs_open(Ixp9Req *r) {
	FileId *f;
	
	f = r->fid->aux;

	if(!verify_file(f)) {
		respond(r, Enofile);
		return;
	}

	switch(f->tab.type) {
	case FsFEvent:
		pending_pushfid(&events, r->fid);
		break;
	case FsFDebug:
		pending_pushfid(pdebug+f->id, r->fid);
		debugfile |= 1<<f->id;
		break;
	}
	if((r->ifcall.mode&3) == OEXEC) {
		respond(r, Enoperm);
		return;
	}
	if((r->ifcall.mode&3) != OREAD && !(f->tab.perm & 0200)) {
		respond(r, Enoperm);
		return;
	}
	if((r->ifcall.mode&3) != OWRITE && !(f->tab.perm & 0400)) {
		respond(r, Enoperm);
		return;
	}
	if((r->ifcall.mode&~(3|OAPPEND|OTRUNC))) {
		respond(r, Enoperm);
		return;
	}
	respond(r, nil);
}

void
fs_create(Ixp9Req *r) {
	FileId *f;
	
	f = r->fid->aux;

	switch(f->tab.type) {
	default:
		respond(r, Enoperm);
		return;
	case FsDBars:
		if(!strlen(r->ifcall.name)) {
			respond(r, Ebadvalue);
			return;
		}
		bar_create(f->p.bar_p, r->ifcall.name);
		f = lookup_file(f, r->ifcall.name);
		if(!f) {
			respond(r, Enofile);
			return;
		}
		r->ofcall.qid.type = f->tab.qtype;
		r->ofcall.qid.path = QID(f->tab.type, f->id);
		f->next = r->fid->aux;
		r->fid->aux = f;
		respond(r, nil);
		break;
	}
}

void
fs_remove(Ixp9Req *r) {
	FileId *f = r->fid->aux;

	if(!verify_file(f)) {
		respond(r, Enofile);
		return;
	}


	switch(f->tab.type) {
	default:
		respond(r, Enoperm);
		return;
	case FsFBar:
		bar_destroy(f->next->p.bar_p, f->p.bar);
		bar_draw(screen);
		respond(r, nil);
		break;
	}
}

void
fs_clunk(Ixp9Req *r) {
	FileId *f;
	PLink *pl;
	Queue *qu;
	char *p, *q;
	Client *c;
	IxpMsg m;
	
	f = r->fid->aux;
	if(!verify_file(f)) {
		respond(r, nil);
		return;
	}

	if(f->pending) {
		/* Should probably be in freefid */
		pl = f->p.p;
		pl->prev->next = pl->next;
		pl->next->prev = pl->prev;
		while((qu = pl->queue)) {
			pl->queue = qu->link;
			free(qu->dat);
			free(qu);
		};
		switch(f->tab.type) {
		case FsFDebug:
			if(pl->pending->fids.next == &pl->pending->fids)
				debugfile &= ~(1<<f->id);
			break;
		}
		free(pl);
		respond(r, nil);
		return;
	}

	switch(f->tab.type) {
	case FsFColRules:
		update_rules(&f->p.rule->rule, f->p.rule->string);
		break;
	case FsFTagRules:
		update_rules(&f->p.rule->rule, f->p.rule->string);
		for(c=client; c; c=c->next)
			apply_rules(c);
		view_update_all();
		break;
	case FsFKeys:
		update_keys();
		break;
	case FsFBar:
		p = toutf8(f->p.bar->buf);
		
		m = ixp_message(p, strlen(p), 0);
		msg_parsecolors(&m, &f->p.bar->col);

		q = (char*)m.end-1;
		while(q >= (char*)m.pos && *q == '\n')
			*q-- = '\0';

		q = f->p.bar->text;
		utflcpy(q, (char*)m.pos, sizeof(((Bar*)0)->text));

		free(p);

		bar_draw(screen);
		break;
	}
	respond(r, nil);
}

void
fs_flush(Ixp9Req *r) {
	Ixp9Req *or;
	FileId *f;
	RLink *rl;

	or = r->oldreq;
	f = or->fid->aux;
	if(f->pending) {
		rl = or->aux;
		if(rl) {
			rl->prev->next = rl->next;
			rl->next->prev = rl->prev;
			free(rl);
		}
	}
	respond(r->oldreq, Einterrupted);
	respond(r, nil);
}

void
fs_freefid(Fid *f) {
	FileId *id, *tid;

	tid = f->aux;
	while((id = tid)) {
		tid = id->next;
		free_file(id);
	}
}

