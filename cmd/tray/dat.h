#include <fmt.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <ixp.h>
#include <stuff/x.h>
#include <stuff/util.h>

#ifndef EXTERN
# define EXTERN extern
#endif

enum { OAuto, OHorizontal, OVertical };

enum XEmbedFlags {
	XEmbedMapped = (1 << 0),
};

enum TrayOpcodes {
	TrayRequestDock,
	TrayBeginMessage,
	TrayCancelMessage,
};

typedef struct Client		Client;
typedef struct Message		Message;
typedef struct Selection	Selection;
typedef struct XEmbed		XEmbed;

struct Client {
	Client*		next;
	XEmbed*		xembed;
	Window		w;
	Window*		indicator;
	Message*	message;
};

struct Message {
	Message*	next;
	long		id;
	ulong		timeout;
	IxpMsg		msg;
};

struct Selection {
	Window*	owner;
	char*	selection;
	ulong	time_start;
	ulong	time_end;
	void	(*cleanup)(Selection*);
	void	(*message)(Selection*, XClientMessageEvent*);
	void	(*request)(Selection*, XSelectionRequestEvent*);
};

struct XEmbed {
	Window*	w;
	Window*	owner;
	void	(*cleanup)(XEmbed*);
	int	version;
	ulong	flags;
};

EXTERN IxpServer	srv;
EXTERN char**		program_args;
EXTERN int		debug;

EXTERN struct {
	Window*		win;
	Image*		pixmap;
	Client*		clients;
	Selection*	selection;
	char*		tags;
	ulong		iconsize;
	ulong		padding;
	long		edge;
	int		orientation;
	Font*		font;
	CTuple		selcolors;
	CTuple		normcolors;
} tray;

