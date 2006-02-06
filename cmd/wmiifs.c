/*
 * (C)opyright MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "wmii.h"

/*
 * filesystem specification
 * / 					Droot
 * /ctl					Fctl 		command interface
 * /foobar				Dmount
 */

/* 8-bit qid.path.type */
enum {                          
    Droot,
    Dmount,
	Fctl
};

typedef struct Bind Bind;
struct Bind {
	unsigned int fid;
	IXPClient c;
};

typedef struct Mount Mount;
struct Mount {
	unsigned short id;
	char wname[IXP_MAX_FLEN];
	char address[256];
	Bind **bind;
	size_t bindsz;
	size_t nbind;
};

Qid root_qid;
static Display *dpy;
static IXPServer srv;
static Mount **mount = nil;
static size_t mountsz = 0;
static size_t nmount = 0;

static char version[] = "wmiifs - " VERSION ", (C)opyright MMIV-MMVI Anselm R. Garbe\n";

static void
usage()
{
    fprintf(stderr, "usage: wmiifs -a <address> [-v]\n");
    exit(1);
}

static Mount *
mount_of_name(char *name)
{
	size_t i;
	for(i = 0; i < nmount; i++)
		if(!strncmp(mount[i]->wname, name, sizeof(mount[i]->wname)))
			return mount[i];
	return nil;
}

static int
index_of_id(unsigned short id)
{
	int i;
	for(i = 0; i < nmount; i++)
		if(mount[i]->id == id)
			return i;
	return -1;
}

/* IXP stuff */

static unsigned long long
mkqpath(unsigned char type, unsigned short id)
{
	return ((unsigned long long) id << 8) | (unsigned long long) type;
}

static unsigned char
qpath_type(unsigned long long path)
{
	return path & 0xff;
}

static unsigned short
qpath_id(unsigned long long path)
{
	return (path >> 8) & 0xffff;
}

static char *
qid_to_name(Qid *qid)
{
	unsigned char type = qpath_type(qid->path);
	unsigned short id = qpath_id(qid->path);
	int i;

	if(id && ((i = index_of_id(id)) == -1))
		return nil;
	switch(type) {
		case Droot: return "/"; break;
		case Dmount: return mount[i]->wname; break;
		case Fctl: return "ctl"; break;
		default: return nil; break;
	}
}

static int
name_to_type(char *name)
{
	if(!name || !name[0] || !strncmp(name, "/", 2) || !strncmp(name, "..", 3))
		return Droot;
	if(!strncmp(name, "ctl", 4))
		return Fctl;
	if(mount_of_name(name))
		return Dmount;
	return -1;
}

static int
mkqid(Qid *dir, char *wname, Qid *new, Bool iswalk)
{
	Mount *mnt;
	int type = name_to_type(wname);
   
	if((dir->type != IXP_QTDIR) || (type == -1))
		return -1;
	
	new->dtype = qpath_type(dir->path);
	new->version = 0;
	switch(type) {
	case Droot:
		*new = root_qid;
		break;
	case Dmount:
		if(!(mnt = mount_of_name(wname)))
			return -1;
		new->type = IXP_QTDIR;
		new->path = mkqpath(type, mnt->id);
		break;
	default:
		new->type = IXP_QTFILE;
		new->path = mkqpath(type, 0);
		break;
	}
	return 0;
}

static char * 
xwalk(IXPConn *c, Fcall *fcall)
{
	unsigned short nwqid = 0;
	Qid dir = root_qid;
	IXPMap *m;
	Mount *mnt;

	if(!(m = ixp_server_fid2map(c, fcall->fid)))
		return Enofid;
	if(fcall->fid != fcall->newfid && (ixp_server_fid2map(c, fcall->newfid)))
		return Enofid;
	if(fcall->nwname) {
		dir = m->qid;
		if((mnt = mount_of_name(fcall->wname[0])) && (fcall->nwname > 1)) {
			Bind *b = cext_emallocz(sizeof(Bind));
			int i;

			mnt->bind = (Bind **)cext_array_attach((void **)mnt->bind, b, sizeof(Bind *), &mnt->bindsz);
			mnt->nbind++;

			/* open socket */
			if(ixp_client_init(&b->c, mnt->address, mnt->nbind - 1) == -1)
				return Enofile;

			b->c.fcall.id = TWALK;
			b->c.fcall.fid = fcall->fid;
			b->c.fcall.newfid = fcall->newfid;
			b->c.fcall.nwname = fcall->nwname - 1;
			for(i = 1; i < fcall->nwname; i++)
				cext_strlcpy(b->c.fcall.wname[i - 1], fcall->wname[i], sizeof(b->c.fcall.wname[i - 1]));

			if(ixp_client_do_fcall(&b->c) == -1);
				/* TODO: unbind client, also in other cases, THINK */

			!mkqid(&dir, fcall->wname[0], &fcall->wqid[0], True);
			for(i = 0; i < b->c.fcall.nwname; i++)
				fcall->wqid[i + 1] = b->c.fcall.wqid[i];
			nwqid = i + 1;
		}
		else {
			for(nwqid = 0; (nwqid < fcall->nwname)
				&& !mkqid(&dir, fcall->wname[nwqid], &fcall->wqid[nwqid], True); nwqid++) {
				/*fprintf(stderr, "wname=%s nwqid=%d\n", fcall->wname[nwqid], nwqid);*/
				dir = fcall->wqid[nwqid];
			}
			if(!nwqid)
				return Enofile;
		}
	}
	/* a fid will only be valid, if the walk was complete */
	if(nwqid == fcall->nwname) {
		if(fcall->fid != fcall->newfid) {
			m = cext_emallocz(sizeof(IXPMap));
			c->map = (IXPMap **)cext_array_attach((void **)c->map,
						m, sizeof(IXPMap *), &c->mapsz);
		}
		m->qid = dir;
		m->fid = fcall->newfid;
	}
	fcall->id = RWALK;
	fcall->nwqid = nwqid;
	ixp_server_respond_fcall(c, fcall);
	return nil;
}

