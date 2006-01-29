/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <sys/socket.h>

#include "../libixp2/ixp.h"
#include "blitz.h"

/*
 * filesystem specification
 * / 					Droot
 * /display				Fdisplay	'north', 'south', 'none'
 * /font				Ffont		<xlib font name>
 * /new					Fnew 		returns id of new item
 * /event				Fevent
 * /default/ 			Ditem
 * /default/color		Fcolor		<#RRGGBB> <#RRGGBB> <#RRGGBB>
 * /1/					Ditem
 * /1/data 				Fdata		<arbitrary data which gets displayed>
 * /1/color				Fcolor		<#RRGGBB> <#RRGGBB> <#RRGGBB>
 */

/* 8-bit qid.path.type */
enum {                          
    Droot,
    Ditem,
	Fctl,
    Fdisplay,
    Ffont,
    Fevent,
    Fdata,                      /* data to display */
    Fcolor
};

typedef struct {
    char data[256];
	char color[24];
    unsigned long bg;
    unsigned long fg;
    unsigned long border;
} Item;

static char E9pversion[] = "9P version not supported";
static char Enoperm[] = "permission denied";
static char Enofid[] = "fid not assigned";
static char Enofile[] = "file not found";
static char Enomode[] = "mode not supported";
static char Enofunc[] = "function not supported";

static unsigned char *msg[IXP_MAX_MSG];
char *errstr = 0;
static size_t nitem = 0;
static size_t itemsz = 0;
static Item **item = 0;
static char *address = nil;
static pid_t mypid = 0;
static IXPServer srv = { 0 };
static Qid root_qid;
static Display *dpy;
static int screen_num;
static char *font = nil;
static Align align = SOUTH;
/*
static XFontStruct *xfont;
static GC gc;
static Window win;
static XRectangle geom;
static Pixm pm;

static Draw zero_draw = { 0 };
*/

static char *version[] = {
    "wmiibar - window manager improved bar - " VERSION "\n"
        "  (C)opyright MMIV-MMVI Anselm R. Garbe\n", 0
};

static void
usage()
{
    fprintf(stderr, "%s %d",
            "usage: wmiibar -a <server address> [-v]\n"
            "      -a    server address \n"
            "      -v    version info\n", 0);
    exit(1);
}

static int
dummy_error_handler(Display * dpy, XErrorEvent * err)
{
    return 0;
}

static void
new_item()
{
	size_t i;
	if(!item) {
		itemsz = 2;
		item = cext_emallocz(sizeof(Item *) * itemsz);
	}
	if(nitem + 1 >= itemsz) {
		Item **tmp = item;
		itemsz *= 2;
		item = cext_emallocz(sizeof(Item *) * itemsz);
		for(i = 0; tmp[i]; i++)
			item[i] = tmp[i];
		free(tmp);
	}
	item[nitem++] = cext_emallocz(sizeof(Item));
	if(nitem > 1)
		cext_strlcpy(item[nitem - 1]->color, item[0]->color,
					 sizeof(item[nitem - 1]->color));
}

static void
detach_item(Item *it)
{
	size_t i;
	for(i = 0; item[i] != it; i++);
	for(; item[i + 1]; i++)
		item[i] = item[i + 1];
	item[i] = nil;
	nitem--;
}


/* IXP stuff */


static unsigned long long
mkqpath(unsigned char type, unsigned short item)
{
    return ((unsigned long long) item << 8) | (unsigned long long) type;
}

static unsigned char
qpath_type(unsigned long long path)
{
    return path & 0xff;
}

static unsigned short
qpath_item(unsigned long long path)
{
    return (path >> 8) & 0xffff;
}

static char *
qid_to_name(Qid *qid)
{
	unsigned char type = qpath_type(qid->path);
	unsigned short i = qpath_item(qid->path);
	static char buf[32];

	switch(type) {
		case Droot: return "/"; break;
		case Ditem:
			if(!i) 
				return "default";
			else if(i == nitem)
				return "new";
			snprintf(buf, sizeof(buf), "%u", i);
			return buf;
			break;
		case Fctl: return "ctl"; break;
		case Fdisplay: return "display"; break;
		case Ffont: return "font"; break;
		case Fdata: return "data"; break;
		case Fevent: return "event"; break;
		case Fcolor: return "color"; break;
		default: return nil; break;
	}
}

