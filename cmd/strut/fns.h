
void	debug(int, const char*, ...);
void	dispatch_event(XEvent*);
uint	flushevents(long, bool);
uint	flushenterevents(void);
void	xevent_loop(void);
void	xtime_kludge(void);

void	restrut(void);

bool	getlong(const char*, long*);
bool	getulong(const char*, ulong*);

void	ewmh_getstrut(Window*, Rectangle[4]);
void	ewmh_setstrut(Window*, Rectangle[4]);

void	printevent(XEvent *e);

