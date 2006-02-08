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
	Fcall fcall;
	IXPClient client;
	IXPConn *rx;
	IXPConn *tx;
};

typedef struct Mount Mount;
struct Mount {
	unsigned short id;
	char wname[IXP_MAX_FLEN];
	char address[256];
};

Qid root_qid;
static Display *dpy;
static IXPServer srv;
static Mount **mount = nil;
static size_t mountsz = 0;
static size_t nmount = 0;
static unsigned char *msg[IXP_MAX_MSG];
static Bind **bind = nil;
static size_t bindsz = 0;
static size_t nbind = 0;

static void do_mnt_fcall(IXPConn *c);

static char version[] = "wmiifs - " VERSION ", (C)opyright MMIV-MMVI Anselm R. Garbe\n";
static char Enoserv[] = "server not found";

static void
usage()
{
    fprintf(stderr, "usage: wmiifs -a <address> [-v]\n");
    exit(1);
}

static Mount *
name_to_mount(char *name)
{
	size_t i;
	for(i = 0; i < nmount; i++)
		if(!strncmp(mount[i]->wname, name, sizeof(mount[i]->wname)))
			return mount[i];
	return nil;
}

static Bind *
fid_to_bind(unsigned int fid)
{
	size_t i;
	for(i = 0; i < nbind; i++)
		if(bind[i]->fid == fid)
			return bind[i];
	return nil;
}

static Bind *
rx_to_bind(IXPConn *c)
{
	size_t i;
	for(i = 0; i < nbind; i++)
		if(bind[i]->rx == c)
			return bind[i];
	return nil;
}

static void
xunbind(Bind *b) {
	cext_array_detach((void **)srv.conn, b->rx, &srv.connsz);
	free(b->rx);
	cext_array_detach((void **)bind, b, &bindsz);
	nbind--;
	ixp_client_deinit(&b->client);
	free(b);
}

static void
close_rx_conn(IXPConn *c)
{
	Bind *b = rx_to_bind(c);
	if(!b)
		return;
	xunbind(b);
}

static Bind *
xbind(char *address, unsigned int fid, IXPConn *tx)
{
	char addr[256];
	Bind *b = cext_emallocz(sizeof(Bind));

	cext_strlcpy(addr, address, sizeof(addr)); 
	if(ixp_client_init(&b->client, addr, fid) == -1) {
		free(b);
		return nil;
	}
	b->rx = ixp_server_open_conn(&srv, b->client.fd, do_mnt_fcall, close_rx_conn);
	b->tx = tx;
	bind = (Bind **)cext_array_attach((void **)bind, b, sizeof(Bind *), &bindsz);
	nbind++;
	return b;
}

static void
xunmount(Mount *mnt) /* called by xremove */
{
	cext_array_detach((void **)mount, mnt, &mountsz);
	nmount--;
	free(mnt);
}

static char *
xmount(char *arg)
{
	char *address = arg;
	char *p, *wname = strchr(address, ' ');
	Mount *mnt;

	*wname = 0;
	wname++;
	if(*wname == '/')
		wname++;
	p = strchr(wname, '/');
	if(p)
		*p = 0; /* mount name is not allowed to contain slashes */

	if(!address || !wname || !*address || !*wname)
		return Enoserv;

	if(name_to_mount(wname))
		return "already mounted";

	mnt = cext_emallocz(sizeof(Mount));
	cext_strlcpy(mnt->wname, wname, sizeof(mnt->wname));
	cext_strlcpy(mnt->address, address, sizeof(mnt->address));
	mount = (Mount **)cext_array_attach((void **)mount, mnt, sizeof(Mount *), &mountsz);
	nmount++;

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
	if(name_to_mount(name))
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
		if(!(mnt = name_to_mount(wname)))
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
		if(!(mnt = name_to_mount(name)))
			return -1;
		return mkstat(stat, dir, name, 0, DMDIR | DMREAD | DMEXEC | DMMOUNT);
		break;
	}
	return 0;
}

static void
mntrespond(IXPConn *c, Fcall *fcall)
{
	Bind *b = rx_to_bind(c);
	/*fprintf(stderr, "mntrespond, welcome: fcall->id=%d\n", fcall->id);*/
	memcpy(&b->fcall, fcall, sizeof(Fcall));
	ixp_server_respond_fcall(b->tx, &b->fcall);
}

