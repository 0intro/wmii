/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include <string.h>
#include "ixp.h"

/* encode/decode stuff */

void *
ixp_enc_u8(unsigned char *msg, unsigned char val)
{
    msg[0] = val;
    return &msg[1];
}

void *
ixp_dec_u8(unsigned char *msg, unsigned char *val)
{
    *val = msg[0];
    return &msg[1];
}

void *
ixp_enc_u16(unsigned char *msg, unsigned short val)
{
    msg[0] = val;
    msg[1] = val >> 8;
    return &msg[2];
}

void *
ixp_dec_u16(unsigned char *msg, unsigned short *val)
{
    *val = msg[0] | (msg[1] << 8);
    return &msg[2];
}

void *
ixp_enc_u32(unsigned char *msg, unsigned int val)
{
    msg[0] = val;
    msg[1] = val >> 8;
    msg[2] = val >> 16;
    msg[3] = val >> 24;
    return &msg[4];
}

void *
ixp_dec_u32(unsigned char *msg, unsigned int *val)
{
    *val = msg[0] | (msg[1] << 8) | (msg[2] << 16) | (msg[3] << 24);
    return &msg[4];
}

void *
ixp_enc_u64(unsigned char *msg, unsigned long long val)
{
    msg[0] = val;
    msg[1] = val >> 8;
    msg[2] = val >> 16;
    msg[3] = val >> 24;
    msg[4] = val >> 32;
    msg[5] = val >> 40;
    msg[6] = val >> 48;
    msg[7] = val >> 56;
    return &msg[8];
}

void *
ixp_dec_u64(unsigned char *msg, unsigned long long *val)
{
    *val =
        (unsigned long long) msg[0] | ((unsigned long long) msg[1] << 8) |
        ((unsigned long long) msg[2] << 16) | ((unsigned long long) msg[3]
                                               << 24) | ((unsigned long
                                                          long) msg[4] <<
                                                         32) | ((unsigned
                                                                 long long)
                                                                msg[5] <<
                                                                40) |
        ((unsigned long long) msg[6] << 48) | ((unsigned long long) msg[7]
                                               << 56);
    return &msg[8];
}

void *
ixp_enc_string(unsigned char *msg, const char *s)
{
    unsigned short len = s ? strlen(s) : 0;
    msg = ixp_enc_u16(msg, len);
    if(s)
        memcpy(msg, s, len);
    return &msg[len];
}

void *
ixp_dec_string(unsigned char *msg, char *string, unsigned short stringlen,
               unsigned short *len)
{
    msg = ixp_dec_u16(msg, len);
    if(!(*len))
        return msg;
    if(*len > stringlen - 1)
        /* might never happen if stringlen == IXP_MAX_MSG */
        string[0] = 0;
    else {
        memcpy(string, msg, *len);
        string[*len] = 0;
    }
    return &msg[*len];
}

void *
ixp_enc_data(unsigned char *msg, unsigned char *data, unsigned int datalen)
{
    memcpy(msg, data, datalen);
    return &msg[datalen];
}

void *
ixp_dec_data(unsigned char *msg, unsigned char *data, unsigned int datalen)
{
    memcpy(data, msg, datalen);
    return &msg[datalen];
}

void *
ixp_enc_prefix(unsigned char *msg, unsigned int size, unsigned char id,
               unsigned short tag)
{
    msg = ixp_enc_u32(msg, size);
    msg = ixp_enc_u8(msg, id);
    return ixp_enc_u16(msg, tag);
}

void *
ixp_dec_prefix(unsigned char *msg, unsigned int *size, unsigned char *id,
               unsigned short *tag)
{
    msg = ixp_dec_u32(msg, size);
    msg = ixp_dec_u8(msg, id);
    return ixp_dec_u16(msg, tag);
}

void *
ixp_enc_qid(unsigned char *msg, Qid * qid)
{
    msg = ixp_enc_u8(msg, qid->type);
    msg = ixp_enc_u32(msg, qid->version);
    return ixp_enc_u64(msg, qid->path);
}

void *
ixp_dec_qid(unsigned char *msg, Qid * qid)
{
    msg = ixp_dec_u8(msg, &qid->type);
    msg = ixp_dec_u32(msg, &qid->version);
    return ixp_dec_u64(msg, &qid->path);
}

void *
ixp_enc_stat(unsigned char *msg, Stat * stat)
{
    msg = ixp_enc_u16(msg, ixp_sizeof_stat(stat));
    msg = ixp_enc_u16(msg, stat->type);
    msg = ixp_enc_u32(msg, stat->dev);
    msg = ixp_enc_qid(msg, &stat->qid);
    msg = ixp_enc_u32(msg, stat->mode);
    msg = ixp_enc_u32(msg, stat->atime);
    msg = ixp_enc_u32(msg, stat->mtime);
    msg = ixp_enc_u64(msg, stat->length);
    msg = ixp_enc_string(msg, stat->name);
    msg = ixp_enc_string(msg, stat->uid);
    msg = ixp_enc_string(msg, stat->gid);
    return ixp_enc_string(msg, stat->muid);
}

void *
ixp_dec_stat(unsigned char *msg, Stat * stat, unsigned short *len)
{
	unsigned short dummy;
	msg = ixp_dec_u16(msg, len);
    msg = ixp_dec_u16(msg, &stat->type);
    msg = ixp_dec_u32(msg, &stat->dev);
    msg = ixp_dec_qid(msg, &stat->qid);
    msg = ixp_dec_u32(msg, &stat->mode);
    msg = ixp_dec_u32(msg, &stat->atime);
    msg = ixp_dec_u32(msg, &stat->mtime);
    msg = ixp_dec_u64(msg, &stat->length);
    msg = ixp_dec_string(msg, stat->name, sizeof(stat->name), &dummy);
    msg = ixp_dec_string(msg, stat->uid, sizeof(stat->uid), &dummy);
    msg = ixp_dec_string(msg, stat->gid, sizeof(stat->gid), &dummy);
    return ixp_dec_string(msg, stat->muid, sizeof(stat->muid), &dummy);
}
