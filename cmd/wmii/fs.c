/* Copyright ©2006 Kris Maglione <fbsdaemon at gmail dot com>
 * See LICENSE file for license details.
 */
#include "dat.h"
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "fns.h"


/* Datatypes: */
typedef struct Dirtab Dirtab;
struct Dirtab {
	char		*name;
	uchar	qtype;
	uint	type;
	uint	perm;
};

typedef struct FidLink FidLink;
struct FidLink {
	FidLink *next;
	Fid *fid;
};

typedef struct FileId FileId;
struct FileId {
	FileId		*next;
	union {
		void	*ref;
		char	*buf;
		Bar	*bar;
		Bar	**bar_p;
		View	*view;
		Client	*client;
		Ruleset	*rule;
		CTuple	*col;
	} p;
	uint	id;
	uint	index;
	Dirtab		tab;
	ushort	nref;
	uchar	volatil;
};

/* Constants */
enum {	/* Dirs */
	FsRoot, FsDClient, FsDClients, FsDBars,
	FsDTag, FsDTags,
	/* Files */
	FsFBar, FsFCctl, FsFColRules, FsFClabel,
	FsFCtags, FsFEvent, FsFKeys, FsFRctl,
	FsFTagRules, FsFTctl, FsFTindex,
	FsFprops
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
/* Pending, outgoing reads on /event */
Ixp9Req *peventread, *oeventread;
/* Fids for /event with pending reads */
FidLink *peventfid;

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
		  {"label",	QTFILE,	FsFClabel,	0600 },
		  {"tags",	QTFILE,		FsFCtags,	0600 },
		  {"props",	QTFILE,		FsFprops,	0400 },
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
static Dirtab *dirtab[] = {
	[FsRoot] = dirtab_root,
	[FsDBars] = dirtab_bars,
	[FsDClients] = dirtab_clients,
	[FsDClient] = dirtab_client,
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
	uint offset, count;
	char *p;

	offset = (r->fid->omode&OAPPEND) ? *len : r->ifcall.offset;
	if(offset > *len || r->ifcall.count == 0) {
		r->ofcall.count = 0;
		return;
	}

	count = r->ifcall.count;
	if(max && (count > max - offset))
		count = max - offset;

	*len = offset + count;
	if(max == 0)
		*buf = erealloc((void*)*buf, *len + 1);

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

static char *
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

		m = ixp_message((uchar*)s, p-s, 0);
		s = fn(f->p.ref, &m);
		if(s)
			err = s;
		s = p + 1;
	}
	return err;
}

static void
respond_event(Ixp9Req *r) {
	FileId *f = r->fid->aux;
	if(f->p.buf) {
		r->ofcall.data = (void *)f->p.buf;
		r->ofcall.count = strlen(f->p.buf);
		respond(r, nil);
		f->p.buf = nil;
	}else{
		r->aux = peventread;
		peventread = r;
	}
}

