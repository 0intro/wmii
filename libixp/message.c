/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ixp.h"

#define IXP_QIDSZ (sizeof(unsigned char) + sizeof(unsigned int)\
		+ sizeof(unsigned long long))

static unsigned short
sizeof_string(const char *s)
{
	return sizeof(unsigned short) + strlen(s);
}

unsigned short
ixp_sizeof_stat(Stat * stat)
{
	return IXP_QIDSZ
		+ 2 * sizeof(unsigned short)
		+ 4 * sizeof(unsigned int)
		+ sizeof(unsigned long long)
		+ sizeof_string(stat->name)
		+ sizeof_string(stat->uid)
		+ sizeof_string(stat->gid)
		+ sizeof_string(stat->muid);
}

unsigned int
ixp_fcall2msg(void *msg, Fcall *fcall, unsigned int msglen)
{
	unsigned int i = sizeof(unsigned char) +
		sizeof(unsigned short) + sizeof(unsigned int);
	int msize = msglen - i;
	unsigned char *p = msg + i;

	switch (fcall->id) {
	case TVERSION:
	case RVERSION:
		ixp_pack_u32(&p, &msize, fcall->maxmsg);
		ixp_pack_string(&p, &msize, fcall->version);
		break;
	case TAUTH:
		ixp_pack_u32(&p, &msize, fcall->afid);
		ixp_pack_string(&p, &msize, fcall->uname);
		ixp_pack_string(&p, &msize, fcall->aname);
		break;
	case RAUTH:
		ixp_pack_qid(&p, &msize, &fcall->aqid);
		break;
	case RATTACH:
		ixp_pack_qid(&p, &msize, &fcall->qid);
		break;
	case TATTACH:
		ixp_pack_u32(&p, &msize, fcall->fid);
		ixp_pack_u32(&p, &msize, fcall->afid);
		ixp_pack_string(&p, &msize, fcall->uname);
		ixp_pack_string(&p, &msize, fcall->aname);
		break;
	case RERROR:
		ixp_pack_string(&p, &msize, fcall->errstr);
		break;
	case TFLUSH:
		ixp_pack_u16(&p, &msize, fcall->oldtag);
		break;
	case TWALK:
		ixp_pack_u32(&p, &msize, fcall->fid);
		ixp_pack_u32(&p, &msize, fcall->newfid);
		ixp_pack_u16(&p, &msize, fcall->nwname);
		for(i = 0; i < fcall->nwname; i++)
			ixp_pack_string(&p, &msize, fcall->wname[i]);
		break;
	case RWALK:
		ixp_pack_u16(&p, &msize, fcall->nwqid);
		for(i = 0; i < fcall->nwqid; i++)
			ixp_pack_qid(&p, &msize, &fcall->wqid[i]);
		break;
	case TOPEN:
		ixp_pack_u32(&p, &msize, fcall->fid);
		ixp_pack_u8(&p, &msize, fcall->mode);
		break;
	case ROPEN:
	case RCREATE:
		ixp_pack_qid(&p, &msize, &fcall->qid);
		ixp_pack_u32(&p, &msize, fcall->iounit);
		break;
	case TCREATE:
		ixp_pack_u32(&p, &msize, fcall->fid);
		ixp_pack_string(&p, &msize, fcall->name);
		ixp_pack_u32(&p, &msize, fcall->perm);
		ixp_pack_u8(&p, &msize, fcall->mode);
		break;
	case TREAD:
		ixp_pack_u32(&p, &msize, fcall->fid);
		ixp_pack_u64(&p, &msize, fcall->offset);
		ixp_pack_u32(&p, &msize, fcall->count);
		break;
	case RREAD:
		ixp_pack_u32(&p, &msize, fcall->count);
		ixp_pack_data(&p, &msize, fcall->data, fcall->count);
		break;
	case TWRITE:
		ixp_pack_u32(&p, &msize, fcall->fid);
		ixp_pack_u64(&p, &msize, fcall->offset);
		ixp_pack_u32(&p, &msize, fcall->count);
		ixp_pack_data(&p, &msize, fcall->data, fcall->count);
		break;
	case RWRITE:
		ixp_pack_u32(&p, &msize, fcall->count);
		break;
	case TCLUNK:
	case TREMOVE:
	case TSTAT:
		ixp_pack_u32(&p, &msize, fcall->fid);
		break;
	case RSTAT:
		ixp_pack_u16(&p, &msize, ixp_sizeof_stat(&fcall->stat));
		ixp_pack_stat(&p, &msize, &fcall->stat);
		break;
	case TWSTAT:
		ixp_pack_u32(&p, &msize, fcall->fid);
		ixp_pack_u16(&p, &msize, ixp_sizeof_stat(&fcall->stat));
		ixp_pack_stat(&p, &msize, &fcall->stat);
		break;
	}

	if(msize < 0)
		return 0;

	msize = msglen - msize;
	ixp_pack_prefix(msg, msize, fcall->id, fcall->tag);
	return msize;
}

