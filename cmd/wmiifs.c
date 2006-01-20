/*
 * (C)opyright MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wmii.h"

/* array indexes for file pointers */
typedef enum {
    F_CTL,
    F_LAST
} FsIndexes;

typedef struct Route Route;
struct Route {
    File dest;
    int src;
};

typedef struct Bind Bind;
struct Bind {
    IXPClient *client;
    Route route[MAX_CONN * MAX_OPEN_FILES];
    File *mount;
    char *prefix;
    Bind *next;
};

static Display *dpy;
static IXPServer *ixps;
static char *sockfile = nil;
static File *files[F_LAST];
static Bind *bindings = nil;

static void quit(void *obj, char *arg);
static void bind(void *obj, char *arg);
static void unbind(void *obj, char *arg);

static Action acttbl[] = {
    {"quit", quit},
    {"unbind", unbind},
    {"bind", bind},
    {0, 0}
};

static char *version[] = {
    "wmiifs - window manager improved filesystem - " VERSION "\n"
        " (C)opyright MMVI Anselm R. Garbe\n", 0
};

static void
usage()
{
    fprintf(stderr,
            "usage: wmiifs -s <socket file> [-v]\n"
            "      -s   socket file\n" "      -v   version info\n");
    exit(1);
}

static void
quit(void *obj, char *arg)
{
    Bind *b;
    while((b = bindings)) {
        bindings = bindings->next;
        if(b->mount) {
            b->mount->content = nil;
            ixp_remove(ixps, b->prefix);
            if(ixps->errstr)
                fprintf(stderr, "wmiifs: error on unbind %s: %s\n",
                        b->prefix, ixps->errstr);
        }
        free(b->prefix);
        free(b);
    }
    bindings = nil;
    ixps->runlevel = SHUTDOWN;
}

static void
do_unbind(Bind * bind)
{
    Bind *b;
    if(bind == bindings)
        bindings = bind->next;
    else {
        for(b = bindings; b && b->next != bind; b = b->next);
        b->next = bind->next;
    }
    /* free stuff */
    deinit_client(bind->client);
    free(bind->prefix);
    free(bind);
}

static void
unbind(void *obj, char *arg)
{
    Bind *b;

    for(b = bindings; b && strncmp(b->prefix, arg, strlen(b->prefix));
        b = b->next);

    if(!b) {
        fprintf(stderr, "wmiifs: unbind: '%s' no such path\n", arg);
        return;
    }
    do_unbind(b);
}

static void
bind(void *obj, char *arg)
{
    Bind *b, *new = nil;
    char cmd[1024];
    char *sfile;

    if(!arg)
        return;
    cext_strlcpy(cmd, arg, sizeof(cmd));
    sfile = strchr(cmd, ' ');
    if(!sfile) {
        fprintf(stderr,
                "wmiifs: bind: '%s' without socket argument, ignoring\n",
                arg);
        return;                 /* shortcut with empty argument */
    }
    *sfile = 0;
    sfile++;
    if(*sfile == 0) {
        fprintf(stderr,
                "wmiifs: bind: '%s' without socket argument, ignoring\n",
                arg);
        return;                 /* shortcut with empty argument */
    }
    new = cext_emallocz(sizeof(Bind));
    new->client = init_ixp_client(sfile);

    if(!new->client) {
        fprintf(stderr,
                "wmiifs: bind: cannot connect to server '%s', ignoring\n",
                sfile);
        free(new);
        return;
    }
    new->prefix = strdup(cmd);
    new->mount = ixp_create(ixps, new->prefix);
    new->mount->content = new->mount;   /* shall be a directory */

    if(!bindings)
        bindings = new;
    else {
        for(b = bindings; b && b->next; b = b->next);
        b->next = new;
    }
}

static void
handle_after_write(IXPServer * s, File * f)
{
    int i;
    size_t len;

    for(i = 0; acttbl[i].name; i++) {
        len = strlen(acttbl[i].name);
        if(!strncmp(acttbl[i].name, (char *) f->content, len)) {
            if(strlen(f->content) > len) {
                acttbl[i].func(0, &((char *) f->content)[len + 1]);
            } else {
                acttbl[i].func(0, 0);
            }
            break;
        }
    }
}

static Bind *
fd_to_bind(int fd, int *client_fd)
{
    File *f = fd_to_file(ixps, fd);
    unsigned int i;
    Bind *b;

    if(!f)
        return nil;
    for(b = bindings; b; b = b->next) {
        for(i = 0; i < MAX_CONN * MAX_OPEN_FILES; i++) {
            if(&b->route[i].dest == f) {
                *client_fd = b->route[i].src;
                return b;
            }
        }
    }
    return nil;
}

static File *
fixp_create(IXPServer * s, char *path)
{
    Bind *b;
    size_t len;

    for(b = bindings; b && strncmp(b->prefix, path, strlen(b->prefix));
        b = b->next);

    if(!b) {
        File *f = ixp_create(s, path);
        return f;
    }
    /* route to b */
    len = strlen(b->prefix);
    b->client->create(b->client, path[len] == 0 ? "/" : &path[len]);
    if(b->client->errstr) {
        if(!strcmp(b->client->errstr, DEAD_SERVER))
            do_unbind(b);
    }
    return nil;
}

