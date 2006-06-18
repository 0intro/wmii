/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include <string.h>
#include "ixp.h"

/* packode/unpackode stuff */

void
ixp_pack_u8(unsigned char **msg, int *msize, unsigned char val)
{
	if((*msize -= 1) >= 0)
		*(*msg)++ = val;
}

void
ixp_unpack_u8(unsigned char **msg, unsigned char *val)
{
	*val = *(*msg)++;
}

void
ixp_pack_u16(unsigned char **msg, int *msize, unsigned short val)
{
	if((*msize -= 2) >= 0) {
		*(*msg)++ = val;
		*(*msg)++ = val >> 8;
	}
}

void
ixp_unpack_u16(unsigned char **msg, unsigned short *val)
{
	*val = *(*msg)++;
	*val |= *(*msg)++ << 8;
}

void
ixp_pack_u32(unsigned char **msg, int *msize, unsigned int val)
{
	if((*msize -= 4) >= 0) {
		*(*msg)++ = val;
		*(*msg)++ = val >> 8;
		*(*msg)++ = val >> 16;
		*(*msg)++ = val >> 24;
	}
}

void
ixp_unpack_u32(unsigned char **msg, unsigned int *val)
{
	*val = *(*msg)++;
	*val |= *(*msg)++ << 8;
	*val |= *(*msg)++ << 16;
	*val |= *(*msg)++ << 24;
}

void
ixp_pack_u64(unsigned char **msg, int *msize, unsigned long long val)
{
	if((*msize -= 8) >= 0) {
		*(*msg)++ = val;
		*(*msg)++ = val >> 8;
		*(*msg)++ = val >> 16;
		*(*msg)++ = val >> 24;
		*(*msg)++ = val >> 32;
		*(*msg)++ = val >> 40;
		*(*msg)++ = val >> 48;
		*(*msg)++ = val >> 56;
	}
}

void
ixp_unpack_u64(unsigned char **msg, unsigned long long *val)
{
	int i;
	*val = 0;
	for (i = 0; i < 8; ++i)
		*val |= (unsigned long long) *(*msg)++ << (8 * i);
}

void
ixp_pack_string(unsigned char **msg, int *msize, const char *s)
{
	unsigned short len = s ? strlen(s) : 0;
	ixp_pack_u16(msg, msize, len);
	if(s)
		ixp_pack_data(msg, msize, (void *)s, len);
}

void
ixp_unpack_strings(unsigned char **msg, unsigned short n, char **strings) {
	unsigned char *s = *msg;
	unsigned int i, size = 0;
	unsigned short len;
	/* XXX: a specially crafted packet could make this read past the end of the buffer */
	for(i=0; i<n; i++) {
		ixp_unpack_u16(&s, &len);
		s += len;
		size += len + 1; /* for '\0' */
	}
	if(!size) {
		/* So we don't try to free some random value */
		*strings = nil;
		return;
	}
	/* XXX: we don't really need mallocz here */
	s = cext_emallocz(size);
	for(i=0; i < n; i++) {
		ixp_unpack_u16(msg, &len);
		memcpy(s, *msg, len);
		s[len] = '\0';
		strings[i] = s;
		*msg += len;
		s += len + 1;
	}
}

void
ixp_unpack_string(unsigned char **msg, char **string, unsigned short *len)
{
	ixp_unpack_u16(msg, len);
	*string = nil;
	if (!*len)
		return;
	/* XXX we don't really need emallocz here */
	*string = cext_emallocz(*len+1);
	memcpy(*string, *msg, *len);
	(*string)[*len] = 0;
	*msg += *len;
}

void
ixp_pack_data(unsigned char **msg, int *msize, unsigned char *data, unsigned int datalen)
{
	if((*msize -= datalen) >= 0) {
		memcpy(*msg, data, datalen);
		*msg += datalen;
	}
}

void
ixp_unpack_data(unsigned char **msg, unsigned char **data, unsigned int datalen)
{
	/* XXX: this could be too large a number */
	*data = cext_emallocz(datalen);
	memcpy(*data, *msg, datalen);
	*msg += datalen;
}

void
ixp_pack_prefix(unsigned char *msg, unsigned int size, unsigned char id,
		unsigned short tag)
{
	int dummy = sizeof(unsigned char) +
		sizeof(unsigned short) + sizeof(unsigned int);
	ixp_pack_u32(&msg, &dummy, size);
	ixp_pack_u8(&msg, &dummy, id);
	ixp_pack_u16(&msg, &dummy, tag);
}

void
ixp_unpack_prefix(unsigned char **msg, unsigned int *size, unsigned char *id,
		unsigned short *tag)
{
	ixp_unpack_u32(msg, size);
	ixp_unpack_u8(msg, id);
	ixp_unpack_u16(msg, tag);
}

void
ixp_pack_qid(unsigned char **msg, int *msize, Qid * qid)
{
	ixp_pack_u8(msg, msize, qid->type);
	ixp_pack_u32(msg, msize, qid->version);
	ixp_pack_u64(msg, msize, qid->path);
}

void
ixp_unpack_qid(unsigned char **msg, Qid * qid)
{
	ixp_unpack_u8(msg, &qid->type);
	ixp_unpack_u32(msg, &qid->version);
	ixp_unpack_u64(msg, &qid->path);
}

void
ixp_pack_stat(unsigned char **msg, int *msize, Stat * stat)
{
	ixp_pack_u16(msg, msize, ixp_sizeof_stat(stat) - sizeof(unsigned short));
	ixp_pack_u16(msg, msize, stat->type);
	ixp_pack_u32(msg, msize, stat->dev);
	ixp_pack_qid(msg, msize, &stat->qid);
	ixp_pack_u32(msg, msize, stat->mode);
	ixp_pack_u32(msg, msize, stat->atime);
	ixp_pack_u32(msg, msize, stat->mtime);
	ixp_pack_u64(msg, msize, stat->length);
	ixp_pack_string(msg, msize, stat->name);
	ixp_pack_string(msg, msize, stat->uid);
	ixp_pack_string(msg, msize, stat->gid);
	ixp_pack_string(msg, msize, stat->muid);
}

void
ixp_unpack_stat(unsigned char **msg, Stat * stat)
{
	unsigned short dummy;
	*msg += sizeof(unsigned short);
	ixp_unpack_u16(msg, &stat->type);
	ixp_unpack_u32(msg, &stat->dev);
	ixp_unpack_qid(msg, &stat->qid);
	ixp_unpack_u32(msg, &stat->mode);
	ixp_unpack_u32(msg, &stat->atime);
	ixp_unpack_u32(msg, &stat->mtime);
	ixp_unpack_u64(msg, &stat->length);
	ixp_unpack_string(msg, &stat->name, &dummy);
	ixp_unpack_string(msg, &stat->uid, &dummy);
	ixp_unpack_string(msg, &stat->gid, &dummy);
	ixp_unpack_string(msg, &stat->muid, &dummy);
}
