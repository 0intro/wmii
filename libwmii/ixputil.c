/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "wmii.h"

#include <cext.h>

static pid_t mypid;
static char *mysockfile;

/* convenience stuff ----------------------------------------------- */

File *wmii_create_ixpfile(IXPServer * s, char *key, char *val)
{
	File *f = ixp_create(s, key);
	if (f && !is_directory(f)) {
		size_t l = val ? strlen(val) : 0;
		f->content = l ? strdup(val) : 0;
		f->size = l;
		return f;
	}
	/* forbidden, file is directory */
	return 0;
}

void wmii_get_ixppath(File * f, char *path, size_t size)
{
	char buf[512];

	buf[0] = '\0';
	if (path)
		_strlcpy(buf, path, sizeof(buf));
	snprintf(path, size, "%s/", f->name);
	if (buf[0] != '\0')
		_strlcat(path, buf, size);
	if (f->parent)
		wmii_get_ixppath(f->parent, path, size);
}

void wmii_move_ixpfile(File * f, File * to_parent)
{
	File *p = f->parent;
	File *fil = p->content;

	/* detach */
	if (p->content == f)
		p->content = fil->next;
	else {
		while (fil->next != f)
			fil = fil->next;
		fil->next = f->next;
	}
	f->next = 0;


	/* attach */
	if (!to_parent->content)
		to_parent->content = f;
	else {
		for (fil = to_parent->content; fil->next; fil = fil->next);
		fil->next = f;
	}
	f->parent = to_parent;
}

static void exit_cleanup()
{
	if (mypid == getpid())
		unlink(mysockfile);
}

IXPServer *wmii_setup_server(char *sockfile)
{
	IXPServer *s;

	if (!sockfile) {
		fprintf(stderr, "%s\n", "libwmii: no socket file provided");
		exit(1);
	}
	mysockfile = sockfile;
	mypid = getpid();
	s = init_server(sockfile, exit_cleanup);
	if (!s) {
		perror("libwmii: cannot initialize IXP server");
		exit(1);
	}
	return s;
}