static int
name_to_type(char *name)
{
	const char *err;
    unsigned int i;
	if(!name || !name[0] || !strncmp(name, "/", 2) || !strncmp(name, "..", 3))
		return Droot;
	if(!strncmp(name, "default", 8) || !strncmp(name, "new", 4))
		return Ditem;
	if(!strncmp(name, "ctl", 4))
		return Fctl;
	if(!strncmp(name, "display", 8))
		return Fdisplay;
	if(!strncmp(name, "font", 5))
		return Ffont;
	if(!strncmp(name, "data", 5))
		return Fdata;
	if(!strncmp(name, "event", 6))
		return Fevent;
	if(!strncmp(name, "color", 6))
		return Fcolor;
   	i = (unsigned short) cext_strtonum(name, 1, 0xffff, &err);
    if(!err && (i <= nitem))
		return Ditem;
	return -1;
}

static int
mkqid(Qid *dir, char *wname, Qid *new)
{
	const char *err;
	int type = name_to_type(wname);
	unsigned short i = qpath_item(dir->path);

    if((dir->type != IXP_QTDIR) || (type == -1))
        return -1;
	
    new->version = 0;
	switch(type) {
	case Droot:
		new->type = IXP_QTDIR;
		*new = root_qid;
		break;
	case Ditem:
		new->type = IXP_QTDIR;
		if(!strncmp(wname, "default", 8))
			new->path = mkqpath(Ditem, 0);
		else if(!strncmp(wname, "new", 4))
			new->path = mkqpath(Ditem, nitem);
		else {
			unsigned short i = cext_strtonum(wname, 1, 0xffff, &err);
			if(err || (i >= nitem))
				return -1;
			new->path = mkqpath(Ditem, i);
		}
		break;
	case Fdata: /* note, Fdata needs to be before Fcolor, fallthrough */
		if(!i)
			return -1;
	case Fcolor:
		if(i > nitem)
			return -1;
	default:
		new->type = IXP_QTFILE;
    	new->path = mkqpath(type, i);
		break;
	}
    return 0;
}

static int
xversion(IXPConn *c)
{
    if(strncmp(c->fcall->version, IXP_VERSION, strlen(IXP_VERSION))) {
        errstr = E9pversion;
        return -1;
    } else if(c->fcall->maxmsg > IXP_MAX_MSG)
        c->fcall->maxmsg = IXP_MAX_MSG;
    c->fcall->id = RVERSION;
    return 0;
}

static int
xattach(IXPConn *c)
{
    IXPMap *new = cext_emallocz(sizeof(IXPMap));
    new->qid = root_qid;
    new->fid = c->fcall->fid;
	c->map = (IXPMap **)cext_array_attach((void **)c->map, new,
					sizeof(IXPMap *), &c->mapsz);
    c->fcall->id = RATTACH;
    c->fcall->qid = root_qid;
    return 0;
}

static int
xwalk(IXPConn *c)
{
    unsigned short nwqid = 0;
    Qid dir = root_qid;
    IXPMap *m;

    if(!(m = ixp_server_fid2map(c, c->fcall->fid))) {
        errstr = Enofid;
        return -1;
    }
    if(c->fcall->fid != c->fcall->newfid
       && (ixp_server_fid2map(c, c->fcall->newfid))) {
        errstr = Enofid;
        return -1;
    }
    if(c->fcall->nwname) {
        dir = m->qid;
        for(nwqid = 0; (nwqid < c->fcall->nwname)
            && !mkqid(&dir, c->fcall->wname[nwqid], &c->fcall->wqid[nwqid]); nwqid++)
            dir = c->fcall->wqid[nwqid];
        if(!nwqid) {
            errstr = Enofile;
            return -1;
        }
    }
    /* a fid will only be valid, if the walk was complete */
    if(nwqid == c->fcall->nwname) {
        if(c->fcall->fid != c->fcall->newfid) {
			m = cext_emallocz(sizeof(IXPMap));
			c->map = (IXPMap **)cext_array_attach((void **)c->map, m,
							sizeof(IXPMap *), &c->mapsz);
        }
        m->qid = dir;
        m->fid = c->fcall->newfid;
    }
    c->fcall->id = RWALK;
    c->fcall->nwqid = nwqid;
    return 0;
}

