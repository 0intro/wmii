enum {	TAG_BUCKETS = 64;
	FID_BUCKETS = 64; }

typedef struct Fid {
	unsigned long	fid;
	unsigned char	omode;
	char		*uid;
	Qid		qid;
	void		*aux;

	/* Implementation specific */
	/* Do not use */

	unsigned long	ofid;
} Fid;

struct IXPConn {
	IXPConn		*next;
	IXPServer	*srv;
	void		*aux;
	int		fd;
	void		(*read) (IXPConn *);
	void		(*close) (IXPConn *);
};

struct P9Conn {
}

Req *
ixp_server_receive_req(IXPConn *c)
{
	Fcall fcall;
	P9Client *pc = c->aux;
	Req *req;
	unsigned int msize;
	char *errstr = nil;

	if(!(msize = ixp_recv_message(c->fd, msg, IXP_MAX_MSG, &errstr)))
		goto Fail;
	if(!(msize = ixp_msg2fcall(&fcall, msg, IXP_MAX_MSG)))
		goto Fail;

	req = cext_emallocz(sizeof(Req));
	req->conn = c;
	req->ifcall = fcall;

	if(lookupkey(pc->tagmap, fcall->tag)) {
		respond(req, Edupdag);
		return nil;
	}

	insertkey(pc->tagmap, fcall.id, req);
	return req;

Fail:
	ixp_close_conn(c);
	return nil;
}

void
ixp_handle_fcall(IXPConn *c)
{
	Req *r;
	unsigned int msize;
	P9Conn *pc = c->aux;

	if(!(r = ixp_server_receive_fcall(c, &fcall)))
		return;

	switch(r->ifcall.id) {
	default:
		respond(r, Enofunc);
		break;
	case TVERSION:
		if(!strncmp(r->ifcall.version, "9P", 3)) {
			r->ofcall.version = "9P";
		}else
		if(!strncmp(r->ifcall.version, "9P2000", 7)) {
			r->ofcall.version = "9P2000";
		}else{
			r->ofcall.version = "unknown";
		}
		r->ofcall.msize = r->ifcall.msize;
		respond(r, nil);
		break;
	case TATTACH:
		if(!(r->fid = createfid(pc->fid_hash, r->ifcall.fid)))
			return respond(r, Edupfid);
		/* attach is a required function */
		srv->attach(r);
		break;
	case TCLUNK:
		if(!destroyfid(pc, r->ifcall.fid))
			return respond(r, Enofid);
		respond(r, nil);
		break;
	case TCREATE:
		if(!(r->fid = lookupkey(pc->fid_hash, r->ifcall.fid)))
			return respond(r, Enofid);
		if(r->fid->omode != -1)
			return respond(r, Ebotch);
		if(!(r->fid->qid.type&QTDIR))
			return respond(r, Enotdir);
		if(!pc->srv->create)
			return respond(r, Enofunc);
		pc->srv->create(r);
		break;
	case TOPEN:
		if(!(r->fid = lookupkey(pc->fid_hash, r->ifcall.fid)))
			return respond(r, Enofid);
		if((r->fid.qid.type&QTDIR) && (r->fcall.mode|ORCLOSE) != (OREAD|ORCLOSE))
			return respond(r, Eisdir);
		r->ofcall.qid = r->fid.qid;
		if(!pc->srv->open)
			return respond(r, Enofunc);
		pc->srv->open(r);
		break;
	case TREAD:
		if(!(r->fid = lookupkey(pc->fid_hash, r->ifcall.fid)))
			return respond(r, Enofid);
		if(r->fid.omode == -1)
			return respond(r, Ebotch);
		if(!pc->srv->read)
			return respond(r, Enofunc);
		pc->srv->read(r);
		break;
	case TREMOVE:
		if(!(r->fid = lookupkey(pc->fid_hash, r->ifcall.fid)))
			return respond(r, Enofid);
		if(!pc->srv->remove)
			return respond(r, Enofunc);
		pc->src->remove(r);
		break;
	case TSTAT:
		if(!(r->fid = lookupkey(pc->fid_hash, r->ifcall.fid)))
			return respond(r, Enofid);
		if(!pc->srv->stat)
			return respond(r, Enofunc);
		pc->srv->stat(r);
		break;
	case TWALK:
		if(!(r->fid = lookupkey(pc->fid_hash, r->ifcall.fid)))
			return respond(r, Enofid);
		if(r->fid->omode != -1)
			return respond(r, "cannot walk from an open fid");
		if(r->ifcall.nwname && !(r->fid->qid.type&QTDIR))
			return respond(r, Enotdir);
		if((r->ifcall.fid != r->ifcall.newfid) &&
		  !(r->newfid = createfid(pc->fid_hash, r->ifcall.newfid)))
			return respond(r, Edupfid);
		if(!pc->srv->walk)
			return respond(r, Enofunc);
		pc->srv->walk(r);
		break;
	case TWRITE:
		if(!(r->fid = lookupkey(pc->fid_hash, r->ifcall.fid)))
			return respond(r, Enofid);
		if((r->fid->omode&3) != OWRITE && (r->fid->omode&3) != ORDWR)
			return respond(r, "write on fid not opened for writing");
		if(!pc->srv->write)
			return respond(r, Enofunc);
		pc->srv->write(r);
		break;
	/* Still to be implemented: flush, wstat, auth */
	}
}

void
respond(Req *r, char *error) {
	P9Conn *pc = r->conn->aux;
	switch(r->ifcall.type) {
	default:
		cext_assert(!"Respond called on unsupported fcall type");
		break;
	case TVERSION:
		cext_assert(!error);
		pc->msize = r->ofcall.msize;
		free(pc->buf);
		pc->buf = cext_emallocz(r->ofcall.msize);
		break;
	case TATTACH:
		if(error)
			destroyfid(r->fid);
		break;
	case TOPEN:
	case TCREATE:
		if(!error) {
			r->fid->omode = r->ofcall.mode;
			r->fid->qid = r->ofcall.qid;
		}
		break;
	case TWALK:
		if(error || r->ofcall.nwqid < r->ifcall.nwname) {
			if(r->ifcall.fid != r->ifcall.newfid)
				destroyfid(r->newfid);
			if(!error && r->ofcall.nwqid == 0)
				error = Enofile;
		}else{
			if(f->ofcall.nwqid == 0)
				r->newfid->qid = r->fid->qid;
			else
				r->newfid->qid = r->ofcall.wqid[r->ofcall.nwqid-1];
		}
		break;
	case TCLUNK:
	case TREAD:
	case TREMOVE:
	case TSTAT:
	case TWRITE:
		break;
	/* Still to be implemented: flush, wstat, auth */
	}

	r->ofcall.tag = r->ifcall.tag;
	if(!error)
		r->ofcall.type = r->ifcall.type + 1;
	else {
		r->ofcall.type = RERROR;
		r->ofcall.ename = error;
	}

	ixp_server_respond_fcall(r->conn, r->ofcall);

	deletekey(pc->tag_pool, r->tag);;
	free(r);
}

void
ixp_cleanup_conn(IXPConn *c) {
}

void
serve_9pcon(IXPConn *c) {
	int fd = accept(c->fd, nil, nil);
	if(fd < 0)
		return;

	P9Conn *pc = cext_emallocz(sizeof(P9Conn));
	pc->srv = c->aux;

	initmap(&pc->tagmap, TAG_BUCKETS, &pc->tag_hash);
	initmap(&pc->fidmap, FID_BUCKETS, &pc->fid_hash);

	ixp_server_open_conn(c->srv, fd, pc, ixp_handle_fcall, ixp_cleanup_conn);
}
