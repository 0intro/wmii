/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include <string.h>
#include "ixp.h"

static unsigned short
sizeof_string(const char *s)
{
    return sizeof(unsigned short) + strlen(s);
}

unsigned short
ixp_sizeof_stat(Stat * stat)
{
    return sizeof(Qid)
        + 2 * sizeof(unsigned short)
        + 4 * sizeof(unsigned int)
        + sizeof(unsigned long long)
        + sizeof_string(stat->name)
        + sizeof_string(stat->uid)
        + sizeof_string(stat->gid)
        + sizeof_string(stat->muid);
}

unsigned int
ixp_fcall_to_msg(Fcall * fcall, void *msg, unsigned int msglen)
{
    unsigned int i, msize =
        sizeof(unsigned char) + sizeof(unsigned short) +
        sizeof(unsigned int);
    void *p = msg;

    switch (fcall->id) {
    case TVERSION:
    case RVERSION:
        msize += sizeof(unsigned int) + sizeof_string(fcall->version);
        break;
    case TAUTH:
        msize +=
            sizeof(unsigned int) + sizeof_string(fcall->uname) +
            sizeof_string(fcall->aname);
        break;
    case RAUTH:
    case RATTACH:
        msize += sizeof(Qid);
        break;
    case TATTACH:
        msize +=
            2 * sizeof(unsigned int) + sizeof_string(fcall->uname) +
            sizeof_string(fcall->aname);
        break;
    case RERROR:
        msize += sizeof_string(fcall->errstr);
        break;
    case RWRITE:
    case TCLUNK:
    case TREMOVE:
    case TSTAT:
        msize += sizeof(unsigned int);
        break;
    case TWALK:
        msize += sizeof(unsigned short) + 2 * sizeof(unsigned int);
        for(i = 0; i < fcall->nwname; i++)
            msize += sizeof_string(fcall->wname[i]);
        break;
    case TFLUSH:
        msize += sizeof(unsigned short);
        break;
    case RWALK:
        msize += sizeof(unsigned short) + fcall->nwqid * sizeof(Qid);
        break;
    case TOPEN:
        msize += sizeof(unsigned int) + sizeof(unsigned char);
        break;
    case ROPEN:
    case RCREATE:
        msize += sizeof(Qid) + sizeof(unsigned int);
        break;
    case TCREATE:
        msize +=
            sizeof(unsigned char) + 2 * sizeof(unsigned int) +
            sizeof_string(fcall->name);
        break;
    case TREAD:
        msize += 2 * sizeof(unsigned int) + sizeof(unsigned long long);
        break;
    case RREAD:
        msize += sizeof(unsigned int) + fcall->count;
        break;
    case TWRITE:
        msize +=
            2 * sizeof(unsigned int) + sizeof(unsigned long long) +
            fcall->count;
        break;
    case RSTAT:
        msize += ixp_sizeof_stat(&fcall->stat);
        break;
    case TWSTAT:
        msize += sizeof(unsigned int) + ixp_sizeof_stat(&fcall->stat);
        break;
    default:
        break;
    }

    if(msize > msglen)
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
        p = ixp_enc_qid(p, &fcall->aqid);
        break;
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
        for(i = 0; i < fcall->nwname; i++)
            p = ixp_enc_string(p, fcall->wname[i]);
        break;
    case RWALK:
        p = ixp_enc_u16(p, fcall->nwqid);
        for(i = 0; i < fcall->nwqid; i++)
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

unsigned int
ixp_msg_to_fcall(void *msg, unsigned int msglen, Fcall * fcall)
{
    unsigned int i, msize;
    unsigned short len;
    void *p = ixp_dec_prefix(msg, &msize, &fcall->id, &fcall->tag);

    if(msize > msglen)          /* bad message */
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
        p = ixp_dec_qid(p, &fcall->aqid);
        break;
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
        for(i = 0; i < fcall->nwname; i++)
            p = ixp_dec_string(p, fcall->wname[i], IXP_MAX_FLEN, &len);
        break;
    case RWALK:
        p = ixp_dec_u16(p, &fcall->nwqid);
        for(i = 0; i < fcall->nwqid; i++)
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