static int
xopen(IXPConn *c)
{
    IXPMap *m = ixp_server_fid2map(c, c->fcall->fid);

    if(!m) {
        errstr = Enofid;
        return -1;
    }
    if(!(c->fcall->mode | IXP_OREAD) && !(c->fcall->mode | IXP_OWRITE)) {
        errstr = Enomode;
        return -1;
    }
    c->fcall->id = ROPEN;
    c->fcall->qid = m->qid;
    c->fcall->iounit = c->fcall->maxmsg
		- (sizeof(unsigned char) + sizeof(unsigned short) + 2 * sizeof(unsigned int));
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
    mkqid(dir, name, &stat->qid);
	return ixp_sizeof_stat(stat);
}

static unsigned int
type_to_stat(Stat *stat, char *name, unsigned short i)
{
	int type = name_to_type(name);
	Qid dir = {IXP_QTDIR, 0, mkqpath(Ditem, i)};

    switch (type) {
    case Droot:
    case Ditem:
		return mkstat(stat, &root_qid, name, 0, DMDIR | DMREAD | DMEXEC);
        break;
	case Fctl:
		return mkstat(stat, &root_qid, name, 0, DMWRITE);
		break;
	case Fdisplay:
		if(align == SOUTH || align == NORTH)
			return mkstat(stat, &root_qid, name, 6, DMREAD | DMWRITE);
		else
			return mkstat(stat, &root_qid, name, 5, DMREAD | DMWRITE);
		break;
    case Fevent:
		return mkstat(stat, &root_qid, name, 0, DMREAD);
		break;
    case Ffont:
		return mkstat(stat, &root_qid, name, strlen(font),
						DMREAD | DMWRITE);
        break;
    case Fdata:
		if(i == nitem)
			i = 0;
		return mkstat(stat, &dir, name, strlen(item[i]->data),
						DMREAD | DMWRITE);
		break;	
    case Fcolor:
		if(i == nitem)
			i = 0;
		return mkstat(stat, &dir, name, strlen(item[i]->color),
						DMREAD | DMWRITE);
		break;
    default:
		fprintf(stderr, "'%s'\n", name);
		errstr = "invalid stat";
		break;
    }
	return 0;
}

static int
xremove(IXPConn *c)
{
    IXPMap *m = ixp_server_fid2map(c, c->fcall->fid);
	unsigned short i;

    if(!m) {
        errstr = Enofid;
        return -1;
    }
	i = qpath_item(m->qid.path);
	if((qpath_type(m->qid.path) == Ditem) && i && (i < nitem)) {
		Item *it = item[i];
		detach_item(it);
		free(it);
    	c->fcall->id = RREMOVE;
		return 0;
	}
	errstr = Enoperm;
	return -1;
}

static int
xread(IXPConn *c)
{
	Stat stat;
    IXPMap *m = ixp_server_fid2map(c, c->fcall->fid);
    unsigned char *p = c->fcall->data;
	unsigned short i;
	unsigned int len;
	char buf[32];

    if(!m) {
        errstr = Enofid;
        return -1;
    }
	i = qpath_item(m->qid.path);
	c->fcall->count = 0; /* EOF by default */
    c->fcall->id = RREAD;
	if(c->fcall->offset)
		return 0;
	switch (qpath_type(m->qid.path)) {
	case Droot:
		c->fcall->count = type_to_stat(&stat, "ctl", 0);
		p = ixp_enc_stat(p, &stat);
		c->fcall->count += type_to_stat(&stat, "display", 0);
		p = ixp_enc_stat(p, &stat);
		c->fcall->count += type_to_stat(&stat, "font", 0);
		p = ixp_enc_stat(p, &stat);
		c->fcall->count += type_to_stat(&stat, "new", 0);
		p = ixp_enc_stat(p, &stat);
		c->fcall->count += type_to_stat(&stat, "event", 0);
		p = ixp_enc_stat(p, &stat);
		c->fcall->count += type_to_stat(&stat, "default", 0);
		p = ixp_enc_stat(p, &stat);
		for(i = 1; i < nitem; i++) {
			snprintf(buf, sizeof(buf), "%u", i);
			len = type_to_stat(&stat, buf, i);
			if(c->fcall->count + len >= c->fcall->iounit)
				break;
			c->fcall->count += len;
			p = ixp_enc_stat(p, &stat);
		}
		break;
	case Ditem:
		if(i > nitem)
			goto error_xread;
		if(!i) {
			c->fcall->count = type_to_stat(&stat, "color", i);
			p = ixp_enc_stat(p, &stat);
			break;
		}
		if(i == nitem)
			new_item();
		c->fcall->count = type_to_stat(&stat, "color", i);
		p = ixp_enc_stat(p, &stat);
		c->fcall->count += type_to_stat(&stat, "data", i);
		p = ixp_enc_stat(p, &stat);
		break;
	case Fctl:
		errstr = Enoperm;
		return -1;
		break;
	case Fdisplay:
		switch(align) {
		case SOUTH:
			memcpy(p, "south", 5);
			c->fcall->count = 5;
			break;
		case NORTH:
			memcpy(p, "north", 5);
			c->fcall->count = 5;
			break;
		default:
			memcpy(p, "none", 4);
			c->fcall->count = 4;
			break;
		}
		break;
	case Ffont:
		if((c->fcall->count = strlen(font)))
			memcpy(p, font, c->fcall->count);
		break;
	case Fevent:
		/* has to be processed asynchroneous, will be enqueued */
		return 1;
		break;
	case Fdata:
		if(i == nitem)
			new_item();
		if(i >= nitem)
			goto error_xread;
		if((c->fcall->count = strlen(item[i]->data)))
			memcpy(p, item[i]->data, c->fcall->count);
		break;
	case Fcolor:
		if(i == nitem)
			new_item();
		if(i >= nitem)
			goto error_xread;
		if((c->fcall->count = strlen(item[i]->color)))
			memcpy(p, item[i]->color, c->fcall->count);
		break;
	default:
error_xread:
		if(!errstr)
			errstr = "invalid read";
		return -1;
		break;
	}
    return 0;
}