static void
mntwalk(IXPConn *c, Fcall *fcall)
{
	Qid dir = root_qid;
	Bind *b = rx_to_bind(c);
	unsigned int nwqid;
	IXPMap *m;

	if(!(m = ixp_server_fid2map(b->tx, b->fcall.fid))) {
		ixp_server_respond_error(b->tx, &b->fcall, Enofid);
		xunbind(b);
		return;
	}
	
	if(mkqid(&dir, b->fcall.wname[0], &b->fcall.wqid[0], True) == -1) {
	/*fprintf(stderr, "%s", "wmiifs: mntwalk cannot make qid\n");*/
		ixp_server_respond_error(b->tx, &b->fcall, Enofile);
		xunbind(b);
		return;
	}
	dir = b->fcall.wqid[0]; /* mount point */
	for(nwqid = 0; nwqid < fcall->nwqid; nwqid++)
		b->fcall.wqid[nwqid + 1] = fcall->wqid[nwqid];
	nwqid++;

	if(!nwqid) {
	/*fprintf(stderr, "%s", "wmiifs: mntwalk nwqid wrong \n");*/
		ixp_server_respond_error(b->tx, &b->fcall, Enofile);
		xunbind(b);
		return;
	}

	/* a fid will only be valid, if the walk was complete */
	/*fprintf(stderr, "nwqid=%d b->fcall.nwname=%d", nwqid, b->fcall.nwname);*/
	if(nwqid == b->fcall.nwname) {
		if(b->fcall.fid != b->fcall.newfid) {
			m = cext_emallocz(sizeof(IXPMap));
			b->tx->map = (IXPMap **)
				cext_array_attach((void **)b->tx->map, m, sizeof(IXPMap *), &b->tx->mapsz);
		}
		m->qid = dir; /* mount point, even if not */
		m->fid = b->fcall.newfid;
	/*fprintf(stderr, "%s", "wmiifs: fffaoooo\n");*/
	}
	/*fprintf(stderr, "wmiifs: success mntwalk b-tx=%x  m->fid = %d (b->fcall.newfid=%d)\n",
			b->tx, m->fid, b->fcall.newfid);*/
	b->fid = m->fid;
	b->fcall.id = RWALK;
	b->fcall.nwqid = nwqid;
	ixp_server_respond_fcall(b->tx, &b->fcall);
}

static char *
mntrequest(Bind *b, Fcall *tx)
{
	unsigned int msize;
	/*fprintf(stderr, "mntrequest: welcome, tx=%x tx->id=%d fid=%d\n", b->tx, tx->id, tx->fid);*/
	memcpy(&b->client.fcall, tx, sizeof(Fcall));
	memcpy(&b->fcall, tx, sizeof(Fcall));
    msize = ixp_fcall_to_msg(&b->client.fcall, msg, IXP_MAX_MSG);
	b->client.errstr = 0;
	if(ixp_send_message(b->client.fd, msg, msize, &b->client.errstr) != msize) {
		xunbind(b);
		return Enofile;
	}
	return nil;
}

