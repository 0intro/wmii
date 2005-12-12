/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../libixp2/ixp.h"

#include <cext.h>

typedef struct {
	char *name;
	int (*cmd) (char **argv);
	int min_argc;
} Command;

static int xcreate(char **argv);
static int xread(char **argv);
static int xwrite(char **argv);
static int xremove(char **argv);
static Command cmds[] = {
	{"create", xcreate, 2},
	{"read", xread, 1},
	{"write", xwrite, 2},
	{"remove", xremove, 1},
	{0, 0}
};
static IXPClient c = { 0 };

static char *version[] = {
	"wmiir - window manager improved remote - " VERSION "\n"
		" (C)opyright MMIV-MMV Anselm R. Garbe\n", 0
};

static void usage()
{
	fprintf(stderr, "%s",
			"usage: wmiir [-s <socket file>] [-v] <command> <args> [...]\n"
			"      -s    socket file (default: $WMIR_SOCKET)\n"
			"      -f    read commands from stdin\n"
			"      -v    version info\n"
			"commands:\n"
			"      create <file> <content>   -- creates file\n"
			"      read   <file/directory>   -- reads file/directory contents\n"
			"      write  <file> <content>   -- writes content to a file\n"
			"      remove <file/directory>   -- removes file\n");
	exit(1);
}

static u32 write_data(u32 fid, u8 * data, u32 count)
{
	u32 len, i, runs = count / c.fcall.iounit;

	for (i = 0; i <= runs; i++) {
		/* write */
		len = count - (i * c.fcall.iounit);
		if (len > c.fcall.iounit)
			len = c.fcall.iounit;
		if (ixp_client_write
			(&c, fid, i * c.fcall.iounit, len,
			 &data[i * c.fcall.iounit]) != count) {
			fprintf(stderr, "wmiir: cannot write file: %s\n", c.errstr);
			return 0;
		}
	}
	return count;
}

static int xcreate(char **argv)
{
	u32 fid;
	char *p = strrchr(argv[0], '/');

	fid = c.root_fid << 2;
	/* walk to bottom-most directory */
	*p = 0;
	if (!ixp_client_walk(&c, fid, argv[0])) {
		fprintf(stderr, "wmiir: cannot walk to %s: %s\n", argv[0],
				c.errstr);
		return 1;
	}
	/* create */
	p++;
	if (!ixp_client_create(&c, fid, p, (u32) 0xff, IXP_OWRITE)) {
		fprintf(stderr, "wmiir: cannot create file: %s\n", c.errstr);
		return 1;
	}
	write_data(fid, (u8 *) argv[1], strlen(argv[1]));
	return !ixp_client_close(&c, fid);
}

static int xwrite(char **argv)
{
	/* open */
	u32 fid = c.root_fid << 2;
	if (!ixp_client_open(&c, fid, argv[0], IXP_OWRITE)) {
		fprintf(stderr, "wmiir: cannot open file: %s\n", c.errstr);
		return 1;
	}
	write_data(fid, (u8 *) argv[1], strlen(argv[1]));
	return !ixp_client_close(&c, fid);
}

static void print_directory(void *result, u32 msize)
{
	void *p = result;
	static Stat stat, zerostat = { 0 };
	u32 len = 0;
	do {
		p = ixp_dec_stat(p, &stat);
		len += stat.size + sizeof(u16);
		if (stat.qid.type == IXP_QTDIR)
			fprintf(stdout, "%s/\n", stat.name);
		else
			fprintf(stdout, "%s\n", stat.name);
		stat = zerostat;
	}
	while (len < msize);
}

static int xread(char **argv)
{
	u32 count, fid = c.root_fid << 2;
	int is_directory = FALSE;
	static u8 result[IXP_MAX_MSG];

	/* open */
	if (!ixp_client_open(&c, fid, argv[0], IXP_OREAD)) {
		fprintf(stderr, "wmiir: cannot open file: %s\n", c.errstr);
		return 1;
	}
	is_directory = !c.fcall.nwqid || (c.fcall.qid.type == IXP_QTDIR);
	/* read */
	if (!(count = ixp_client_read(&c, fid, 0, result, IXP_MAX_MSG))
		&& c.errstr) {
		fprintf(stderr, "wmiir: cannot read file: %s\n", c.errstr);
		return 1;
	}
	if (count) {
		if (is_directory)
			print_directory(result, count);
		else {
			u32 i;
			for (i = 0; i < count; i++)
				putchar(result[i]);
		}
	}
	return !ixp_client_close(&c, fid);
}

static int xremove(char **argv)
{
	u32 fid;

	/* remove */
	fid = c.root_fid << 2;
	if (!ixp_client_remove(&c, fid, argv[0])) {
		fprintf(stderr, "wmiir: cannot remove file: %s\n", c.errstr);
		return 1;
	}
	return 0;
}

static int perform_cmd(int argc, char **argv)
{
	int i;
	for (i = 0; cmds[i].name; i++)
		if (!strncmp(cmds[i].name, argv[0], strlen(cmds[i].name))) {
			if (cmds[i].min_argc <= argc)
				return cmds[i].cmd(&argv[1]);
			else
				usage();
		}
	/* bogus command */
	return 1;
}

int main(int argc, char *argv[])
{
	int i = 0, ret, read_stdin = 0;
	char line[4096];
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
				if (i + 1 < argc)
					sockfile = argv[++i];
				else
					usage();
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
	if (!ixp_client_init(&c, sockfile)) {
		fprintf(stderr, "wmiir: %s\n", c.errstr);
		exit(1);
	}
	/* wether perform directly or read from stdin */
	if (read_stdin) {
		char *_argv[3];
		int _argc;
		while (fgets(line, 4096, stdin))
			if ((_argc = cext_tokenize(_argv, 3, line, ' '))) {
				if ((ret = perform_cmd(_argc, _argv)))
					break;
			}
	} else
		ret = perform_cmd(argc - (i + 1), &argv[i]);

	/* close socket */
	ixp_client_deinit(&c);

	return ret;
}
