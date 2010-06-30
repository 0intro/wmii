/* Copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "dat.h"
#include <ctype.h>
#include <stdarg.h>
#include <time.h>
#include "fns.h"

typedef union IxpFileIdU IxpFileIdU;
union IxpFileIdU {
	Bar*		bar;
	Bar**		bar_p;
	CTuple*		col;
	Client*		client;
	Ruleset*	rule;
	View*		view;
	char*		buf;
	void*		ref;
};

#include <ixp_srvutil.h>

static IxpPending	events;
static IxpPending	pdebug[NDebugOpt];

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
	FsFRules,
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
static IxpDirtab
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
		  {"rules",	QTFILE,		FsFRules,	0600 },
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
static IxpDirtab* dirtab[] = {
	[FsRoot] = dirtab_root,
	[FsDBars] = dirtab_bars,
	[FsDClients] = dirtab_clients,
	[FsDClient] = dirtab_client,
	[FsDDebug] = dirtab_debug,
	[FsDTags] = dirtab_tags,
	[FsDTag] = dirtab_tag,
};
typedef char* (*MsgFunc)(void*, IxpMsg*);
typedef char* (*BufFunc)(void*);

typedef struct ActionTab ActionTab;
static struct ActionTab {
	MsgFunc		msg;
	BufFunc		read;
	size_t		buffer;
	size_t		size;
	int		max;
} actiontab[] = {
	[FsFBar]      = { .msg = (MsgFunc)message_bar,          .read = (BufFunc)readctl_bar },
	[FsFCctl]     = { .msg = (MsgFunc)message_client,     	.read = (BufFunc)readctl_client },
	[FsFRctl]     = { .msg = (MsgFunc)message_root,       	.read = (BufFunc)readctl_root },
	[FsFTctl]     = { .msg = (MsgFunc)message_view,       	.read = (BufFunc)readctl_view },
	[FsFTindex]   = { .msg = (MsgFunc)0,		    	.read = (BufFunc)view_index },
	[FsFColRules] = { .buffer = offsetof(Ruleset, string),	.size = offsetof(Ruleset, size) },
	[FsFKeys]     = { .buffer = offsetof(Defs, keys),	.size = offsetof(Defs, keyssz) },
	[FsFRules]    = { .buffer = offsetof(Ruleset, string), 	.size = offsetof(Ruleset, size) },
	[FsFClabel]   = { .buffer = offsetof(Client, name),	.max = sizeof ((Client*)0)->name },
	[FsFCtags]    = { .buffer = offsetof(Client, tags),   	.max = sizeof ((Client*)0)->tags },
	[FsFprops]    = { .buffer = offsetof(Client, props),  	.max = sizeof ((Client*)0)->props },
};

void
event(const char *format, ...) {
	va_list ap;

	va_start(ap, format);
	vsnprint(buffer, sizeof buffer, format, ap);
	va_end(ap);

	ixp_pending_write(&events, buffer, strlen(buffer));
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

	if(flag == 0)
		flag = dflags;

	if(!((debugflag|debugfile) & flag))
		return;

	s = vsmprint(fmt, ap);
	dwrite(flag, s, strlen(s), false);
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
dwrite(int flag, void *buf, int n, bool always) {
	int i;

	if(flag == 0)
		flag = dflags;

	if(always || debugflag&flag)
		write(2, buf, n);

	if(debugfile&flag)
	for(i=0; i < nelem(pdebug); i++)
		if(flag & (1<<i))
			ixp_pending_write(pdebug+i, buf, n);
}

static uint	fs_size(IxpFileId*);

static void
dostat(IxpStat *s, IxpFileId *f) {
	s->type = 0;
	s->dev = 0;
	s->qid.path = QID(f->tab.type, f->id);
	s->qid.version = 0;
	s->qid.type = f->tab.qtype;
	s->mode = f->tab.perm;
	s->atime = time(nil);
	s->mtime = s->atime;
	s->length = fs_size(f);;
	s->name = f->tab.name;
	s->uid = user;
	s->gid = user;
	s->muid = user;
}

/*
 * All lookups and directory organization should be performed through
 * lookup_file, mostly through the dirtab[] tree.
 */