static char *
xwalk(IXPConn *c, Fcall *fcall)
{
	unsigned short nwqid = 0;
	Qid dir = root_qid;
	IXPMap *m;
	Mount *mnt;

	/*fprintf(stderr, "xwalk: welcome fcall->wname[0]=%s fcall->fid=%d\n", fcall->wname[0], fcall->fid);*/
	if(!(m = ixp_server_fid2map(c, fcall->fid)))
		return Enofid;
	if(fcall->fid != fcall->newfid && (ixp_server_fid2map(c, fcall->newfid)))
		return Enofid;
	/*fprintf(stderr, "xwalk: fcall->nwname=%d\n", fcall->nwname);*/
	if(fcall->nwname) {
		dir = m->qid;
		if((mnt = name_to_mount(fcall->wname[0]))) {
			unsigned int i;
			Bind *b;
		   
			if(!(b = xbind(mnt->address, fcall->fid, c)))
				return Enoserv;

			memcpy(&b->client.fcall, fcall, sizeof(Fcall));
			memcpy(&b->fcall, fcall, sizeof(Fcall));
			/*fprintf(stderr, "xwalk: sending fid=%d (newfid=%d)\n", b->client.fcall.fid, b->client.fcall.newfid);*/

			b->client.fcall.nwname = fcall->nwname - 1;
			for(i = 1; i < fcall->nwname; i++)
				cext_strlcpy(b->client.fcall.wname[i - 1], fcall->wname[i],
						     sizeof(b->client.fcall.wname[i - 1]));

			i = ixp_fcall_to_msg(&b->client.fcall, msg, IXP_MAX_MSG);
			b->client.errstr = 0;
			if(ixp_send_message(b->client.fd, msg, i, &b->client.errstr) != i) {
				xunbind(b);
				return Enofile;
			}
			return nil;
		}
		else {
			for(nwqid = 0; (nwqid < fcall->nwname)
				&& !mkqid(&dir, fcall->wname[nwqid], &fcall->wqid[nwqid], True); nwqid++) {
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
xcreate(IXPConn *c, Fcall *fcall)
{
	IXPMap *m = ixp_server_fid2map(c, fcall->fid);
	Bind *b;

	if(!m)
		return Enofid;
	if((b = fid_to_bind(m->fid)))
		return mntrequest(b, fcall);
	return Enoperm;
}

static char *
xopen(IXPConn *c, Fcall *fcall)
{
	IXPMap *m = ixp_server_fid2map(c, fcall->fid);
	Bind *b;

	if(!m)
		return Enofid;
	if((b = fid_to_bind(m->fid)))
		return mntrequest(b, fcall);
	if(!(fcall->mode | IXP_OREAD) && !(fcall->mode | IXP_OWRITE))
		return Enomode;
	fcall->id = ROPEN;
	fcall->qid = m->qid;
	fcall->iounit = 2048;
	ixp_server_respond_fcall(c, fcall);
	return nil;
}

static void
mntclunkremove(IXPConn *c, Fcall *fcall)
{
	Bind *b = rx_to_bind(c);
	IXPMap *m;

	if(!(m = ixp_server_fid2map(b->tx, b->fcall.fid))) {
		ixp_server_respond_error(b->tx, &b->fcall, Enofid);
		xunbind(b);
		return;
	}
	cext_array_detach((void **)b->tx->map, m, &b->tx->mapsz);
	free(m);
	ixp_server_respond_fcall(b->tx, &b->fcall);
	xunbind(b);
}

static char *
xremove(IXPConn *c, Fcall *fcall)
{
	IXPMap *m = ixp_server_fid2map(c, fcall->fid);
	Bind *b;

	if(!m)
		return Enofid;
	if((b = fid_to_bind(m->fid)))
		return mntrequest(b, fcall);
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
	Bind *b;
	int i;

	if(!m)
		return Enofid;
	if((b = fid_to_bind(m->fid)))
		return mntrequest(b, fcall);
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
	Bind *b;
	char *name;
	/*fprintf(stderr, "xstat: welcome, c=%x fcall->fid=%d\n", c, fcall->fid);*/
	if(!m)
		return Enofid;
	/*fprintf(stderr, "xstat: now fetching bind, fcall->fid=%d\n", fcall->fid);*/
	if((b = fid_to_bind(m->fid)))
		return mntrequest(b, fcall);
	name = qid_to_name(&m->qid);
	if(!type_to_stat(&fcall->stat, name, &m->qid))
		return Enofile;
	fcall->id = RSTAT;
	ixp_server_respond_fcall(c, fcall);
	return nil;
}

static char *
xwrite(IXPConn *c, Fcall *fcall)
{
	char *p, buf[256];
	IXPMap *m = ixp_server_fid2map(c, fcall->fid);
	Bind *b;

	if(!m)
		return Enofid;
	if((b = fid_to_bind(m->fid)))
		return mntrequest(b, fcall);
	switch (qpath_type(m->qid.path)) {
	case Fctl:
		if(fcall->count > sizeof(buf) - 1)
			return Enocommand;
		memcpy(buf, fcall->data, fcall->count);
		buf[fcall->count] = 0;
		if(!strncmp(buf, "quit", 5)) {
			srv.running = 0;
			break;
		}
		else if(!strncmp(buf, "mount ", 6)) {
			p = xmount(&buf[6]);
			if(p)
				return p;
			break;
		}
		else if(!strncmp(buf, "unmount ", 8)) {
			Mount *mnt;
			char *mstr = &buf[8];
			if(*mstr == '/')
				mstr++;
			p = strchr(mstr, '/');
			if(p)
				*p = 0; /* mount name is not allowed to contain slashes */
			if((mnt = name_to_mount(mstr))) {
				xunmount(mnt);
				break;
			}
			return "no such mount point";
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

char *
xclunk(IXPConn *c, Fcall *fcall)
{
    IXPMap *m = ixp_server_fid2map(c, fcall->fid);
	Bind *b;

    if(!m)
        return Enofid;
	if((b = fid_to_bind(m->fid)))
		return mntrequest(b, fcall);
	cext_array_detach((void **)c->map, m, &c->mapsz);
    free(m);
    fcall->id = RCLUNK;
	ixp_server_respond_fcall(c, fcall);
    return nil;
}

static void
do_mnt_fcall(IXPConn *c)
{
	static Fcall fcall;
	unsigned int msize;

	if((msize = ixp_server_receive_fcall(c, &fcall))) {
		/*fprintf(stderr, "mntfcall=%d\n", fcall.id);*/
		switch(fcall.id) {
		case RWALK: mntwalk(c, &fcall); break;
		case RREMOVE:
		case RCLUNK: mntclunkremove(c, &fcall); break;
		case RCREATE:
		case ROPEN:
		case RREAD:
		case RWRITE:
		case RSTAT: 
		case RERROR: mntrespond(c, &fcall); break;
		}
	}
}

static void
do_fcall(IXPConn *c)
{
	static Fcall fcall;
	unsigned int msize;
	char *errstr;

	if((msize = ixp_server_receive_fcall(c, &fcall))) {
		/*fprintf(stderr, "locfcall=%d\n", fcall.id);*/
		switch(fcall.id) {
		case TVERSION: errstr = wmii_ixp_version(c, &fcall); break;
		case TATTACH: errstr = wmii_ixp_attach(c, &fcall); break;
		case TWALK: errstr = xwalk(c, &fcall); break;
		case TREMOVE: errstr = xremove(c, &fcall); break;
		case TCREATE: errstr = xcreate(c, &fcall); break;
		case TOPEN: errstr = xopen(c, &fcall); break;
		case TREAD: errstr = xread(c, &fcall); break;
		case TWRITE: errstr = xwrite(c, &fcall); break;
		case TCLUNK: errstr = xclunk(c, &fcall); break;
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
