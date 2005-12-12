/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ixp.h"

static IXPClient *c;
static int exit_code = 0;

static char *version[] = {
	"wmiir - window manager improved remote - " VERSION "\n"
		" (C)opyright MMIV-MMV Anselm R. Garbe\n", 0
};

static void usage()
{
	fprintf(stderr, "%s",
			"usage: wmiir [-s <socket file>] [-v] <action> <action_arg> [...]\n"
			"      -s    socket file (default: $WMIR_SOCKET)\n"
			"      -f    read actions from stdin\n"
			"      -v    version info\n"
			"actions:\n"
			"      create <file> [<content>] -- creates and optionally writes content to a file\n"
			"      write  <file> <content>   -- writes content to a file\n"
			"      read   <directory/file>   -- reads file or directory contents\n"
			"      remove <directory/file>   -- removes file or directory, use with care!\n");
	exit(1);
}

static void perform(char *action, char *file, char *content)
{
	size_t out_len = 0;
	char output[2050];
	int crt, fd = -1;

	if (!action)
		return;

	crt = !strncmp(action, "create", 7);
	if (!strncmp(action, "write", 6) || crt) {
		if (!file)
			return;
		/* create file first */
		if (crt) {
			c->create(c, file);
			if (c->errstr) {
				fprintf(stderr, "wmiir: error: create %s: %s\n", file,
						c->errstr);
				exit_code = 1;
				return;
			}
		}
		if (!content)
			return;
		fd = c->open(c, file);
		if (c->errstr) {
			fprintf(stderr, "wmiir: error: open %s: %s\n", file, c->errstr);
			exit_code = 1;
			return;
		}
		c->write(c, fd, content, strlen(content));
		if (c->errstr) {
			fprintf(stderr, "wmiir: error: write %s: %s\n", file,
					c->errstr);
			exit_code = 1;
			if (!strncmp(c->errstr, DEAD_SERVER, strlen(DEAD_SERVER) + 1))
				return;
		}
	} else if (!strncmp(action, "read", 5)) {
		if (!file)
			return;
		fd = c->open(c, file);
		if (c->errstr) {
			fprintf(stderr, "wmiir: error: open %s: %s\n", file, c->errstr);
			exit_code = 1;
			return;
		}
		do {
			out_len = c->read(c, fd, output, 2048);
			if (c->errstr) {
				fprintf(stderr, "wmiir: error: read %s: %s\n", file,
						c->errstr);
				exit_code = 1;
				if (!strncmp
					(c->errstr, DEAD_SERVER, strlen(DEAD_SERVER) + 1))
					return;
				break;
			}
			output[out_len] = 0;
			fprintf(stdout, "%s", output);
		}
		while (out_len == 2048);
		fprintf(stdout, "%s", "\n");
	} else if (!strncmp(action, "remove", 7)) {
		if (!file)
			return;
		c->remove(c, file);
		if (c->errstr) {
			fprintf(stderr, "wmiir: error: remove %s: %s\n", file,
					c->errstr);
			exit_code = 1;
			return;
		}
	}
	if (fd != -1) {
		c->close(c, fd);
		if (c->errstr) {
			fprintf(stderr, "wmiir: error: close %s: %s\n", file,
					c->errstr);
			exit_code = 1;
			return;
		}
	}
}

int main(int argc, char *argv[])
{
	int i = 0, read_stdin = 0;
	char line[4096], *p;
	char *sockfile = getenv("WMIR_SOCKET");

	/* command line args */
	if (argc > 1) {
		for (i = 1; (i < argc) && (argv[i][0] == '-'); i++) {
			switch (argv[i][1]) {
			case 'v':
				fprintf(stderr, "%s", version[0]);
				exit(0);
				break;
			case 's':
				if (i + 1 < argc) {
					sockfile = argv[++i];
				} else {
					usage();
				}
				break;
			case 'f':
				read_stdin = 1;
				break;
			default:
				usage();
				break;
			}
		}
	}
	if ((argc <= 1) || (!read_stdin && (i + 1) >= argc)) {
		fprintf(stderr, "%s", "wmiir: arguments: ");
		for (i = 1; i < argc; i++)
			fprintf(stderr, "%s, ", argv[i]);
		fprintf(stderr, "%s", "\n");
		usage();
	}
	if (!sockfile) {
		fprintf(stderr, "%s",
				"wmiir: error: WMIR_SOCKET environment not set\n");
		usage();
	}
	/* open socket */
	if (!(c = init_ixp_client(sockfile))) {
		fprintf(stderr, "wmiir: cannot connect to server '%s'\n", sockfile);
		exit(1);
	}
	if (read_stdin) {
		/* simple shell */
		char *action, *file, *content;
		while (fgets(line, 4096, stdin)) {
			p = line;
			while (*p != 0 && (*p == ' ' || *p == '\t'))
				p++;
			if (*p == 0)
				continue;		/* empty line */
			if (strncmp(p, "create ", 7) &&
				strncmp(p, "write ", 6) &&
				strncmp(p, "read ", 5) && strncmp(p, "remove ", 7))
				continue;

			action = p;
			while (*p != 0 && *p != ' ' && *p != '\t' && *p != '\n')
				p++;
			if (*p == 0 || *p == '\n')
				continue;		/* ignore bogus command */
			*p = 0;
			p++;
			while (*p != 0 && (*p == ' ' || *p == '\t'))
				p++;
			if (*p == 0)
				continue;		/* ignore bogus command */
			file = p;
			while (*p != 0 && *p != ' ' && *p != '\t' && *p != '\n')
				p++;
			if (*p == 0 || *p == '\n') {
				content = 0;
				*p = 0;
			} else {
				*p = 0;
				p++;
				content = p;
			}
			if (file[0] == 0)
				continue;
			if (content) {
				static size_t len;
				if ((len = strlen(content)))
					content[len - 1] = 0;
			}
			perform(action, file, content);
			if (c->errstr)
				fprintf(stderr, "wmiir: error: read %s: %s\n", file,
						c->errstr);
		}
	} else {
		perform(argv[i], argv[i + 1], (i + 2) < argc ? argv[i + 2] : 0);
	}

	if (c->errstr) {
		deinit_client(c);
		exit_code = 1;
	}
	return exit_code;
}
