#include "dat.h"
#include <fmt.h>
#include "fns.h"

static char* fcnames[] = {
	"TVersion",
	"RVersion",
	"TAuth",
	"RAuth",
	"TAttach",
	"RAttach",
	"TError",
	"RError",
	"TFlush",
	"RFlush",
	"TWalk",
	"RWalk",
	"TOpen",
	"ROpen",
	"TCreate",
	"RCreate",
	"TRead",
	"RRead",
	"TWrite",
	"RWrite",
	"TClunk",
	"RClunk",
	"TRemove",
	"RRemove",
	"TStat",
	"RStat",
	"TWStat",
	"RWStat",
};

static int
qid(Fmt *f, Qid *q) {
	return fmtprint(f, "(%uhd,%uld,%ullx)", q->type, q->version, q->path);
}

int
Ffmt(Fmt *f) {
	Fcall *fcall;

	fcall = va_arg(f->args, Fcall*);
	fmtprint(f, "% 2d %s\t", fcall->hdr.tag, fcnames[fcall->hdr.type - TVersion]);
	switch(fcall->hdr.type) {
	case TVersion:
	case RVersion:
		fmtprint(f, " msize: %uld version: \"%s\"", (ulong)fcall->version.msize, fcall->version.version);
		break;
	case TAuth:
		fmtprint(f, " afid: %uld uname: \"%s\" aname: \"%s\"", (ulong)fcall->tauth.afid, fcall->tauth.uname, fcall->tauth.aname);
		break;
	case RAuth:
		fmtprint(f, " aqid: ");
		qid(f, &fcall->rauth.aqid);
		break;
	case RAttach:
		fmtprint(f, " qid: ");
		qid(f, &fcall->rattach.qid);
		break;
	case TAttach:
		fmtprint(f, " fid: %uld afid: %uld uname: \"%s\" aname: \"%s\"", (ulong)fcall->hdr.fid, (ulong)fcall->tattach.afid, fcall->tattach.uname, fcall->tattach.aname);
		break;
	case RError:
		fmtprint(f, " \"%s\"", fcall->error.ename);
		break;
	case TFlush:
		fmtprint(f, " oldtag: %uld", (ulong)fcall->tflush.oldtag);
		break;
	case TWalk:
		fmtprint(f, " newfid: %uld wname: {", (ulong)fcall->twalk.newfid);
		for(int i=0; i<fcall->twalk.nwname; i++) {
			if(i > 0) fmtprint(f, ", ");
			fmtprint(f, "\"%s\"", fcall->twalk.wname[i]);
		}
		fmtprint(f, "}");
		break;
	case RWalk:
		fmtprint(f, " wqid: {");
		for(int i=0; i<fcall->rwalk.nwqid; i++) {
			if(i > 0) fmtprint(f, ", ");
			qid(f, &fcall->rwalk.wqid[i]);
		}
		fmtprint(f, "}");
		break;
	case TOpen:
		fmtprint(f, " fid: %uld mode: %ulo", (ulong)fcall->hdr.fid, (ulong)fcall->topen.mode);
		break;
	case ROpen:
	case RCreate:
		fmtprint(f, " qid: ");
		qid(f, &fcall->ropen.qid);
		fmtprint(f, " %uld", (ulong)fcall->ropen.iounit);
		break;
	case TCreate:
		fmtprint(f, " fid: %uld name:  \"%s\" perm: %ulo mode: %ulo", (ulong)fcall->hdr.fid, fcall->tcreate.name, (ulong)fcall->tcreate.perm, (ulong)fcall->tcreate.mode);
		break;
	case TRead:
		fmtprint(f, " fid: %uld offset: %ulld count: %uld", (ulong)fcall->hdr.fid, fcall->tread.offset, (ulong)fcall->tread.count);
		break;
	case RRead:
		fmtprint(f, " data: {data: %uld}", fcall->rread.count);
		break;
	case TWrite:
		fmtprint(f, " fid: %uld offset: %ulld data: {data: %uld}", (ulong)fcall->hdr.fid, fcall->twrite.offset, fcall->twrite.count);
		break;
	case RWrite:
		fmtprint(f, " count: %uld", (ulong)fcall->rwrite.count);
		break;
	case TClunk:
	case TRemove:
	case TStat:
		fmtprint(f, " fid: %uld", (ulong)fcall->hdr.fid);
		break;
	case RStat:
		fmtprint(f, " stat: {data: %uld}", fcall->rstat.nstat);
		break;
	}

	return 0;
}

