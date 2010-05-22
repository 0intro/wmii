/* Copyright ©2007-2010 Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */

#include <stuff/geom.h>

#include <stdarg.h>

#include <regexp9.h>

/* Types */

typedef struct Regex Regex;

struct Regex {
	char*	regex;
	Reprog*	regc;
};

enum {
	CLeft = 1<<0,
	CCenter = 1<<1,
	CRight = 1<<2,
};
enum DebugOpt {
	D9p	= 1<<0,
	DDnd	= 1<<1,
	DEvent	= 1<<2,
	DEwmh	= 1<<3,
	DFocus	= 1<<4,
	DGeneric= 1<<5,
	DStack  = 1<<6,
	NDebugOpt = 7,
};
enum {
	GInvert = 1<<0,
};

#ifdef VARARGCK
# pragma varargck	argpos	_die	3
# pragma varargck	argpos	fatal	1
# pragma varargck	argpos	sxprint	1
#endif

#define strlcat stuff_strlcat
#define strcasestr stuff_strcasestr

void	_die(char*, int, char*, ...);
void	backtrace(char*);
void	closeexec(int);
char**	comm(int, char**, char**);
int	doublefork(void);
void*	emalloc(uint);
void*	emallocz(uint);
void*	erealloc(void*, uint);
char*	estrdup(const char*);
char*	estrndup(const char*, uint);
void	fatal(const char*, ...);
void*	freelater(void*);
int	getbase(const	char**,	long*);
bool	getint(const	char*,	int*);
bool	getlong(const	char*,	long*);
bool	getulong(const	char*,	ulong*);
void	grep(char**, Reprog*, int);
char*	join(char**, char*);
int	max(int, int);
int	min(int, int);
char*	pathsearch(const char*, const char*, bool);
void	refree(Regex*);
void	reinit(Regex*, char*);
int	spawn3(int[3], const char*, char*[]);
int	spawn3l(int[3], const char*, ...);
uint	stokenize(char**, uint, char*, char*);
char*	strcasestr(const char*, const char*);
char*	strend(char*,	int);
uint	strlcat(char*, const char*, uint);
int	strlcatprint(char*, int, const char*, ...);
char*	sxprint(const char*, ...);
uint	tokenize(char**, uint, char*, char);
void	uniq(char**);
int	unquote(char*, char*[], int);
int	utflcpy(char*, const char*, int);
char*	vsxprint(const char*, va_list);

extern char	buffer[8092];
extern char*	_buffer;
extern char*	const _buf_end;

#define bufclear() \
	BLOCK( _buffer = buffer; _buffer[0] = '\0' )
#define bufprint(...) \
	_buffer = seprint(_buffer, _buf_end, __VA_ARGS__)

#define die(...) \
	_die(__FILE__, __LINE__, __VA_ARGS__)

char *argv0;
#undef ARGBEGIN
#undef ARGEND
#undef ARGF
#undef EARGF
#define ARGBEGIN \
		int _argtmp=0, _inargv; char *_argv=nil;        \
		if(!argv0) argv0=*argv; argv++, argc--;         \
		_inargv=1; USED(_inargv);		        \
		while(argc && argv[0][0] == '-') {              \
			_argv=&argv[0][1]; argv++; argc--;      \
			if(_argv[0] == '-' && _argv[1] == '\0') \
				break; \
			while(*_argv) switch(*_argv++)
#define ARGEND }_inargv=0;USED(_argtmp, _argv, _inargv)

#define EARGF(f) ((_inargv && *_argv) ? \
			(_argtmp=strlen(_argv), _argv+=_argtmp, _argv-_argtmp) \
			: ((argc > 0) ? \
				(--argc, ++argv, _used(argc), *(argv-1)) \
				: ((f), (char*)0)))
#define ARGF() EARGF(_used(0))

/* map.c */
typedef struct Map Map;
typedef struct MapEnt MapEnt;

struct Map {
	MapEnt**bucket;
	uint	nhash;
};

void**	hash_get(Map*, const char*, bool create);
void*	hash_rm(Map*, const char*);
void**	map_get(Map*, ulong, bool create);
void*	map_rm(Map*, ulong);

/* Yuck. */
#define VECTOR(type, nam, c) \
typedef struct Vector_##nam Vector_##nam;      \
struct Vector_##nam {                          \
	type*	ary;                           \
	long	n;                             \
	long	size;                          \
};                                             \
void	vector_##c##free(Vector_##nam*);       \
void	vector_##c##init(Vector_##nam*);       \
void	vector_##c##push(Vector_##nam*, type); \

VECTOR(long, long, l)
VECTOR(Rectangle, rect, r)
VECTOR(void*, ptr, p)
#undef  VECTOR

