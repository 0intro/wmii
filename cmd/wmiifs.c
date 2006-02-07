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

typedef struct Mount Mount;
struct Mount {
	unsigned short id;
	char wname[IXP_MAX_FLEN];
	char address[256];
	IXPClient client;
	IXPConn *respond;
};

Qid root_qid;
static Display *dpy;
static IXPServer srv;
static Mount **mount = nil;
static size_t mountsz = 0;
static size_t nmount = 0;
static unsigned short tag = 0;
static unsigned char *msg[IXP_MAX_MSG];

static void rx_fcall(IXPConn *c);

static char version[] = "wmiifs - " VERSION ", (C)opyright MMIV-MMVI Anselm R. Garbe\n";
static char Enoserv[] = "server not found";

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

static void
xunmount(Mount *mnt)
{
	cext_array_detach((void **)mount, mnt, &mountsz);
	nmount--;
	ixp_client_deinit(&mnt->client);
	free(mnt);
}

static void
xclose_mount(IXPConn *c)
{
	size_t i;
	cext_array_detach((void **)srv.conn, c, &srv.connsz);
	for(i = 0; i < nmount; i++)
		if(mount[i]->respond == c) {
			xunmount(mount[i]);
			break;
		}
	if(c->map) {
		for(i = 0; (i < c->mapsz) && c->map[i]; i++)
			free(c->map[i]);
		free(c->map);
	}
	if(c->pend) {
		for(i = 0; (i < c->pendsz) && c->pend[i]; i++)
			free(c->pend[i]);
		free(c->pend);
	}
	free(c);
}


