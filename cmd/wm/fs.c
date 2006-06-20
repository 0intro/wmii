/*
 * (C)opyright MMVI Kris Maglione <bsdaemon at comcast dot net>
 * See LICENSE file for license details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "wm.h"

/* Datatypes: */
/**************/
typedef struct Dirtab Dirtab;
struct Dirtab {
	char		*name;
	unsigned char	qtype;
	unsigned int	type;
	unsigned int	perm;
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
		Rules	*rule;
		BlitzColor	*col;
	};
	unsigned int	id;
	unsigned int	index;
	Dirtab		tab;
	unsigned short	nref;
};

/* Constants */
/*************/
enum {	/* Dirs */
	FsRoot, FsDClient, FsDClients, FsDBar,
	FsDTag, FsDTags,
	/* Files */
	FsFBar, FsFBorder, FsFCNorm, FsFCSel,
	FsFCctl, FsFColRules, FsFCtags,
	FsFEvent, FsFFont, FsFKeys, FsFRctl,
	FsFTagRules, FsFTctl, FsFTindex,
	FsFgrabmod, FsFprops, RsFFont
};

/* Error messages */
static char
	*Enoperm = "permission denied",
	*Enofile = "file not found",
	*Ebadvalue = "bad value",
	*Einterrupted = "interrupted";

/* Macros */
#define QID(t, i) (((long long)((t)&0xFF)<<32)|((i)&0xFFFFFFFF))