static IxpFileId*
lookup_file(IxpFileId *parent, char *name)
{
	IxpFileId *ret, *file, **last;
	IxpDirtab *dir;
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
#               define push_file(nam, id_, vol)   \
			file = ixp_srv_getfile(); \
			file->id = id_;           \
			file->volatil = vol;      \
			*last = file;             \
			last = &file->next;       \
			file->tab = *dir;         \
			file->tab.name = estrdup(nam)
		/* Dynamic dirs */
		if(dir->name[0] == '\0') {
			switch(parent->tab.type) {
			case FsDClients:
				if(!name || !strcmp(name, "sel")) {
					if((c = selclient())) {
						push_file("sel", c->w.xid, true);
						file->p.client = c;
						file->index = c->w.xid;
					}
					if(name)
						goto LastItem;
				}
				SET(id);
				if(name) {
					id = (uint)strtol(name, &name, 16);
					if(*name)
						goto NextItem;
				}
				for(c=client; c; c=c->next) {
					if(!name || c->w.xid == id) {
						push_file(sxprint("%#C", c), c->w.xid, true);
						file->p.client = c;
						file->index = c->w.xid;
						assert(file->tab.name);
						if(name)
							goto LastItem;
					}
				}
				break;
			case FsDDebug:
				for(i=0; i < nelem(pdebug); i++)
					if(!name || !strcmp(name, debugtab[i])) {
						push_file(debugtab[i], i, false);
						if(name)
							goto LastItem;
					}
				break;
			case FsDTags:
				if(!name || !strcmp(name, "sel")) {
					if(selview) {
						push_file("sel", selview->id, true);
						file->p.view = selview;
					}
					if(name)
						goto LastItem;
				}
				for(v=view; v; v=v->next) {
					if(!name || !strcmp(name, v->name)) {
						push_file(v->name, v->id, true);
						file->p.view = v;
						if(name)
							goto LastItem;
					}
				}
				break;
			case FsDBars:
				for(b=*parent->p.bar_p; b; b=b->next) {
					if(!name || !strcmp(name, b->name)) {
						push_file(b->name, b->id, true);
						file->p.bar = b;
						if(name)
							goto LastItem;
					}
				}
				break;
			}
		}else /* Static dirs */
		if(!name && !(dir->flags & FLHide) || name && !strcmp(name, dir->name)) {
			push_file(file->tab.name, 0, false);
			file->p.ref = parent->p.ref;
			file->index = parent->index;
			/* Special considerations: */
			switch(file->tab.type) {
			case FsDBars:
				if(!strcmp(file->tab.name, "lbar"))
					file->p.bar_p = &screen[0].bar[BLeft];
				else
					file->p.bar_p = &screen[0].bar[BRight];
				file->id = (int)(uintptr_t)file->p.bar_p;
				break;
			case FsFColRules:
				file->p.rule = &def.colrules;
				break;
			case FsFKeys:
				file->p.ref = &def;
				break;
			case FsFRules:
				file->p.rule = &def.rules;
				break;
			}
			if(name)
				goto LastItem;
		}
	NextItem:
		continue;
#		undef push_file
	}
LastItem:
	*last = nil;
	return ret;
}

/* Service Functions */
void
fs_attach(Ixp9Req *r) {
	IxpFileId *f;

	f = ixp_srv_getfile();
	f->tab = dirtab[FsRoot][0];
	f->tab.name = estrdup("/");
	r->fid->aux = f;
	r->fid->qid.type = f->tab.qtype;
	r->fid->qid.path = QID(f->tab.type, 0);
	r->ofcall.rattach.qid = r->fid->qid;
	ixp_respond(r, nil);
}

void
fs_walk(Ixp9Req *r) {

	ixp_srv_walkandclone(r, lookup_file);
}

static uint
fs_size(IxpFileId *f) {
	ActionTab *t;

	t = &actiontab[f->tab.type];
	if(f->tab.type < nelem(actiontab))
		if(t->size)
			return structmember(f->p.ref, int, t->size);
		else if(t->buffer && t->max)
			return strlen(structptr(f->p.ref, char, t->buffer));
		else if(t->buffer)
			return strlen(structmember(f->p.ref, char*, t->buffer));
		else if(t->read)
			return strlen(t->read(f->p.ref));
	return 0;
}

