/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include <string.h>

#include "ixp.h"

#include <cext.h>

/* request messages ------------------------------------------------ */

void *tcreate_message(char *path, size_t * msg_len)
{
	char *msg;
	ReqHeader h;
	*msg_len = sizeof(ReqHeader) + strlen(path) + 1;
	msg = emalloc(*msg_len);
	h.req = TCREATE;
	memcpy(msg, &h, sizeof(ReqHeader));
	memcpy(msg + sizeof(ReqHeader), path, strlen(path) + 1);
	return msg;
}

void *topen_message(char *path, size_t * msg_len)
{
	char *msg;
	ReqHeader h;
	*msg_len = sizeof(ReqHeader) + strlen(path) + 1;
	msg = emalloc(*msg_len);
	h.req = TOPEN;
	memcpy(msg, &h, sizeof(ReqHeader));
	memcpy(msg + sizeof(ReqHeader), path, strlen(path) + 1);
	return msg;
}

void *tread_message(int fd, size_t offset, size_t buf_len,
					size_t * msg_len)
{
	char *msg;
	ReqHeader h;
	*msg_len = sizeof(ReqHeader);
	msg = emalloc(*msg_len);
	h.req = TREAD;
	h.fd = fd;
	h.offset = offset;
	h.buf_len = buf_len;
	memcpy(msg, &h, sizeof(ReqHeader));
	return msg;
}

void *twrite_message(int fd, size_t offset, void *content,
					 size_t content_len, size_t * msg_len)
{
	char *msg;
	ReqHeader h;
	*msg_len = sizeof(ReqHeader) + content_len;
	msg = emalloc(*msg_len);
	h.req = TWRITE;
	h.fd = fd;
	h.offset = offset;
	h.buf_len = content_len;
	memcpy(msg, &h, sizeof(ReqHeader));
	memcpy(msg + sizeof(ReqHeader), content, content_len);
	return msg;
}

void *tclose_message(int fd, size_t * msg_len)
{
	char *msg;
	ReqHeader h;
	*msg_len = sizeof(ReqHeader);
	msg = emalloc(*msg_len);
	h.req = TCLUNK;
	h.fd = fd;
	memcpy(msg, &h, sizeof(ReqHeader));
	return msg;
}

void *tremove_message(char *path, size_t * msg_len)
{
	char *msg;
	ReqHeader h;
	*msg_len = sizeof(ReqHeader) + strlen(path) + 1;
	msg = emalloc(*msg_len);
	h.req = TREMOVE;
	memcpy(msg, &h, sizeof(ReqHeader));
	memcpy(msg + sizeof(ReqHeader), path, strlen(path) + 1);
	return msg;
}

/* response messages ----------------------------------------------- */

void *rcreate_message(size_t * msg_len)
{
	char *msg;
	ResHeader h;
	*msg_len = sizeof(ResHeader);
	msg = emalloc(*msg_len);
	h.res = RCREATE;
	memcpy(msg, &h, sizeof(ResHeader));
	return msg;
}

void *ropen_message(int fd, size_t * msg_len)
{
	char *msg;
	ResHeader h;
	*msg_len = sizeof(ResHeader);
	msg = emalloc(*msg_len);
	h.res = ROPEN;
	h.fd = fd;
	memcpy(msg, &h, sizeof(ResHeader));
	return msg;
}

void *rread_message(void *content, size_t content_len, size_t * msg_len)
{
	char *msg;
	ResHeader h;
	*msg_len = sizeof(ResHeader) + content_len;
	msg = emalloc(*msg_len);
	h.res = RREAD;
	h.buf_len = content_len;
	memcpy(msg, &h, sizeof(ResHeader));
	memmove(msg + sizeof(ResHeader), content, content_len);
	return msg;
}

void *rwrite_message(size_t * msg_len)
{
	char *msg;
	ResHeader h;
	*msg_len = sizeof(ResHeader);
	msg = emalloc(*msg_len);
	h.res = RWRITE;
	memcpy(msg, &h, sizeof(ResHeader));
	return msg;
}

void *rclose_message(size_t * msg_len)
{
	char *msg;
	ResHeader h;
	*msg_len = sizeof(ResHeader);
	msg = emalloc(*msg_len);
	h.res = RCLUNK;
	memcpy(msg, &h, sizeof(ResHeader));
	return msg;
}

void *rremove_message(size_t * msg_len)
{
	char *msg;
	ResHeader h;
	*msg_len = sizeof(ResHeader);
	msg = emalloc(*msg_len);
	h.res = RREMOVE;
	memcpy(msg, &h, sizeof(ResHeader));
	return msg;
}

void *rerror_message(char *errstr, size_t * msg_len)
{
	char *msg;
	size_t len = strlen(errstr) + 1;
	ResHeader h;
	*msg_len = sizeof(ResHeader) + len;
	msg = emalloc(*msg_len);
	h.res = RERROR;
	memcpy(msg, &h, sizeof(ResHeader));
	memmove(msg + sizeof(ResHeader), errstr, len);
	return msg;
}