/* Global Vars */
/***************/
FileId *free_fileid = nil;
Req *pending_event_reads = nil;
FidLink *pending_event_fids;
P9Srv p9srv = {
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

/* ad-hoc file tree. Empty names ("") indicate a dynamic entry to be filled
 * in by lookup_file */
static Dirtab
dirtab_root[]=	 {{".",		QTDIR,		FsRoot,		0500|DMDIR },
		  {"rbar",	QTDIR,		FsDBar,		0700|DMDIR },
		  {"lbar",	QTDIR,		FsDBar,		0700|DMDIR },
		  {"client",	QTDIR,		FsDClients,	0500|DMDIR },
		  {"tag",	QTDIR,		FsDTags,	0500|DMDIR },
		  {"ctl",	QTAPPEND,	FsFRctl,	0600|DMAPPEND },
		  {"border",	QTFILE,		FsFBorder,	0600 }, 
		  {"colrules",	QTFILE,		FsFColRules,	0600 }, 
		  {"event",	QTFILE,		FsFEvent,	0600 },
		  {"font",	QTFILE,		FsFFont,	0600 },
		  {"grabmod",	QTFILE,		FsFgrabmod,	0600 },
		  {"keys",	QTFILE,		FsFKeys,	0600 },
		  {"normcolors",QTFILE,		FsFCNorm,	0600 },
		  {"selcolors",	QTFILE,		FsFCSel,	0600 },
		  {"tagrules",	QTFILE,		FsFTagRules,	0600 }, 
		  {nil}},
dirtab_clients[]={{".",		QTDIR,		FsDClients,	0500|DMDIR },
		  {"",		QTDIR,		FsDClient,	0500|DMDIR },
		  {nil}},
dirtab_client[]= {{".",		QTDIR,		FsDClient,	0500|DMDIR },
		  {"ctl",	QTAPPEND,	FsFCctl,	0200|DMAPPEND },
		  {"tags",	QTFILE,		FsFCtags,	0600 },
		  {"props",	QTFILE,		FsFprops,	0400 },
		  {nil}},
dirtab_bar[]=	 {{".",		QTDIR,		FsDBar,		0700|DMDIR },
		  {"",		QTFILE,		FsFBar,		0600 },
		  {nil}},
dirtab_tags[]=	 {{".",		QTDIR,		FsDTags,	0500|DMDIR },
		  {"",		QTDIR,		FsDTag,		0500|DMDIR },
		  {nil}},
dirtab_tag[]=	 {{".",		QTDIR,		FsDTag,		0500|DMDIR },
		  {"ctl",	QTAPPEND,	FsFTctl,	0200|DMAPPEND },
		  {"index",	QTFILE,		FsFTindex,	0400 },
		  {nil}};
/* Writing the lists separately and using an array of their references
 * removes the need for casting and allows for C90 conformance,
 * since otherwise we would need to use compound literals */
static Dirtab *dirtab[] = {
	[FsRoot]	dirtab_root,
	[FsDBar]	dirtab_bar,
	[FsDClients]	dirtab_clients,
	[FsDClient]	dirtab_client,
	[FsDTags]	dirtab_tags,
	[FsDTag]	dirtab_tag,
};

/* Utility Functions */
/*********************/

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

/* This function's name belies it's true purpose. It increases
 * the reference count of the FileId list */
static void
clone_files(FileId *f) {
	for(; f; f=f->next)
		cext_assert(f->nref++);
}

/* This should be moved to libixp */
static void
write_buf(Req *r, void *buf, unsigned int len) {
	if(r->ifcall.offset >= len)
		return;

	len -= r->ifcall.offset;
	if(len > r->ifcall.count)
		len = r->ifcall.count;
	/* XXX: mallocz is not really needed here */
	r->ofcall.data = cext_emallocz(len);
	memcpy(r->ofcall.data, buf + r->ifcall.offset, len);
	r->ofcall.count = len;
}

/* This should be moved to libixp */
void
write_to_buf(Req *r, void *buf, unsigned int *len, unsigned int max) {
	unsigned int offset, count;

	offset = (r->fid->omode&OAPPEND) ? *len : r->ifcall.offset;
	if(offset > *len || r->ifcall.count == 0) {
		r->ofcall.count = 0;
		return;
	}

	count = r->ifcall.count;
	if(max && (count > max - offset))
		count = max - offset;

	*len = offset + count;
	
	if(max == 0) {
		*(void **)buf = realloc(*(void **)buf, *len + 1);
		cext_assert(*(void **)buf);
		buf = *(void **)buf;
	}
		
	memcpy(buf + offset, r->ifcall.data, count);
	r->ofcall.count = count;
	((char *)buf)[offset+count] = '\0'; /* shut up valgrind */
	/* and save some lines later... we alloc for it anyway */
}

/* This should be moved to libixp */
void
data_to_cstring(Req *r) {
	unsigned int i;
	for(i=0; i < r->ifcall.count; i++)
		if(r->ifcall.data[i] == '\n')
			break;
	if(i == r->ifcall.count)
		r->ifcall.data = realloc(r->ifcall.data, i + 1);
	cext_assert(r->ifcall.data);
	r->ifcall.data[i] = '\0';
}

/* This should be moved to liblitz */
char *
parse_colors(char **buf, int *buflen, BlitzColor *col) {
	unsigned int i;
	if(*buflen < 23 || 3 != sscanf(*buf, "#%06x #%06x #%06x", &i,&i,&i))
		return Ebadvalue;
	(*buflen) -= 23;
	bcopy(*buf, col->colstr, 23);
	blitz_loadcolor(col);

	(*buf) += 23;
	if(**buf == '\n' || **buf == ' ') {
		(*buf)++;
		(*buflen)--;
	}
	return nil;
}

void
respond_event(Req *r) {
	FileId *f = r->fid->aux;
	if(f->buf) {
		r->ofcall.data = f->buf;
		r->ofcall.count = strlen(f->buf);
		respond(r, nil);
		f->buf = nil;
	}else{
		r->aux = pending_event_reads;
		pending_event_reads = r;
	}
}

void
write_event(char *buf) {
	unsigned int len, slen;
	FidLink *f;
	FileId *fi;
	Req *aux;

	if(!(len = strlen(buf)))
		return;
	for(f=pending_event_fids; f; f=f->next) {
		fi = f->fid->aux;
		slen = fi->buf ? strlen(fi->buf) : 0;
		fi->buf = realloc(fi->buf, slen + len + 1);
		fi->buf[slen] = '\0'; /* shut up valgrind */
		strcat(fi->buf, buf);
	}
	while((aux = pending_event_reads)) {
		pending_event_reads = pending_event_reads->aux;
		respond_event(aux);
	}
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

/* lookup_file */
/***************/
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
	unsigned int i, id;

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
				if(!name || !strncmp(name, "sel", 4)) {
					if((c = sel_client())) {
						file = get_file();
						*last = file;
						last = &file->next;
						file->ref = c;
						file->id = c->id;
						file->index = idx_of_client(c);
						file->tab = *dir;
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
						file = get_file();
						*last = file;
						last = &file->next;
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
						file = get_file();
						*last = file;
						last = &file->next;
						file->ref = sel;
						file->id = sel->id;
						file->tab = *dir;
						file->tab.name = strdup("sel");
					}if(name) goto LastItem;
				}
				for(v=view; v; v=v->next) {
					if(!name || !strcmp(name, v->name)) {
						file = get_file();
						*last = file;
						last = &file->next;
						file->ref = v;
						file->id = v->id;
						file->tab = *dir;
						file->tab.name = strdup(v->name);
						if(name) goto LastItem;
					}
				}
				break;
			case FsDBar:
				for(b=*parent->bar_p; b; b=b->next) {
					if(!name || !strcmp(name, b->name)) {
						file = get_file();
						*last = file;
						last = &file->next;
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
			file = get_file();
			*last = file;
			last = &file->next;
			file->id = 0;
			file->ref = parent->ref;
			file->index = parent->index;
			file->tab = *dir;
			file->tab.name = strdup(file->tab.name);

			/* Special considerations: */
			switch(file->tab.type) {
			case FsDBar:
				if(!strncmp(file->tab.name, "lbar", 5))
					file->ref = &lbar;
				else
					file->ref = &rbar;
				break;
			case FsFColRules:
				file->ref = &def.colrules;
				break;
			case FsFTagRules:
				file->ref = &def.tagrules;
				break;
			case FsFCSel:
				file->ref = &def.selcolor;
				break;
			case FsFCNorm:
				file->ref = &def.normcolor;
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

/* Service Functions */
/*********************/
void
fs_attach(Req *r) {
	FileId *f = get_file();
	f->tab = dirtab[FsRoot][0];
	f->tab.name = strdup("/");
	f->ref = nil; /* shut up valgrind */
	r->fid->aux = f;
	r->fid->qid.type = f->tab.qtype;
	r->fid->qid.path = QID(f->tab.type, 0);
	r->ofcall.qid = r->fid->qid;
	respond(r, nil);
}

void
fs_walk(Req *r) {
	FileId *f, *nf;
	int i;

	f = r->fid->aux;

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
			cext_assert(!nf->next);
			nf->next = f;
			f = nf;
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
		return respond(r, Enofile);
	}

	/* Remove refs for r->fid if no new fid */
	/* If Fids were ref counted, this could be
	 * done in their decref function */
	if(r->ifcall.fid == r->ifcall.newfid) {
		nf=r->fid->aux;
		r->fid->aux = f;
		while((nf = f)) {
			f=f->next;
			free_file(nf);
		}
	}

	r->newfid->aux = f;
	r->ofcall.nwqid = i;
	respond(r, nil);
}

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
	FileId *f, *tf;
	int n, offset;
	int size;

	offset = 0;
	f = r->fid->aux;

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
			tf=tf->next;
			free_file(f);
		}

		r->ofcall.count = r->ifcall.count - size;
		respond(r, nil);
	}else{
		switch(f->tab.type) {
		case FsFprops:
			write_buf(r, (void *)f->client->props, strlen(f->client->props));
			return respond(r, nil);
		case FsFCSel:
		case FsFCNorm:
			write_buf(r, (void *)f->col->colstr, strlen(f->col->colstr));
			return respond(r, nil);
		case FsFColRules:
		case FsFTagRules:
			write_buf(r, (void *)f->rule->string, f->rule->size);
			return respond(r, nil);
		case FsFKeys:
			write_buf(r, (void *)def.keys, def.keyssz);
			return respond(r, nil);
		case FsFFont:
			write_buf(r, (void *)def.font.fontstr, strlen(def.font.fontstr));
			return respond(r, nil);
		case FsFCtags:
			write_buf(r, (void *)f->client->tags, strlen(f->client->tags));
			return respond(r, nil);
		case FsFTctl:
			write_buf(r, (void *)f->view->name, strlen(f->view->name));
			return respond(r, nil);
		case FsFgrabmod:
			write_buf(r, (void *)def.grabmod, strlen(def.grabmod));
			return respond(r, nil);
		case FsFBar:
			write_buf(r, (void *)f->bar->buf, strlen(f->bar->buf));
			return respond(r, nil);
		case FsFCctl:
			if(r->ifcall.offset)
				return respond(r, nil);
			n = asprintf((char **)&r->ofcall.data, "%d", f->index);
			cext_assert(n >= 0);
			r->ofcall.count = n;
			return respond(r, nil);
		case FsFBorder:
			if(r->ifcall.offset)
				return respond(r, nil);
			n = asprintf((char **)&r->ofcall.data, "%d", def.border);
			cext_assert(n >= 0);
			r->ofcall.count = n;
			return respond(r, nil);
		case FsFTindex:
			buf = view_index(f->view);
			n = strlen((char *)buf);
			write_buf(r, (void *)buf, n);
			return respond(r, nil);
		case FsFEvent:
			respond_event(r);
			return;
		}
		/* XXX: This should be taken care of by open */
		/* should probably be an assertion in the future */
		respond(r, Enoperm);
	}
}

/* This function needs to be seriously cleaned up */
void
fs_write(Req *r) {
	FileId *f;
	char *buf, *errstr;
	unsigned int i;

	f = r->fid->aux;
	switch(f->tab.type) {
	case FsFColRules:
	case FsFTagRules:
		write_to_buf(r, &f->rule->string, &f->rule->size, 0);
		return respond(r, nil);
	case FsFKeys:
		write_to_buf(r, &def.keys, &def.keyssz, 0);
		return respond(r, nil);
	case FsFFont:
		data_to_cstring(r);
		i=strlen(def.font.fontstr);
		write_to_buf(r, &def.font.fontstr, &i, 0);
		blitz_loadfont(&def.font);
		r->ofcall.count = i- r->ifcall.offset;
		return respond(r, nil);
	case FsFCtags:
		data_to_cstring(r);
		i=strlen(f->client->tags);
		write_to_buf(r, &f->client->tags, &i, 255);
		r->ofcall.count = i- r->ifcall.offset;
		return respond(r, nil);
	case FsFBorder:
		data_to_cstring(r);
		i = (unsigned int)strtol((char *)r->ifcall.data, &buf, 10);
		if(*buf)
			return respond(r, Ebadvalue);
		def.border = i;
		r->ofcall.count = r->ifcall.count;
		return respond(r, nil);
	case FsFBar:
		/* XXX: This should validate after each write */
		i = strlen(f->bar->buf);
		write_to_buf(r, &f->bar->buf, &i, 279);
		r->ofcall.count = i- r->ifcall.offset;
		return respond(r, nil);
	case FsFCSel:
	case FsFCNorm:
		/* XXX: This allows for junk after the color string */
		data_to_cstring(r);
		buf = (char *)r->ifcall.data;
		i = r->ifcall.count;
		if((errstr = parse_colors((char **)&buf, (int *)&i, f->col)))
			return respond(r, errstr);
		draw_clients();
		r->ofcall.count = r->ifcall.count - i;
		return respond(r, nil);
	case FsFCctl:
		data_to_cstring(r);
		if(r->ifcall.count == 0)
			return respond(r, nil);
		if((errstr = message_client(f->client, (char *)r->ifcall.data)))
			return respond(r, errstr);
		r->ofcall.count = r->ifcall.count;
		return respond(r, nil);
	case FsFTctl:
		data_to_cstring(r);
		if(r->ifcall.count == 0)
			return respond(r, nil);

		if((errstr = message_view(f->view, (char *)r->ifcall.data)))
			return respond(r, errstr);
		r->ofcall.count = r->ifcall.count;
		return respond(r, nil);
	case FsFRctl:
		data_to_cstring(r);
		if(!r->ifcall.data || r->ifcall.count == 0)
			return respond(r, nil);
		if((errstr = message_root(r->ifcall.data)))
			return respond(r, errstr);
		r->ofcall.count = r->ifcall.count;
		return respond(r, nil);
	case FsFEvent:
		/* XXX: r->ifcall.count could be very large */
		buf = cext_emallocz(r->ifcall.count + 1);
		bcopy(r->ifcall.data, buf, r->ifcall.count);
		write_event(buf);
		free(buf);
		r->ofcall.count = r->ifcall.count;
		return respond(r, nil);
	case FsFgrabmod:
		data_to_cstring(r);
		/* XXX: This is too long/specific; it needs to be moved */
		{
			unsigned long mod;
			mod = mod_key_of_str(r->ifcall.data);
			if((mod != Mod1Mask) && (mod != Mod2Mask) && (mod != Mod3Mask)
					&& (mod != Mod4Mask) && (mod != Mod5Mask))
				return respond(r, Ebadvalue);
			cext_strlcpy(def.grabmod, r->ifcall.data, sizeof(def.grabmod));
			def.mod = mod;
			if(view)
				restack_view(sel);
			r->ofcall.count = r->ifcall.count;
			return respond(r, nil);
		}
	}
	respond(r, Enoperm);
}

void
fs_open(Req *r) {
	FidLink *fl;
	FileId *f = r->fid->aux;
	switch(f->tab.type) {
	case FsFEvent:
		fl = cext_emallocz(sizeof(FidLink));
		fl->fid = r->fid;
		fl->next = pending_event_fids;
		pending_event_fids = fl;
		break;
	}
	/* XXX */
	r->ofcall.mode = r->ifcall.mode;
	respond(r, nil);
}

void
fs_create(Req *r) {
	FileId *f = r->fid->aux;
	switch(f->tab.type) {
	default:
		/* XXX: This should be taken care of by the library */
		return respond(r, Enoperm);
	case FsDBar:
		if(!strlen(r->ifcall.name))
			return respond(r, Ebadvalue);
		create_bar(f->bar_p, r->ifcall.name);
		f = lookup_file(f, r->ifcall.name);
		if(!f)
			return respond(r, Enofile);
		r->ofcall.qid.type = f->tab.qtype;
		r->ofcall.qid.path = QID(f->tab.type, f->id);
		free_file(f);
		respond(r, nil);
		break;
	}
}

void
fs_remove(Req *r) {
	FileId *f = r->fid->aux;
	switch(f->tab.type) {
	default:
		/* XXX: This should be taken care of by the library */
		return respond(r, Enoperm);
	case FsFBar:
		destroy_bar(f->next->bar_p, f->bar);
		respond(r, nil);
		break;
	}
}

void
fs_clunk(Req *r) {
	Client *c;
	FidLink **fl, *ft;
	char *buf;
	int i;
	FileId *f = r->fid->aux;

	switch(f->tab.type) {
	case FsFColRules:
		update_rules(&f->rule->rule, f->rule->string);
		break;
	case FsFTagRules:
		update_rules(&f->rule->rule, f->rule->string);
		for(c=client; c; c=c->next)
			apply_rules(c);
		update_views();
		break;
	case FsFKeys:
		def.keys[def.keyssz] = '\0';
		update_keys();
		break;
	case FsFCtags:
		apply_tags(f->client, f->client->tags);
		update_views();
		draw_frame(f->client->sel);
		break;
	case FsFBar:
		buf = f->bar->buf;
		i = strlen(f->bar->buf);
		parse_colors(&buf, &i, &f->bar->color);
		while(buf[i - 1] == '\n')
			buf[--i] = '\0';
		strncpy(f->bar->data, buf, 255);
		draw_bar();
		break;
	case FsFEvent:
		for(fl=&pending_event_fids; *fl; fl=&(*fl)->next)
			if((*fl)->fid == r->fid) {
				ft = *fl;
				*fl = (*fl)->next;
				f = ft->fid->aux;
				free(f->buf);
				free(ft);
				break;
			}
		break;
	}
	respond(r, nil);
}

void
fs_flush(Req *r) {
	Req **t;
	for(t=&pending_event_reads; *t; t=(Req **)&(*t)->aux)
		if(*t == r->oldreq) {
			*t = (*t)->aux;
			respond(r->oldreq, Einterrupted);
			break;
		}
	respond(r, nil);
}

void
fs_freefid(Fid *f) {
	FileId *id, *tid;

	for(id=f->aux; id; id = tid) {
		tid = id->next;
		free_file(id);
	}
}