static char *
xopen(IXPConn *c, Fcall *fcall)
{
	IXPMap *m = ixp_server_fid2map(c, fcall->fid);

	if(!m)
		return Enofid;
	if(!(fcall->mode | IXP_OREAD) && !(fcall->mode | IXP_OWRITE))
		return Enomode;
	fcall->id = ROPEN;
	fcall->qid = m->qid;
	fcall->iounit = 2048;
	ixp_server_respond_fcall(c, fcall);
	return nil;
}

static unsigned int
mkstat(Stat *stat, Qid *dir, char *name, unsigned long long length, unsigned int mode)
{
	stat->mode = mode;
	stat->atime = stat->mtime = time(0);
	cext_strlcpy(stat->uid, getenv("USER"), sizeof(stat->uid));
	cext_strlcpy(stat->gid, getenv("USER"), sizeof(stat->gid));
	cext_strlcpy(stat->muid, getenv("USER"), sizeof(stat->muid));

	cext_strlcpy(stat->name, name, sizeof(stat->name));
	stat->length = length;
	mkqid(dir, name, &stat->qid, False);
	return ixp_sizeof_stat(stat);
}

static unsigned int
type_to_stat(Stat *stat, char *name, Qid *dir)
{
	Mount *mnt;
	int type = name_to_type(name);

	switch (type) {
	case Droot:
		return mkstat(stat, dir, name, 0, DMDIR | DMREAD | DMEXEC);
		break;
	case Fctl:
		return mkstat(stat, dir, name, 0, DMWRITE);
		break;
	case Dmount:
		if(!(mnt = mount_of_name(name)))
			return -1;
		return mkstat(stat, dir, name, 0, DMREAD | DMWRITE | DMMOUNT);
		break;
	}
	return 0;
}

static char *
xremove(IXPConn *c, Fcall *fcall)
{
	IXPMap *m = ixp_server_fid2map(c, fcall->fid);
	unsigned short id = qpath_id(m->qid.path);
	int i;

	if(!m)
		return Enofid;
	if(id && ((i = qpath_id(id)) == -1))
		return Enofile;
	if((qpath_type(m->qid.path) == Dmount) && (i < nmount)) {
		/* TODO: umount */
		fcall->id = RREMOVE;
		ixp_server_respond_fcall(c, fcall);
		return nil;
	}
	return Enoperm;
}

static char *
xread(IXPConn *c, Fcall *fcall)
{
	Stat stat;
	IXPMap *m = ixp_server_fid2map(c, fcall->fid);
	unsigned short id;
	unsigned char *p = fcall->data;
	unsigned int len = 0;
	int i;

	if(!m)
		return Enofid;
	id = qpath_id(m->qid.path);
	if(id && ((i = index_of_id(id)) == -1))
		return Enofile;

	fcall->count = 0;
	if(fcall->offset) {
		switch (qpath_type(m->qid.path)) {
		case Droot:
			/* jump to offset */
			len = type_to_stat(&stat, "ctl", &m->qid);
			for(i = 0; i < nmount; i++) {
				len += type_to_stat(&stat, mount[i]->wname, &m->qid);
				if(len <= fcall->offset)
					continue;
				break;
			}
			/* offset found, proceeding */
			for(; i < nmount; i++) {
				len = type_to_stat(&stat, mount[i]->wname, &m->qid);
				if(fcall->count + len > fcall->iounit)
					break;
				fcall->count += len;
				p = ixp_enc_stat(p, &stat);
			}
			break;
		default:
			break;
		}
	}
	else {
		switch (qpath_type(m->qid.path)) {
		case Droot:
			fcall->count = type_to_stat(&stat, "ctl", &m->qid);
			p = ixp_enc_stat(p, &stat);
			for(i = 0; i < nmount; i++) {
				len = type_to_stat(&stat, mount[i]->wname, &m->qid);
				if(fcall->count + len > fcall->iounit)
					break;
				fcall->count += len;
				p = ixp_enc_stat(p, &stat);
			}
			break;
		case Dmount:
			/* TODO: */
			break;
		case Fctl:
			return Enoperm;
			break;
		default:
			return "invalid read";
			break;
		}
	}
	fcall->id = RREAD;
	ixp_server_respond_fcall(c, fcall);
	return nil;
}