static File *
fixp_open(IXPServer * s, char *path)
{
    Bind *b;
    int fd;
    size_t len;

    for(b = bindings; b && strncmp(b->prefix, path, strlen(b->prefix));
        b = b->next);

    if(!b) {
        File *f = ixp_open(s, path);
        return f;
    }
    /* route to b */
    len = strlen(b->prefix);
    fd = b->client->open(b->client, path[len] == 0 ? "/" : &path[len]);
    if(b->client->errstr) {
        set_error(s, b->client->errstr);
        if(!strcmp(b->client->errstr, DEAD_SERVER))
            do_unbind(b);
        return nil;
    }
    b->route[fd].src = fd;
    return &b->route[fd].dest;
}

static size_t
fixp_read(IXPServer * s, int fd, size_t offset, void *out_buf,
          size_t out_buf_len)
{
    int cfd;
    Bind *b = fd_to_bind(fd, &cfd);
    size_t result;

    if(!b) {
        result = ixp_read(s, fd, offset, out_buf, out_buf_len);
        return result;
    }
    /* route to b */
    result = seek_read(b->client, cfd, offset, out_buf, out_buf_len);
    if(b->client->errstr) {
        set_error(s, b->client->errstr);
        if(!strcmp(b->client->errstr, DEAD_SERVER))
            do_unbind(b);
    }
    return result;
}

static void
fixp_write(IXPServer * s, int fd, size_t offset, void *content,
           size_t in_len)
{
    int cfd;
    Bind *b = fd_to_bind(fd, &cfd);

    if(!b) {
        ixp_write(s, fd, offset, content, in_len);
        return;
    }
    /* route to b */
    seek_write(b->client, cfd, offset, content, in_len);
    if(b->client->errstr) {
        set_error(s, b->client->errstr);
        if(!strcmp(b->client->errstr, DEAD_SERVER))
            do_unbind(b);
    }
}

static void
fixp_close(IXPServer * s, int fd)
{
    int cfd;
    Bind *b = fd_to_bind(fd, &cfd);

    if(!b) {
        ixp_close(s, fd);
        return;
    }
    /* route to b */
    b->client->close(b->client, cfd);
    if(b->client->errstr) {
        set_error(s, b->client->errstr);
        if(!strcmp(b->client->errstr, DEAD_SERVER))
            do_unbind(b);
    }
}

static void
fixp_remove(IXPServer * s, char *path)
{
    Bind *b;
    size_t len;

    for(b = bindings; b && strncmp(b->prefix, path, strlen(b->prefix));
        b = b->next);

    if(!b) {
        ixp_remove(s, path);
        return;
    }
    /* route to b */
    len = strlen(b->prefix);
    b->client->remove(b->client, path[len] == 0 ? "/" : &path[len]);
    if(b->client->errstr) {
        set_error(s, b->client->errstr);
        if(!strcmp(b->client->errstr, DEAD_SERVER))
            do_unbind(b);
    }
}

static void
check_event(Connection * e)
{
    XEvent ev;
    while(XPending(dpy)) {
        /*
         * wmiifs isn't interested in any X events, so just drop them
         * all
         */
        XNextEvent(dpy, &ev);
    }
    /* why check them? because X won't kill wmiifs when X dies */
}

static void
run()
{
    if(!(files[F_CTL] = ixp_create(ixps, "/ctl"))) {
        perror("wmiifs: cannot connect IXP server");
        exit(1);
    }
    files[F_CTL]->after_write = handle_after_write;

    /* routing functions */
    ixps->create = fixp_create;
    ixps->remove = fixp_remove;
    ixps->open = fixp_open;
    ixps->close = fixp_close;
    ixps->read = fixp_read;
    ixps->write = fixp_write;

    /* main event loop */
    run_server_with_fd_support(ixps, ConnectionNumber(dpy),
                               check_event, 0);

    deinit_server(ixps);
}

int
main(int argc, char *argv[])
{
    int i;

    /* command line args */
    for(i = 1; (i < argc) && (argv[i][0] == '-'); i++) {
        switch (argv[i][1]) {
        case 'v':
            fprintf(stdout, "%s", version[0]);
            exit(0);
            break;
        case 's':
            if(i + 1 < argc)
                sockfile = argv[++i];
            else
                usage();
            break;
        default:
            usage();
            break;
        }
    }

    if(!getenv("HOME")) {
        fprintf(stderr, "%s",
                "wmiifs: $HOME environment variable is not set\n");
        usage();
    }
    /* just for the case X crashes/gets quit */
    dpy = XOpenDisplay(0);
    if(!dpy) {
        fprintf(stderr, "%s", "wmiifs: cannot open display\n");
        exit(1);
    }
    ixps = wmii_setup_server(sockfile);

    run();

    return 0;
}
