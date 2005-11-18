/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include <string.h>
#include "ixp.h"

static u16 
sizeof_string(const char *s)
{
	return sizeof(u16) + strlen(s);
}

u16 
ixp_sizeof_stat(Stat * stat)
{
	return sizeof(Qid)
	+ 2 * sizeof(u16)
	+ 4 * sizeof(u32)
	+ sizeof(u64)
	+ sizeof_string(stat->name)
	+ sizeof_string(stat->uid)
	+ sizeof_string(stat->gid)
	+ sizeof_string(stat->muid);
}

u32 
ixp_fcall_to_msg(Fcall * fcall, void *msg, u32 msglen)
{
	u32             i, msize = sizeof(u8) + sizeof(u16) + sizeof(u32);
	void           *p = msg;

	switch (fcall->id) {
	case TVERSION:
	case RVERSION:
		msize += sizeof(u32) + sizeof_string(fcall->version);
		break;
	case TAUTH:
		msize +=
			sizeof(u32) + sizeof_string(fcall->uname) +
			sizeof_string(fcall->aname);
		break;
	case RAUTH:
	case RATTACH:
		msize += sizeof(Qid);
		break;
	case TATTACH:
		msize +=
			2 * sizeof(u32) + sizeof_string(fcall->uname) +
			sizeof_string(fcall->aname);
		break;
	case RERROR:
		msize += sizeof_string(fcall->errstr) + sizeof(u32);
		break;
	case RWRITE:
	case TCLUNK:
	case TREMOVE:
	case TSTAT:
		msize += sizeof(u32);
		break;
	case TWALK:
		msize += sizeof(u16) + 2 * sizeof(u32);
		for (i = 0; i < fcall->nwname; i++)
			msize += sizeof_string(fcall->wname[i]);
		break;
	case TFLUSH:
		msize += sizeof(u16);
		break;
	case RWALK:
		msize += sizeof(u16) + fcall->nwqid * sizeof(Qid);
		break;
	case TOPEN:
		msize += sizeof(u32) + sizeof(u8);
		break;
	case ROPEN:
	case RCREATE:
		msize += sizeof(Qid) + sizeof(u32);
		break;
	case TCREATE:
		msize += sizeof(u8) + 2 * sizeof(u32) + sizeof_string(fcall->name);
		break;
	case TREAD:
		msize += 2 * sizeof(u32) + sizeof(u64);
		break;
	case RREAD:
		msize += sizeof(u32) + fcall->count;
		break;
	case TWRITE:
		msize += 2 * sizeof(u32) + sizeof(u64) + fcall->count;
		break;
	case RSTAT:
		msize += ixp_sizeof_stat(&fcall->stat);
		break;
	case TWSTAT:
		msize += sizeof(u32) + ixp_sizeof_stat(&fcall->stat);
		break;
	default:
		break;
	}

	if (msize > msglen)
		return 0;
	p = ixp_enc_prefix(p, msize, fcall->id, fcall->tag);

	switch (fcall->id) {
	case TVERSION:
	case RVERSION:
		p = ixp_enc_u32(p, fcall->maxmsg);
		p = ixp_enc_string(p, fcall->version);
		break;
	case TAUTH:
		p = ixp_enc_u32(p, fcall->afid);
		p = ixp_enc_string(p, fcall->uname);
		p = ixp_enc_string(p, fcall->aname);
		break;
	case RAUTH:
	case RATTACH:
		p = ixp_enc_qid(p, &fcall->qid);
		break;
	case TATTACH:
		p = ixp_enc_u32(p, fcall->fid);
		p = ixp_enc_u32(p, fcall->afid);
		p = ixp_enc_string(p, fcall->uname);
		p = ixp_enc_string(p, fcall->aname);
		break;
	case RERROR:
		p = ixp_enc_string(p, fcall->errstr);
		break;
	case TFLUSH:
		p = ixp_enc_u16(p, fcall->oldtag);
		break;
	case TWALK:
		p = ixp_enc_u32(p, fcall->fid);
		p = ixp_enc_u32(p, fcall->newfid);
		p = ixp_enc_u16(p, fcall->nwname);
		for (i = 0; i < fcall->nwname; i++)
			p = ixp_enc_string(p, fcall->wname[i]);
		break;
	case RWALK:
		p = ixp_enc_u16(p, fcall->nwqid);
		for (i = 0; i < fcall->nwqid; i++)
			p = ixp_enc_qid(p, &fcall->wqid[i]);
		break;
	case TOPEN:
		p = ixp_enc_u32(p, fcall->fid);
		p = ixp_enc_u8(p, fcall->mode);
		break;
	case ROPEN:
	case RCREATE:
		p = ixp_enc_qid(p, &fcall->qid);
		p = ixp_enc_u32(p, fcall->iounit);
		break;
	case TCREATE:
		p = ixp_enc_u32(p, fcall->fid);
		p = ixp_enc_string(p, fcall->name);
		p = ixp_enc_u32(p, fcall->perm);
		p = ixp_enc_u8(p, fcall->mode);
		break;
	case TREAD:
		p = ixp_enc_u32(p, fcall->fid);
		p = ixp_enc_u64(p, fcall->offset);
		p = ixp_enc_u32(p, fcall->count);
		break;
	case RREAD:
		p = ixp_enc_u32(p, fcall->count);
		p = ixp_enc_data(p, fcall->data, fcall->count);
		break;
	case TWRITE:
		p = ixp_enc_u32(p, fcall->fid);
		p = ixp_enc_u64(p, fcall->offset);
		p = ixp_enc_u32(p, fcall->count);
		p = ixp_enc_data(p, fcall->data, fcall->count);
		break;
	case RWRITE:
		p = ixp_enc_u32(p, fcall->count);
		break;
	case TCLUNK:
	case TREMOVE:
	case TSTAT:
		p = ixp_enc_u32(p, fcall->fid);
		break;
	case RSTAT:
		p = ixp_enc_stat(p, &fcall->stat);
		break;
	case TWSTAT:
		p = ixp_enc_u32(p, fcall->fid);
		p = ixp_enc_stat(p, &fcall->stat);
		break;
	}
	return msize;
}