unsigned int
ixp_msg2fcall(Fcall *fcall, void *msg, unsigned int msglen)
{
	unsigned int i, msize;
	unsigned short len;
	void *p = ixp_unpack_prefix(msg, &msize, &fcall->id, &fcall->tag);

	if(msize > msglen)          /* bad message */
		return 0;
	switch (fcall->id) {
	case TVERSION:
	case RVERSION:
		p = ixp_unpack_u32(p, &fcall->maxmsg);
		p = ixp_unpack_string(p, fcall->version, sizeof(fcall->version), &len);
		break;
	case TAUTH:
		p = ixp_unpack_u32(p, &fcall->afid);
		p = ixp_unpack_string(p, fcall->uname, sizeof(fcall->uname), &len);
		p = ixp_unpack_string(p, fcall->aname, sizeof(fcall->aname), &len);
		break;
	case RAUTH:
		p = ixp_unpack_qid(p, &fcall->aqid);
		break;
	case RATTACH:
		p = ixp_unpack_qid(p, &fcall->qid);
		break;
	case TATTACH:
		p = ixp_unpack_u32(p, &fcall->fid);
		p = ixp_unpack_u32(p, &fcall->afid);
		p = ixp_unpack_string(p, fcall->uname, sizeof(fcall->uname), &len);
		p = ixp_unpack_string(p, fcall->aname, sizeof(fcall->aname), &len);
		break;
	case RERROR:
		p = ixp_unpack_string(p, fcall->errstr, sizeof(fcall->errstr), &len);
		break;
	case TFLUSH:
		p = ixp_unpack_u16(p, &fcall->oldtag);
		break;
	case TWALK:
		p = ixp_unpack_u32(p, &fcall->fid);
		p = ixp_unpack_u32(p, &fcall->newfid);
		p = ixp_unpack_u16(p, &fcall->nwname);
		for(i = 0; i < fcall->nwname; i++)
			p = ixp_unpack_string(p, fcall->wname[i], IXP_MAX_FLEN, &len);
		break;
	case RWALK:
		p = ixp_unpack_u16(p, &fcall->nwqid);
		for(i = 0; i < fcall->nwqid; i++)
			p = ixp_unpack_qid(p, &fcall->wqid[i]);
		break;
	case TOPEN:
		p = ixp_unpack_u32(p, &fcall->fid);
		p = ixp_unpack_u8(p, &fcall->mode);
		break;
	case ROPEN:
	case RCREATE:
		p = ixp_unpack_qid(p, &fcall->qid);
		p = ixp_unpack_u32(p, &fcall->iounit);
		break;
	case TCREATE:
		p = ixp_unpack_u32(p, &fcall->fid);
		p = ixp_unpack_string(p, fcall->name, sizeof(fcall->name), &len);
		p = ixp_unpack_u32(p, &fcall->perm);
		p = ixp_unpack_u8(p, &fcall->mode);
		break;
	case TREAD:
		p = ixp_unpack_u32(p, &fcall->fid);
		p = ixp_unpack_u64(p, &fcall->offset);
		p = ixp_unpack_u32(p, &fcall->count);
		break;
	case RREAD:
		p = ixp_unpack_u32(p, &fcall->count);
		p = ixp_unpack_data(p, fcall->data, fcall->count);
		break;
	case TWRITE:
		p = ixp_unpack_u32(p, &fcall->fid);
		p = ixp_unpack_u64(p, &fcall->offset);
		p = ixp_unpack_u32(p, &fcall->count);
		p = ixp_unpack_data(p, fcall->data, fcall->count);
		break;
	case RWRITE:
		p = ixp_unpack_u32(p, &fcall->count);
		break;
	case TCLUNK:
	case TREMOVE:
	case TSTAT:
		p = ixp_unpack_u32(p, &fcall->fid);
		break;
	case RSTAT:
		p = ixp_unpack_u16(p, &len);
		p = ixp_unpack_stat(p, &fcall->stat);
		break;
	case TWSTAT:
		p = ixp_unpack_u32(p, &fcall->fid);
		p = ixp_unpack_u16(p, &len);
		p = ixp_unpack_stat(p, &fcall->stat);
		break;
	}

	if(msg + msize == p)
		return msize;
	return 0;
}