static int
xstat(IXPConn *c)
{
    IXPMap *m = ixp_server_fid2map(c, c->fcall->fid);
	char *name;

    if(!m) {
        errstr = Enofid;
        return -1;
    }

	name = qid_to_name(&m->qid);
	if(!type_to_stat(&c->fcall->stat, name, qpath_item(m->qid.path)))
		return -1;
    c->fcall->id = RSTAT;
    return 0;
}

static int
xwrite(IXPConn *c)
{
	char buf[256];
    IXPMap *m = ixp_server_fid2map(c, c->fcall->fid);
	unsigned short i;

    if(!m) {
        errstr = Enofid;
        return -1;
    }

	i = qpath_item(m->qid.path);
	switch (qpath_type(m->qid.path)) {
	case Fctl:
		if(c->fcall->count == 5) {
			memcpy(buf, c->fcall->data, 4);
			buf[4] = 0;
			if(!strncmp(buf, "quit", 5)) {
				srv.running = 0;
				break;
			}
		}
		errstr = "command not supported";
		return -1;
		break;
	case Fdisplay:
		if(c->fcall->count > 5)
			goto error_xwrite;
		memcpy(buf, c->fcall->data, c->fcall->count);
		buf[c->fcall->count] = 0;
		if(!blitz_strtoalign(&align, buf))
			goto error_xwrite;
		/* TODO: resize/hide */
		break;
	case Ffont:
		if(font)
			free(font);
		font = cext_emallocz(c->fcall->count + 1);
		memcpy(font, c->fcall->data, c->fcall->count);
		/* TODO: XQueryFont */
		break;
	case Fdata:
		{
			unsigned int len = c->fcall->count;
			if(i == nitem)
				new_item();
			if(!i || (i >= nitem))
				goto error_xwrite;
			if(len >= sizeof(item[i]->data))
				len = sizeof(item[i]->data) - 1;
			memcpy(item[i]->data, c->fcall->data, len);
			item[i]->data[len] = 0;
			/* TODO: redraw */
		}
		break;
	case Fcolor:
		if(i == nitem)
			new_item();
		if((i >= nitem) || (c->fcall->count != 24)
			|| (c->fcall->data[0] != '#') || (c->fcall->data[8] != '#')
		    || (c->fcall->data[16] != '#'))
		{
			errstr = "wrong color format";
			goto error_xwrite;
		}
		memcpy(item[i]->color, c->fcall->data, c->fcall->count);
		item[i]->color[c->fcall->count] = 0;
		/* TODO: update color */
		break;
	default:
error_xwrite:
		if(!errstr)
			errstr = "invalid write";
		return -1;
		break;
	}
    c->fcall->id = RWRITE;
	return 0;
}

static int
xclunk(IXPConn *c)
{
    IXPMap *m = ixp_server_fid2map(c, c->fcall->fid);

    if(!m) {
        errstr = Enofid;
        return -1;
    }
	cext_array_detach((void **)c->map, m, &c->mapsz);
    free(m);
    c->fcall->id = RCLUNK;
    return 0;
}