void
write_event(char *format, ...) {
	uint len, slen;
	va_list ap;
	FidLink *f;
	FileId *fi;
	Ixp9Req *req;

	va_start(ap, format);
	vsnprint(buffer, sizeof(buffer), format, ap);
	va_end(ap);

	len = strlen(buffer);
	if(len == 0)
		return;

	for(f=peventfid; f; f=f->next) {
		fi = f->fid->aux;

		slen = 0;
		if(fi->p.buf)
			slen = strlen(fi->p.buf);
		fi->p.buf = erealloc(fi->p.buf, slen + len + 1);
		fi->p.buf[slen] = '\0';
		strcat(fi->p.buf, buffer);
	}
	oeventread = peventread;
	peventread = nil;
	while((req = oeventread)) {
		oeventread = oeventread->aux;
		respond_event(req);
	}
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

	if(!(parent->tab.perm & DMDIR))
		return nil;
	dir = dirtab[parent->tab.type];
	last = &ret;
	ret = nil;
	for(; dir->name; dir++) {
		/* Dynamic dirs */
		if(!*dir->name) { /* strlen(dir->name) == 0 */
			switch(parent->tab.type) {
			case FsDClients:
				if(!name || !strcmp(name, "sel")) {
					if((c = selclient())) {
						file = get_file();
						*last = file;
						last = &file->next;
						file->volatil = True;
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
						file->volatil = True;
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
			case FsDTags:
				if(!name || !strcmp(name, "sel")) {
					if(screen->sel) {
						file = get_file();
						*last = file;
						last = &file->next;
						file->volatil = True;
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
						file->volatil = True;
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
						file->volatil = True;
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
		if(!name || !strcmp(name, dir->name)) {
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
					file->p.bar_p = &screen[0].bar[BarLeft];
				else
					file->p.bar_p = &screen[0].bar[BarRight];
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

static Bool
verify_file(FileId *f) {
	FileId *nf;
	int ret;

	if(!f->next)
		return True;

	ret = False;
	if(verify_file(f->next)) {
		nf = lookup_file(f->next, f->tab.name);
		if(nf) {
			if(!nf->volatil || nf->p.ref == f->p.ref)
				ret = True;
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
	uchar *buf;
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
	int size;

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
		buf = emallocz(size);
		m = ixp_message((uchar*)buf, size, MsgPack);

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
			buf = read_root_ctl();
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
			buf = (char*)view_index(f->p.view);
			n = strlen(buf);
			write_buf(r, buf, n);
			respond(r, nil);
			return;
		case FsFTctl:
			buf = (char*)view_ctl(f->p.view);
			n = strlen(buf);
			write_buf(r, buf, n);
			respond(r, nil);
			return;
		case FsFEvent:
			respond_event(r);
			return;
		}
	}
	/* 
	 * This is an assert because this should this should not be called if
	 * the file is not open for reading.
	 */
	assert(!"Read called on an unreadable file");
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
		draw_frame(f->p.client->sel);
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
		/* Why the explicit cast? Ask gcc. */
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
			write_event("%.*s", (int)r->ifcall.count, r->ifcall.data);
		else
			write_event("%.*s\n", (int)r->ifcall.count, r->ifcall.data);
		r->ofcall.count = r->ifcall.count;
		respond(r, nil);
		return;
	}
	/*
	 * This is an assert because this function should not be called if
	 * the file is not open for writing.
	 */
	assert(!"Write called on an unwritable file");
}

void
fs_open(Ixp9Req *r) {
	FidLink *fl;
	FileId *f = r->fid->aux;

	if(!verify_file(f)) {
		respond(r, Enofile);
		return;
	}

	switch(f->tab.type) {
	case FsFEvent:
		fl = emallocz(sizeof(FidLink));
		fl->fid = r->fid;
		fl->next = peventfid;
		peventfid = fl;
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
	FileId *f = r->fid->aux;

	switch(f->tab.type) {
	default:
		respond(r, Enoperm);
		return;
	case FsDBars:
		if(!strlen(r->ifcall.name)) {
			respond(r, Ebadvalue);
			return;
		}
		create_bar(f->p.bar_p, r->ifcall.name);
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
		destroy_bar(f->next->p.bar_p, f->p.bar);
		draw_bar(screen);
		respond(r, nil);
		break;
	}
}

void
fs_clunk(Ixp9Req *r) {
	FidLink **fl, *ft;
	FileId *f;
	char *p, *q;
	Client *c;
	IxpMsg m;
	
	f = r->fid->aux;
	if(!verify_file(f)) {
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
		update_views();
		break;
	case FsFKeys:
		update_keys();
		break;
	case FsFBar:
		p = toutf8(f->p.bar->buf);
		
		m = ixp_message((uchar*)p, strlen(p), 0);
		parse_colors(&m, &f->p.bar->col);

		q = (char*)m.end-1;
		while(q >= (char*)m.pos && *q == '\n')
			*q-- = '\0';

		q = f->p.bar->text;
		utflcpy(q, (char*)m.pos, sizeof(((Bar*)0)->text));

		free(p);

		draw_bar(screen);
		break;
	case FsFEvent:
		for(fl=&peventfid; *fl; fl=&fl[0]->next)
			if(fl[0]->fid == r->fid) {
				ft = fl[0];
				fl[0] = fl[0]->next;
				f = ft->fid->aux;
				free(f->p.buf);
				free(ft);
				break;
			}
		break;
	}
	respond(r, nil);
}

void
fs_flush(Ixp9Req *r) {
	Ixp9Req **i, **j;

	for(i=&peventread; i != &oeventread; i=&oeventread)
		for(j=i; *j; j=(Ixp9Req**)&j[0]->aux)
			if(*j == r->oldreq) {
				j[0] = j[0]->aux;
				respond(r->oldreq, Einterrupted);
				goto done;
			}
done:
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
