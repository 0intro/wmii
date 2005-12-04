/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include <string.h>
#include "ixp.h"

/* encode/decode stuff */

void *ixp_enc_u8(u8 * msg, u8 val)
{
	msg[0] = val;
	return &msg[1];
}

void *ixp_dec_u8(u8 * msg, u8 * val)
{
	*val = msg[0];
	return &msg[1];
}

void *ixp_enc_u16(u8 * msg, u16 val)
{
	msg[0] = val;
	msg[1] = val >> 8;
	return &msg[2];
}

void *ixp_dec_u16(u8 * msg, u16 * val)
{
	*val = msg[0] | (msg[1] << 8);
	return &msg[2];
}

void *ixp_enc_u32(u8 * msg, u32 val)
{
	msg[0] = val;
	msg[1] = val >> 8;
	msg[2] = val >> 16;
	msg[3] = val >> 24;
	return &msg[4];
}

void *ixp_dec_u32(u8 * msg, u32 * val)
{
	*val = msg[0] | (msg[1] << 8) | (msg[2] << 16) | (msg[3] << 24);
	return &msg[4];
}

void *ixp_enc_u64(u8 * msg, u64 val)
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

void *ixp_dec_u64(u8 * msg, u64 * val)
{
	*val = (u64) msg[0] | ((u64) msg[1] << 8) |
		((u64) msg[2] << 16) | ((u64) msg[3] << 24) |
		((u64) msg[4] << 32) | ((u64) msg[5] << 40) |
		((u64) msg[6] << 48) | ((u64) msg[7] << 56);
	return &msg[8];
}

void *ixp_enc_string(u8 * msg, const char *s)
{
	u16 len = s ? strlen(s) : 0;
	msg = ixp_enc_u16(msg, len);
	if (s)
		memcpy(msg, s, len);
	return &msg[len];
}

void *ixp_dec_string(u8 * msg, char *string, u16 stringlen, u16 * len)
{
	msg = ixp_dec_u16(msg, len);
	if (!(*len))
		return msg;
	if (*len > stringlen - 1)
		/* might never happen if stringlen == IXP_MAX_MSG */
		string[0] = '\0';
	else {
		memcpy(string, msg, *len);
		string[*len] = 0;
	}
	return &msg[*len];
}

void *ixp_enc_data(u8 * msg, u8 * data, u32 datalen)
{
	memcpy(msg, data, datalen);
	return &msg[datalen];
}

void *ixp_dec_data(u8 * msg, u8 * data, u32 datalen)
{
	memcpy(data, msg, datalen);
	return &msg[datalen];
}

void *ixp_enc_prefix(u8 * msg, u32 size, u8 id, u16 tag)
{
	msg = ixp_enc_u32(msg, size);
	msg = ixp_enc_u8(msg, id);
	return ixp_enc_u16(msg, tag);
}

void *ixp_dec_prefix(u8 * msg, u32 * size, u8 * id, u16 * tag)
{
	msg = ixp_dec_u32(msg, size);
	msg = ixp_dec_u8(msg, id);
	return ixp_dec_u16(msg, tag);
}

void *ixp_enc_qid(u8 * msg, Qid * qid)
{
	msg = ixp_enc_u8(msg, qid->type);
	msg = ixp_enc_u32(msg, qid->version);
	return ixp_enc_u64(msg, qid->path);
}

void *ixp_dec_qid(u8 * msg, Qid * qid)
{
	msg = ixp_dec_u8(msg, &qid->type);
	msg = ixp_dec_u32(msg, &qid->version);
	return ixp_dec_u64(msg, &qid->path);
}

void *ixp_enc_stat(u8 * msg, Stat * stat)
{
	msg = ixp_enc_u16(msg, stat->size);
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

void *ixp_dec_stat(u8 * msg, Stat * stat)
{
	u16 len;
	msg = ixp_dec_u16(msg, &stat->size);
	msg = ixp_dec_u16(msg, &stat->type);
	msg = ixp_dec_u32(msg, &stat->dev);
	msg = ixp_dec_qid(msg, &stat->qid);
	msg = ixp_dec_u32(msg, &stat->mode);
	msg = ixp_dec_u32(msg, &stat->atime);
	msg = ixp_dec_u32(msg, &stat->mtime);
	msg = ixp_dec_u64(msg, &stat->length);
	msg = ixp_dec_string(msg, stat->name, sizeof(stat->name), &len);
	msg = ixp_dec_string(msg, stat->uid, sizeof(stat->uid), &len);
	msg = ixp_dec_string(msg, stat->gid, sizeof(stat->gid), &len);
	return ixp_dec_string(msg, stat->muid, sizeof(stat->muid), &len);
}