static void
close_ixp_conn(IXPServer *s, IXPConn *c)
{
	size_t i;
	cext_array_detach((void **)s->conn, c, &s->connsz);
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
	shutdown(c->fd, SHUT_RDWR);
	close(c->fd);
	free(c);
}

static void
handle_ixp_req(IXPServer *s, IXPConn *c)
{
    unsigned int msize;
	int ret = -1;
    errstr = 0;
	if(!(msize = ixp_recv_message(c->fd, msg, IXP_MAX_MSG, &errstr))) {
		close_ixp_conn(s, c);
		return;
	}
	if(!c->fcall)
		c->fcall = cext_emallocz(sizeof(Fcall));
    if((msize = ixp_msg_to_fcall(msg, IXP_MAX_MSG, c->fcall))) {
		switch(c->fcall->id) {
		case TVERSION: ret = xversion(c); break;
		case TATTACH: ret = xattach(c); break;
		case TWALK: ret = xwalk(c); break;
		case TREMOVE: ret = xremove(c); break;
		case TOPEN: ret = xopen(c); break;
		case TREAD: ret = xread(c); break;
		case TWRITE: ret = xwrite(c); break;
		case TCLUNK: ret = xclunk(c); break;
		case TSTAT: ret = xstat(c); break;
		default:
			break;
		}
	}
	if(ret == -1) {
		if(!errstr)
			errstr = Enofunc;
		c->fcall->id = RERROR;
		cext_strlcpy(c->fcall->errstr, errstr, sizeof(c->fcall->errstr));
	}
	else if(ret == 1) {
		c->pend = (Fcall **)cext_array_attach((void **)c->pend, c->fcall, sizeof(Fcall *), &c->pendsz);
		c->fcall = nil;
		return;	 /* response asynchroneously */
	}
	msize = ixp_fcall_to_msg(c->fcall, msg, IXP_MAX_MSG);
	if(ixp_send_message(c->fd, msg, msize, &errstr) != msize)
		close_ixp_conn(s, c);
}

static void
new_ixp_conn(IXPServer *s, IXPConn *c)
{
    IXPConn *new;
	int fd = ixp_accept_sock(c->fd);
	
	if(fd >= 0) {
		new = cext_emallocz(sizeof(IXPConn));
		new->fd = fd;
		new->read = handle_ixp_req;
		new->close = close_ixp_conn;
		s->conn = (IXPConn **)cext_array_attach((void **)s->conn, new,
					sizeof(IXPConn *), &s->connsz);
	}
}



/* main */

int
main(int argc, char *argv[])
{
    int i;
	IXPConn *c;

    /* command line args */
    for(i = 1; (i < argc) && (argv[i][0] == '-'); i++) {
        switch (argv[i][1]) {
        case 'v':
            fprintf(stdout, "%s", version[0]);
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

    dpy = XOpenDisplay(0);
    if(!dpy) {
        fprintf(stderr, "%s", "wmiibar: cannot open display\n");
        exit(1);
    }
    XSetErrorHandler(dummy_error_handler);
    screen_num = DefaultScreen(dpy);

    if(!address) {
		fprintf(stderr, "%s\n", "wmiibar: no socket address provided");
		exit(1);
	}
	i = ixp_create_sock(address, &errstr);
	if(i < 0) {
        fprintf(stderr, "wmiibar: fatal: %s\n", errstr);
		exit(1);
	}
	c = cext_emallocz(sizeof(IXPConn));
	c->fd = i;
	c->read = new_ixp_conn;
	c->close = close_ixp_conn;
	srv.conn = (IXPConn **)cext_array_attach((void **)srv.conn, c,
					sizeof(IXPConn *), &srv.connsz);
	/* TODO: add X conn */
	
    root_qid.type = IXP_QTDIR;
    root_qid.version = 0;
    root_qid.path = mkqpath(Droot, 0);

    mypid = getpid();

    /* default item settings */
	new_item();
	cext_strlcpy(item[0]->color, BLITZ_SEL_COLOR, sizeof(item[0]->color));

    font = strdup(BLITZ_FONT);

	errstr = ixp_server_loop(&srv);

	if(errstr)
    	fprintf(stderr, "wmiibar: fatal: %s\n", errstr);

	/* cleanup */
	for(i = 0; (i < srv.connsz) && srv.conn[i]; i++)
		if(srv.conn[i]->close)
			srv.conn[i]->close(&srv, srv.conn[i]);

	return errstr ? 1 : 0;
}