static char *
xmount(char *arg)
{
	static unsigned int id = 1;
	char *address = arg;
	char *wname = strchr(address, ' ');
	Mount *mnt;

	*wname = 0;
	wname++;

	if(!address || !wname || !*address || !*wname)
		return Enoserv;

	mnt = cext_emallocz(sizeof(Mount));
	cext_strlcpy(mnt->wname, wname, sizeof(mnt->wname));
	cext_strlcpy(mnt->address, address, sizeof(mnt->address));
	mount = (Mount **)cext_array_attach((void **)mount, mnt, sizeof(Mount *), &mountsz);
	nmount++;

	/* open socket */
	if(ixp_client_init(&mnt->client, mnt->address, id++) == -1) {
		xunmount(mnt);
		return Enoserv;
	}

	mnt->respond = ixp_server_open_conn(&srv, mnt->client.fd, rx_fcall, xclose_mount);
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
txwalk(IXPConn *c, Fcall *fcall)
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
			unsigned int i;

			ixp_fcall_to_msg(fcall, msg, IXP_MAX_MSG);
			ixp_msg_to_fcall(msg, IXP_MAX_MSG, &mnt->client.fcall);
			fcall->tag = mnt->client.fcall.tag = tag++;

			mnt->client.fcall.nwname = fcall->nwname - 1;
			for(i = 1; i < fcall->nwname; i++)
				cext_strlcpy(mnt->client.fcall.wname[i - 1], fcall->wname[i],
					      		sizeof(mnt->client.fcall.wname[i - 1]));

    		i = ixp_fcall_to_msg(&mnt->client.fcall, msg, IXP_MAX_MSG);
			mnt->client.errstr = 0;
			if(ixp_send_message(mnt->client.fd, msg, i, &mnt->client.errstr) != i) {
				xclose_mount(mnt->respond);
				return Enofile;
			}
			/* message will be received by mnt->respond */
			ixp_server_enqueue_fcall(mnt->respond, fcall);
			return nil;
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

static Mount *
map2mount(IXPMap *m)
{
	unsigned short id;
	int i;
	if((id = qpath_id(m->qid.path))) { /* remote */
   		i = index_of_id(id);
	    if((i >= 0) && (i < nmount))
			return mount[i];
	}
	return nil;
}

static char *
txopen(IXPConn *c, Fcall *fcall)
{
	IXPMap *m = ixp_server_fid2map(c, fcall->fid);
	Mount *mnt;

	if(!m)
		return Enofid;
	if((mnt = map2mount(m))) { /* remote */
		unsigned int msize;
		ixp_fcall_to_msg(fcall, msg, IXP_MAX_MSG);
		ixp_msg_to_fcall(msg, IXP_MAX_MSG, &mnt->client.fcall);
		fcall->tag = mnt->client.fcall.tag = tag++;

    	msize = ixp_fcall_to_msg(&mnt->client.fcall, msg, IXP_MAX_MSG);
		mnt->client.errstr = 0;
		if(ixp_send_message(mnt->client.fd, msg, msize, &mnt->client.errstr) != msize) {
			xclose_mount(mnt->respond);
			return Enofile;
		}
		/* message will be received by mnt->respond */
		ixp_server_enqueue_fcall(mnt->respond, fcall);
		return nil;
	}
	else { /* local */
		if(!(fcall->mode | IXP_OREAD) && !(fcall->mode | IXP_OWRITE))
			return Enomode;
		fcall->id = ROPEN;
		fcall->qid = m->qid;
		fcall->iounit = 2048;
		ixp_server_respond_fcall(c, fcall);
	}
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
txremove(IXPConn *c, Fcall *fcall)
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
txread(IXPConn *c, Fcall *fcall)
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
txwrite(IXPConn *c, Fcall *fcall)
{
	char *err;
	char buf[256];
	IXPMap *m = ixp_server_fid2map(c, fcall->fid);

	if(!m)
		return Enofid;
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
			err = xmount(&buf[6]);
			if(err)
				return err;
			break;
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
txclunk(IXPConn *c, Fcall *fcall)
{
    IXPMap *m = ixp_server_fid2map(c, fcall->fid);

    if(!m)
        return Enofid;
	cext_array_detach((void **)c->map, m, &c->mapsz);
    free(m);
    fcall->id = RCLUNK;
	ixp_server_respond_fcall(c, fcall);
    return nil;
}

static void
rx_fcall(IXPConn *c)
{
	static Fcall fcall;
	unsigned int msize;
	char *errstr;

	if((msize = ixp_server_receive_fcall(c, &fcall))) {
		/*fprintf(stderr, "fcall=%d\n", fcall.id);*/
		switch(fcall.id) {
		case RWALK: errstr = rxwalk(c, &fcall); break;
		case RREMOVE: errstr = rxremove(c, &fcall); break;
		case RCREATE: errstr = rxcreate(c, &fcall); break;
		case ROPEN: errstr = rxopen(c, &fcall); break;
		case RREAD: errstr = rxread(c, &fcall); break;
		case RWRITE: errstr = rxwrite(c, &fcall); break;
		case RCLUNK: errstr = rxclunk(c, &fcall); break;
		case RSTAT: errstr = rxstat(c, &fcall); break;
		default: errstr = Enofunc; break;
		}
		if(errstr)
			ixp_server_respond_error(c, &fcall, errstr);
	}
}

static void
tx_fcall(IXPConn *c)
{
	static Fcall fcall;
	unsigned int msize;
	char *errstr;

	if((msize = ixp_server_receive_fcall(c, &fcall))) {
		/*fprintf(stderr, "fcall=%d\n", fcall.id);*/
		switch(fcall.id) {
		case TVERSION: errstr = wmii_ixp_version(c, &fcall); break;
		case TATTACH: errstr = wmii_ixp_attach(c, &fcall); break;
		case TWALK: errstr = txwalk(c, &fcall); break;
		case TREMOVE: errstr = txremove(c, &fcall); break;
		case TCREATE: errstr = txcreate(c, &fcall); break;
		case TOPEN: errstr = txopen(c, &fcall); break;
		case TREAD: errstr = txread(c, &fcall); break;
		case TWRITE: errstr = txwrite(c, &fcall); break;
		case TCLUNK: errstr = txclunk(c, &fcall); break;
		case TSTAT: errstr = txstat(c, &fcall); break;
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
		ixp_server_open_conn(c->srv, fd, tx_fcall, ixp_server_close_conn);
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