void
fs_stat(Ixp9Req *r) {
	IxpMsg m;
	IxpStat s;
	int size;
	char *buf;
	IxpFileId *f;

	f = r->fid->aux;

	if(!ixp_srv_verifyfile(f, lookup_file)) {
		ixp_respond(r, Enofile);
		return;
	}

	dostat(&s, f);
	size = ixp_sizeof_stat(&s);
	r->ofcall.rstat.nstat = size;
	buf = emallocz(size);

	m = ixp_message(buf, size, MsgPack);
	ixp_pstat(&m, &s);

	r->ofcall.rstat.stat = (uchar*)m.data;
	ixp_respond(r, nil);
}

void
fs_read(Ixp9Req *r) {
	char *buf;
	IxpFileId *f;
	ActionTab *t;
	int n, found;

	f = r->fid->aux;
	found = 0;

	if(!ixp_srv_verifyfile(f, lookup_file)) {
		ixp_respond(r, Enofile);
		return;
	}

	if(f->tab.perm & DMDIR && f->tab.perm & 0400) {
		ixp_srv_readdir(r, lookup_file, dostat);
		return;
	}
	else{
		if(f->pending) {
			ixp_pending_respond(r);
			return;
		}
		t = &actiontab[f->tab.type];
		if(f->tab.type < nelem(actiontab)) {
			if(t->read)
				buf = t->read(f->p.ref);
			else if(t->buffer && t->max)
				buf = structptr(f->p.ref, char, t->buffer);
			else if(t->buffer)
				buf = structmember(f->p.ref, char*, t->buffer);
			else
				goto done;
			n = t->size ? structmember(f->p.ref, int, t->size) : strlen(buf);
			ixp_srv_readbuf(r, buf, n);
			ixp_respond(r, nil);
			found++;
		}
	done:
		switch(f->tab.type) {
		default:
			if(found)
				return;
		}
	}
	/* This should not be called if the file is not open for reading. */
	die("Read called on an unreadable file");
}

void
fs_write(Ixp9Req *r) {
	IxpFileId *f;
	ActionTab *t;
	char *errstr;
	int found;

	found = 0;
	errstr = nil;
	if(r->ifcall.io.count == 0) {
		ixp_respond(r, nil);
		return;
	}
	f = r->fid->aux;

	if(!ixp_srv_verifyfile(f, lookup_file)) {
		ixp_respond(r, Enofile);
		return;
	}

	switch(f->tab.type) {
	case FsFCtags:
		r->ofcall.io.count = r->ifcall.io.count;
		ixp_srv_data2cstring(r);
		client_applytags(f->p.client, r->ifcall.io.data);
		ixp_respond(r, nil);
		return;
	}

	if(waserror()) {
		ixp_respond(r, ixp_errbuf());
		return;
	}

	t = &actiontab[f->tab.type];
	if(f->tab.type < nelem(actiontab)) {
		if(t->msg) {
			errstr = ixp_srv_writectl(r, t->msg);
			r->ofcall.io.count = r->ifcall.io.count;
		}
		else if(t->buffer && t->max)
			ixp_srv_writebuf(r, (char*[]){ structptr(f->p.ref, char, t->buffer) },
					 t->size ? structptr(f->p.ref, uint, t->size)
					         : (uint[]){ strlen(structptr(f->p.ref, char, t->buffer)) },
					 t->max);
		else if(t->buffer)
			ixp_srv_writebuf(r, structptr(f->p.ref, char*, t->buffer),
					 t->size ? structptr(f->p.ref, uint, t->size) : nil,
					 t->max);
		else
			goto done;
		ixp_respond(r, nil);
		found++;
	}
done:
	switch(f->tab.type) {
	case FsFClabel:
		frame_draw(f->p.client->sel);
		update_class(f->p.client);
		break;
	case FsFCtags:
		client_applytags(f->p.client, f->p.client->tags);
		break;
	case FsFEvent:
		if(r->ifcall.io.data[r->ifcall.io.count-1] == '\n')
			event("%.*s", (int)r->ifcall.io.count, r->ifcall.io.data);
		else
			event("%.*s\n", (int)r->ifcall.io.count, r->ifcall.io.data);
		r->ofcall.io.count = r->ifcall.io.count;
		ixp_respond(r, nil);
		break;
	default:
		/* This should not be called if the file is not open for writing. */
		if(!found)
			die("Write called on an unwritable file");
	}
	poperror();
	return;
}