u32 
ixp_msg_to_fcall(void *msg, u32 msglen, Fcall * fcall)
{
	u32             i, msize;
	u16             len;
	void           *p = ixp_dec_prefix(msg, &msize, &fcall->id, &fcall->tag);

	if (msize > msglen)	/* bad message */
		return 0;

	switch (fcall->id) {
	case TVERSION:
	case RVERSION:
		p = ixp_dec_u32(p, &fcall->maxmsg);
		p = ixp_dec_string(p, fcall->version, sizeof(fcall->version),
				   &len);
		break;
	case TAUTH:
		p = ixp_dec_u32(p, &fcall->afid);
		p = ixp_dec_string(p, fcall->uname, sizeof(fcall->uname), &len);
		p = ixp_dec_string(p, fcall->aname, sizeof(fcall->aname), &len);
		break;
	case RAUTH:
	case RATTACH:
		p = ixp_dec_qid(p, &fcall->qid);
		break;
	case TATTACH:
		p = ixp_dec_u32(p, &fcall->fid);
		p = ixp_dec_u32(p, &fcall->afid);
		p = ixp_dec_string(p, fcall->uname, sizeof(fcall->uname), &len);
		p = ixp_dec_string(p, fcall->aname, sizeof(fcall->aname), &len);
		break;
	case RERROR:
		p = ixp_dec_string(p, fcall->errstr, sizeof(fcall->errstr), &len);
		break;
	case TFLUSH:
		p = ixp_dec_u16(p, &fcall->oldtag);
		break;
	case TWALK:
		p = ixp_dec_u32(p, &fcall->fid);
		p = ixp_dec_u32(p, &fcall->newfid);
		p = ixp_dec_u16(p, &fcall->nwname);
		for (i = 0; i < fcall->nwname; i++)
			p = ixp_dec_string(p, fcall->wname[i], IXP_MAX_FLEN, &len);
		break;
	case RWALK:
		p = ixp_dec_u16(p, &fcall->nwqid);
		for (i = 0; i < fcall->nwqid; i++)
			p = ixp_dec_qid(p, &fcall->wqid[i]);
		break;
	case TOPEN:
		p = ixp_dec_u32(p, &fcall->fid);
		p = ixp_dec_u8(p, &fcall->mode);
		break;
	case ROPEN:
	case RCREATE:
		p = ixp_dec_qid(p, &fcall->qid);
		p = ixp_dec_u32(p, &fcall->iounit);
		break;
	case TCREATE:
		p = ixp_dec_u32(p, &fcall->fid);
		p = ixp_dec_string(p, fcall->name, sizeof(fcall->name), &len);
		p = ixp_dec_u32(p, &fcall->perm);
		p = ixp_dec_u8(p, &fcall->mode);
		break;
	case TREAD:
		p = ixp_dec_u32(p, &fcall->fid);
		p = ixp_dec_u64(p, &fcall->offset);
		p = ixp_dec_u32(p, &fcall->count);
		break;
	case RREAD:
		p = ixp_dec_u32(p, &fcall->count);
		p = ixp_dec_data(p, fcall->data, fcall->count);
		break;
	case TWRITE:
		p = ixp_dec_u32(p, &fcall->fid);
		p = ixp_dec_u64(p, &fcall->offset);
		p = ixp_dec_u32(p, &fcall->count);
		p = ixp_dec_data(p, fcall->data, fcall->count);
		break;
	case RWRITE:
		p = ixp_dec_u32(p, &fcall->count);
		break;
	case TCLUNK:
	case TREMOVE:
	case TSTAT:
		p = ixp_dec_u32(p, &fcall->fid);
		break;
	case RSTAT:
		p = ixp_dec_stat(p, &fcall->stat);
		break;
	case TWSTAT:
		p = ixp_dec_u32(p, &fcall->fid);
		p = ixp_dec_stat(p, &fcall->stat);
		break;
	}

	return msize;
}