static char *
xstat(IXPConn *c, Fcall *fcall)
{
	IXPMap *m = ixp_server_fid2map(c, fcall->fid);
	char *name;

	if(!m)
		return Enofid;
	name = qid_to_name(&m->qid);
	/*fprintf(stderr, "xstat: name=%s\n", name);*/
	if(!type_to_stat(&fcall->stat, name, &m->qid))
		return Enofile;
	fcall->id = RSTAT;
	ixp_server_respond_fcall(c, fcall);
	return nil;
}

static char *
xwrite(IXPConn *c, Fcall *fcall)
{
	char buf[256];
	IXPMap *m = ixp_server_fid2map(c, fcall->fid);

	if(!m)
		return Enofid;
	switch (qpath_type(m->qid.path)) {
	case Fctl:
		if(fcall->count == 4) {
			memcpy(buf, fcall->data, 4);
			buf[4] = 0;
			if(!strncmp(buf, "quit", 5)) {
				srv.running = 0;
				break;
			}
		}
		return Enocommand;
		break;
	default:
		return "invalid write";
		break;
	}
	fcall->id = RWRITE;
	ixp_server_respond_fcall(c, fcall);
	return nil;
}

static void
do_fcall(IXPConn *c)
{
	static Fcall fcall;
	unsigned int msize;
	char *errstr;

	if((msize = ixp_server_receive_fcall(c, &fcall))) {
		/*fprintf(stderr, "fcall=%d\n", fcall.id);*/
		switch(fcall.id) {
		case TVERSION: errstr = wmii_ixp_version(c, &fcall); break;
		case TATTACH: errstr = wmii_ixp_attach(c, &fcall); break;
		case TWALK: errstr = xwalk(c, &fcall); break;
		case TREMOVE: errstr = xremove(c, &fcall); break;
		case TOPEN: errstr = xopen(c, &fcall); break;
		case TREAD: errstr = xread(c, &fcall); break;
		case TWRITE: errstr = xwrite(c, &fcall); break;
		case TCLUNK: errstr = wmii_ixp_clunk(c, &fcall); break;
		case TSTAT: errstr = xstat(c, &fcall); break;
		default: errstr = Enofunc; break;
		}
		if(errstr)
			ixp_server_respond_error(c, &fcall, errstr);
	}
}

static void
new_ixp_conn(IXPConn *c)
{
	int fd = ixp_accept_sock(c->fd);
	
	if(fd >= 0)
		ixp_server_open_conn(c->srv, fd, do_fcall, ixp_server_close_conn);
}

static void
check_x_event(IXPConn *c)
{
    XEvent ev;
    while(XPending(dpy))
        XNextEvent(dpy, &ev);
    /* why check them? because X won't kill wmiifs when X dies */
}

/* main */

int
main(int argc, char *argv[])
{
    int i;
	char *errstr;
	char *address = nil;

    /* command line args */
    for(i = 1; (i < argc) && (argv[i][0] == '-'); i++) {
        switch (argv[i][1]) {
        case 'v':
            fprintf(stdout, "%s", version);
            exit(0);
            break;
        case 'a':
            if(i + 1 < argc)
                address = argv[++i];
            else
                usage();
            break;
        default:
            usage();
            break;
        }
    }

    /* just for the case X crashes/gets quit */
    dpy = XOpenDisplay(0);
    if(!dpy) {
        fprintf(stderr, "%s", "wmiifs: cannot open display\n");
        exit(1);
    }

	i = ixp_create_sock(address, &errstr);
	if(i < 0) {
        fprintf(stderr, "wmiifs: fatal: %s\n", errstr);
		exit(1);
	}

	/* IXP server */
	ixp_server_open_conn(&srv, i, new_ixp_conn, ixp_server_close_conn);
    root_qid.type = IXP_QTDIR;
    root_qid.version = 0;
    root_qid.path = mkqpath(Droot, 0);

	/* X server */
	ixp_server_open_conn(&srv, ConnectionNumber(dpy), check_x_event, nil);

    /* main loop */
	errstr = ixp_server_loop(&srv);
	if(errstr)
    	fprintf(stderr, "wmiibar: fatal: %s\n", errstr);

	/* cleanup */
	ixp_server_close(&srv);
	XCloseDisplay(dpy);

	return errstr ? 1 : 0;
}
