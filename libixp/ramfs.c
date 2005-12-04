/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ixp.h"

#include <cext.h>

static File zero_file = { 0 };

int is_directory(File * f)
{
	return !f->size && f->content;
}

File *ixp_create(IXPServer * s, char *path)
{
	File *f, *p;
	char *buf = strdup(path);
	char *tok, *tok_ptr;

	if (!path)
		return 0;

	/* cannot create a directory with empty name */
	if (buf[strlen(buf) - 1] == '/')
		return 0;
	tok = strtok_r(buf, "/", &tok_ptr);
	p = s->root;
	f = p->content;
	/* first determine the path as it exists already */
	while (f && tok) {
		if (!strcmp(f->name, tok)) {
			tok = strtok_r(0, "/", &tok_ptr);
			p = f;
			if (tok && is_directory(f))
				f = f->content;
			else
				break;
			continue;
		}
		f = f->next;
	}
	if (p->size && tok) {
		free(buf);
		return 0;				/* cannot create subdirectory, if file has
								 * content */
	}
	/* only create missing parts, if file is directory */
	while (tok) {
		f = (File *) emalloc(sizeof(File));
		*f = zero_file;
		f->name = strdup(tok);
		f->parent = p;
		if (p->content) {
			p = p->content;
			while (p->next) {
				p = p->next;
			}
			p->next = f;
		} else {
			p->content = f;
		}
		p = f;
		tok = strtok_r(0, "/", &tok_ptr);
	}
	free(buf);
	return f;
}

static int comp_file_name(const void *f1, const void *f2)
{
	File *f = (File *) f1;
	File *p = (File *) f2;
	return strcmp(*(char **) f->name, *(char **) p->name);
}

static char *_ls(File * f)
{
	File *p;
	char *result = 0;
	size_t size = 1;			/* for \n */
	int num = 0, i;
	File **tmp;

	for (p = f; p; p = p->next)
		num++;
	tmp = emalloc(sizeof(File *) * num);
	i = 0;
	for (p = f; p; p = p->next) {
		size += strlen(p->name) + 1;
		if (is_directory(p))
			size++;
		tmp[i++] = p;
	}
	qsort(tmp, num, sizeof(char *), comp_file_name);
	result = emalloc(size);
	result[0] = '\0';
	for (i = 0; i < num; i++) {
		strncat(result, tmp[i]->name, size);
		if (is_directory(tmp[i]))
			strncat(result, "/\n", size);
		else
			strncat(result, "\n", size);
	}
	free(tmp);
	return result;
}

File *ixp_open(IXPServer * s, char *path)
{
	File *f;

	f = ixp_walk(s, path);
	if (!f) {
		set_error(s, "file does not exist");
		return 0;
	}
	f->lock++;
	return f;
}

void ixp_close(IXPServer * s, int fd)
{
	File *f = fd_to_file(s, fd);
	if (!f)
		set_error(s, "invalid file descriptor");
	else if (f->lock > 0)
		f->lock--;
}

size_t
ixp_read(IXPServer * s, int fd, size_t offset, void *out_buf,
		 size_t out_buf_len)
{
	File *f = fd_to_file(s, fd);
	void *result = 0;
	size_t len = 0, res_len = 0;

	if (!f) {
		set_error(s, "invalid file descriptor");
		return 0;
	}
	/* callback */
	if (f->before_read)
		f->before_read(s, f);
	if (is_directory(f)) {
		result = _ls(f->content);
		res_len = strlen(result);
	} else if (f->size) {
		result = f->content;
		res_len = f->size;
	}
	if (offset > res_len) {
		set_error(s, "invalid offset when reading file");
		if (is_directory(f))
			free(result);
		return 0;
	}
	if (result) {
		len = res_len - offset;
		if (len > out_buf_len)
			len = out_buf_len;
		memcpy(out_buf, (char *) result + offset, len);
		if (is_directory(f))
			free(result);
	}
	return len;
}

void
ixp_write(IXPServer * s, int fd, size_t offset, void *content,
		  size_t in_len)
{
	File *f = fd_to_file(s, fd);

	if (!f) {
		set_error(s, "invalid file descriptor");
		return;
	}
	if (is_directory(f)) {
		/* we cannot write to directories */
		set_error(s, "cannot write to a directory");
		return;
	}
	if (in_len) {
		/* offset 0 flushes the file */
		if (!offset || (offset + in_len > f->size)) {
			f->content = realloc(f->content, offset + in_len + 1);
			f->size = offset + in_len;
		}
		memcpy((char *) f->content + offset, content, in_len);
		/* internal EOF character */
		((char *) f->content)[f->size] = '\0';
	} else if (!offset) {
		/* blank file */
		if (f->content)
			free(f->content);
		f->content = 0;
		f->size = 0;
	}
	/* callback */
	if (f->after_write)
		f->after_write(s, f);
}

static void _ixp_remove(IXPServer * s, File * f)
{
	if (!f)
		return;
	if (f->next) {
		_ixp_remove(s, f->next);
		if (s->errstr)
			return;
	}
	if (f->lock) {
		set_error(s, "cannot remove opened file");
		return;					/* a file is opened, so stop removing tree */
	}
	if (!f->bind && is_directory(f)) {
		_ixp_remove(s, f->content);
		if (s->errstr)
			return;
	}
	if (f->content && f->size) {
		free(f->content);
	}
	if (f != s->root) {
		if (f->name) {
			free(f->name);
		}
		free(f);
	}
}

void ixp_remove_file(IXPServer * s, File * f)
{
	File *p, *n;
	set_error(s, 0);
	if (!f) {
		set_error(s, "file does not exist");
		return;
	}
	if (f->lock) {
		set_error(s, "cannot remove opened file");
		return;
	}
	/* detach */
	p = f->parent;
	n = f->next;
	f->next = 0;
	if (p) {
		if (p->content == f)
			p->content = n;
		else {
			p = p->content;
			while (p && (p->next != f))
				p = p->next;
			if (p)
				p->next = n;
		}
	}
	/* remove now */
	_ixp_remove(s, f);
}


void ixp_remove(IXPServer * s, char *path)
{
	ixp_remove_file(s, ixp_walk(s, path));
}

File *ixp_walk(IXPServer * s, char *path)
{
	File *f = 0;
	File *n;
	char *buf;
	char *tok, *tok_ptr;

	if (!path) {
		return 0;
	}
	buf = strdup(path);

	tok = strtok_r(buf, "/", &tok_ptr);
	f = s->root->content;
	if (!tok && buf[0] == '/') {
		f = s->root;
	}
	while (f && tok) {
		n = f->next;
		if (!strcmp(f->name, tok)) {
			tok = strtok_r(0, "/", &tok_ptr);
			if (tok && f->size)
				return 0;
			if (!tok)
				break;
			f = f->content;
			continue;
		}
		f = n;
	}
	if (f && (path[strlen(path) - 1] == '/') && !is_directory(f))
		f = 0;
	free(buf);
	return f;
}