void
fs_open(Ixp9Req *r) {
	IxpFileId *f;

	f = r->fid->aux;

	if(!ixp_srv_verifyfile(f, lookup_file)) {
		ixp_respond(r, Enofile);
		return;
	}

	switch(f->tab.type) {
	case FsFEvent:
		ixp_pending_pushfid(&events, r->fid);
		break;
	case FsFDebug:
		ixp_pending_pushfid(pdebug+f->id, r->fid);
		debugfile |= 1<<f->id;
		break;
	}

	if((r->ifcall.topen.mode&3) == OEXEC
	|| (r->ifcall.topen.mode&3) != OREAD && !(f->tab.perm & 0200)
	|| (r->ifcall.topen.mode&3) != OWRITE && !(f->tab.perm & 0400)
	|| (r->ifcall.topen.mode & ~(3|OAPPEND|OTRUNC)))
		ixp_respond(r, Enoperm);
	else
		ixp_respond(r, nil);
}

void
fs_create(Ixp9Req *r) {
	IxpFileId *f;

	f = r->fid->aux;

	switch(f->tab.type) {
	default:
		ixp_respond(r, Enoperm);
		return;
	case FsDBars:
		if(!strlen(r->ifcall.tcreate.name)) {
			ixp_respond(r, Ebadvalue);
			return;
		}
		bar_create(f->p.bar_p, r->ifcall.tcreate.name);
		f = lookup_file(f, r->ifcall.tcreate.name);
		if(!f) {
			ixp_respond(r, Enofile);
			return;
		}
		r->ofcall.ropen.qid.type = f->tab.qtype;
		r->ofcall.ropen.qid.path = QID(f->tab.type, f->id);
		f->next = r->fid->aux;
		r->fid->aux = f;
		ixp_respond(r, nil);
		break;
	}
}

void
fs_remove(Ixp9Req *r) {
	IxpFileId *f;
	WMScreen *s;

	f = r->fid->aux;
	if(!ixp_srv_verifyfile(f, lookup_file)) {
		ixp_respond(r, Enofile);
		return;
	}

	switch(f->tab.type) {
	default:
		ixp_respond(r, Enoperm);
		return;
	case FsFBar:
		s = f->p.bar->screen;
		bar_destroy(f->next->p.bar_p, f->p.bar);
		bar_draw(s);
		break;
	case FsDClient:
		client_kill(f->p.client, true);
		break;
	}
	ixp_respond(r, nil);
}

void
fs_clunk(Ixp9Req *r) {
	IxpFileId *f;

	f = r->fid->aux;
	if(!ixp_srv_verifyfile(f, lookup_file)) {
		ixp_respond(r, nil);
		return;
	}

	if(f->pending) {
		/* Should probably be in freefid */
		if(ixp_pending_clunk(r)) {
			if(f->tab.type == FsFDebug)
				debugfile &= ~(1<<f->id);
		}
		return;
	}

	switch(f->tab.type) {
	case FsFColRules:
	case FsFRules:
		update_rules(&f->p.rule->rule, f->p.rule->string);
		break;
	case FsFKeys:
		update_keys();
		break;
	}
	ixp_respond(r, nil);
}

void
fs_flush(Ixp9Req *r) {
	Ixp9Req *or;
	IxpFileId *f;

	or = r->oldreq;
	f = or->fid->aux;
	if(f->pending)
		ixp_pending_flush(r);
	/* else die() ? */
	ixp_respond(r->oldreq, Einterrupted);
	ixp_respond(r, nil);
}

void
fs_freefid(IxpFid *f) {
	IxpFileId *id, *tid;

	tid = f->aux;
	while((id = tid)) {
		tid = id->next;
		ixp_srv_freefile(id);
	}
}

