/* Copyright ©2007-2008 Kris Maglione <jg@suckless.org>
 * See LICENSE file for license details.
 */

#ifdef VARARGCK
# pragma varargck	argpos	debug	2
# pragma varargck	argpos	dprint	1
# pragma varargck	argpos	event	1
# pragma varargck	argpos	warning	1
#
# pragma varargck	type	"a"	Area*	
# pragma varargck	type	"C"	Client*	
# pragma varargck	type	"r"	void
#endif

#define foreach_area(v, s, a) \
	Area *__anext; /* Getting ugly... */  \
	for(s=0; s <= nscreens; s++)          \
		for((a)=(s < nscreens ? (v)->areas[s] : v->floating), __anext=(a)->next; (a); (void)(((a)=__anext) && (__anext=(a)->next)))

#define foreach_column(v, s, a) \
	Area *__anext; /* Getting ugly... */  \
	for(s=0; s < nscreens; s++)           \
		for((a)=(v)->areas[s], __anext=(a)->next; (a); (void)(((a)=__anext) && (__anext=(a)->next)))

#define foreach_frame(v, s, a, f) \
	Frame *__fnext;           \
	foreach_area(v, s, a)     \
		for((void)(((f)=(a)->frame) && (__fnext=(f)->anext)); (f); (void)(((f)=__fnext) && (__fnext=(f)->anext)))

#define btassert(arg, cond) \
	(cond ? fprint(1, __FILE__":%d: failed assertion: " #cond "\n", __LINE__), backtrace(arg), true : false)

/* area.c */
int	afmt(Fmt*);
void	area_attach(Area*, Frame*);
Area*	area_create(View*, Area *pos, int scrn, uint w);
void	area_destroy(Area*);
void	area_detach(Frame*);
void	area_focus(Area*);
uint	area_idx(Area*);
void	area_moveto(Area*, Frame*);
char*	area_name(Area*);
Client*	area_selclient(Area*);
void	area_setsel(Area*, Frame*);

/* bar.c */
Bar*	bar_create(Bar**, const char*);
void	bar_destroy(Bar**, Bar*);
void	bar_draw(WMScreen*);
Bar*	bar_find(Bar*, const char*);
void	bar_init(WMScreen*);
void	bar_resize(WMScreen*);
void	bar_sety(int);
void	bar_setbounds(int, int);

/* client.c */
int	Cfmt(Fmt *f);
void	apply_rules(Client*);
void	apply_tags(Client*, const char*);
void	client_configure(Client*);
Client*	client_create(XWindow, XWindowAttributes*);
void	client_destroy(Client*);
char*	client_extratags(Client*);
bool	client_floats_p(Client*);
void	client_focus(Client*);
Frame*	client_groupframe(Client*, View*);
void	client_kill(Client*, bool);
void	client_manage(Client*);
void	client_map(Client*);
void	client_message(Client*, char*, long);
void	client_prop(Client*, Atom);
void	client_reparent(Client*, Window*, Point);
void	client_resize(Client*, Rectangle);
void	client_setcursor(Client*, Cursor);
void	client_seturgent(Client*, bool, int);
void	client_setviews(Client*, char**);
void	client_unmap(Client*, int state);
Frame*	client_viewframe(Client *c, View *v);
char*	clientname(Client*);
void	focus(Client*, bool restack);
void	fullscreen(Client*, int);
Client*	group_leader(Group*);
int	map_frame(Client*);
Client*	selclient(void);
int	unmap_frame(Client*);
void	update_class(Client*);
Client*	win2client(XWindow);
Rectangle	client_grav(Client*, Rectangle);

/* column.c */
bool	column_setmode(Area*, const char*);
char*	column_getmode(Area*);
void	column_arrange(Area*, bool dirty);
void	column_attach(Area*, Frame*);
void	column_attachrect(Area*, Frame*, Rectangle);
void	column_detach(Frame*);
void	column_frob(Area*);
void	column_insert(Area*, Frame*, Frame*);
Area*	column_new(View*, Area*, int, uint);
void	column_remove(Frame*);
void	column_resize(Area*, int);
void	column_resizeframe(Frame*, Rectangle);
void	column_settle(Area*);
void	div_draw(Divide*);
void	div_set(Divide*, int x);
void	div_update_all(void);
int	stack_count(Frame*, int*);

/* event.c */
void	check_x_event(IxpConn*);
void	dispatch_event(XEvent*);
uint	flushenterevents(void);
uint	flushevents(long, bool dispatch);
void	print_focus(const char*, Client*, const char*);
void	xtime_kludge(void);

/* ewmh.c */
int	ewmh_clientmessage(XClientMessageEvent*);
void	ewmh_destroyclient(Client*);
void	ewmh_framesize(Client*);
void	ewmh_getstrut(Client*);
void	ewmh_getwintype(Client*);
void	ewmh_init(void);
void	ewmh_initclient(Client*);
void	ewmh_pingclient(Client*);
int	ewmh_prop(Client*, Atom);
long	ewmh_protocols(Window*);
void	ewmh_updateclient(Client*);
void	ewmh_updateclientlist(void);
void	ewmh_updateclients(void);
void	ewmh_updatestacking(void);
void	ewmh_updatestate(Client*);
void	ewmh_updateview(void);
void	ewmh_updateviews(void);

/* float.c */
void	float_arrange(Area*);
void	float_attach(Area*, Frame*);
void	float_detach(Frame*);
void	float_resizeframe(Frame*, Rectangle);

/* frame.c */
Frame*	frame_create(Client*, View*);
int	frame_delta_h(void);
void	frame_draw(Frame*);
void	frame_draw_all(void);
void	frame_focus(Frame*);
uint	frame_idx(Frame*);
void	frame_insert(Frame*, Frame *pos);
void	frame_remove(Frame*);
void	frame_resize(Frame*, Rectangle);
bool	frame_restack(Frame*, Frame*);
void	frame_swap(Frame*, Frame*);
int	ingrabbox_p(Frame*, int x, int y);
void	move_focus(Frame*, Frame*);
Rectangle constrain(Rectangle);
Rectangle frame_client2rect(Client*, Rectangle, bool);
WinHints  frame_gethints(Frame*);
Rectangle frame_hints(Frame*, Rectangle, Align);
Rectangle frame_rect2client(Client*, Rectangle, bool);

/* fs.c */
void	fs_attach(Ixp9Req*);
void	fs_clunk(Ixp9Req*);
void	fs_create(Ixp9Req*);
void	fs_flush(Ixp9Req*);
void	fs_freefid(Fid*);
void	fs_open(Ixp9Req*);
void	fs_read(Ixp9Req*);
void	fs_remove(Ixp9Req*);
void	fs_stat(Ixp9Req*);
void	fs_walk(Ixp9Req*);
void	fs_write(Ixp9Req*);
void	event(const char*, ...);

/* geom.c */
Align	get_sticky(Rectangle src, Rectangle dst);
Cursor	quad_cursor(Align);
Align	quadrant(Rectangle, Point);
bool	rect_contains_p(Rectangle, Rectangle);
bool	rect_haspoint_p(Point, Rectangle);
bool	rect_intersect_p(Rectangle, Rectangle);
Rectangle	rect_intersection(Rectangle, Rectangle);

/* key.c */
void	init_lock_keys(void);
void	kpress(XWindow, ulong mod, KeyCode);
ulong	str2modmask(const char*);
void	update_keys(void);

/* main.c */
void	init_screens(void);

/* map.c */
void**	hash_get(Map*, const char*, bool create);
void*	hash_rm(Map*, const char*);
void**	map_get(Map*, ulong, bool create);
void*	map_rm(Map*, ulong);

/* message.c */
bool	getlong(const char*, long*);
bool	getulong(const char*, ulong*);
char*	message_client(Client*, IxpMsg*);
char*	message_root(void*, IxpMsg*);
char*	message_view(View*, IxpMsg*);
char*	msg_getword(IxpMsg*);
char*	msg_parsecolors(IxpMsg*, CTuple*);
char*	msg_selectarea(Area*, IxpMsg*);
char*	msg_sendclient(View*, IxpMsg*, bool swap);
char*	readctl_root(void);
char*	readctl_view(View*);
Area*	strarea(View*, const char*);
void	warning(const char*, ...);
/* debug */
void	debug(int, const char*, ...);
void	dprint(const char*, ...);
void	dwrite(int, void*, int, bool);
bool	setdebug(int);
void	vdebug(int, const char*, va_list);

/* mouse.c */
Window*	constraintwin(Rectangle);
void	destroyconstraintwin(Window*);
void	grab_button(XWindow, uint button, ulong mod);
void	mouse_checkresize(Frame*, Point, bool);
void	mouse_movegrabbox(Client*, bool);
void	mouse_resize(Client*, Align, bool);
void	mouse_resizecol(Divide*);
bool	readmotion(Point*);
int	readmouse(Point*, uint*);
Align	snap_rect(Rectangle *rects, int num, Rectangle *current, Align *mask, int snapw);

/* printevent.c */
void	printevent(XEvent*);

/* rule.c */
void	trim(char *str, const char *chars);
void	update_rules(Rule**, const char*);

/* view.c */
void	view_arrange(View*);
void	view_attach(View*, Frame*);
View*	view_create(const char*);
void	view_destroy(View*);
void	view_detach(Frame*);
Area*	view_findarea(View*, int, bool);
void	view_focus(WMScreen*, View*);
bool	view_fullscreen_p(View*);
char*	view_index(View*);
void	view_init(View*, int iscreen);
char**	view_names(void);
uint	view_newcolwidth(View*, int i);
void	view_restack(View*);
void	view_scale(View*, int w);
Client*	view_selclient(View*);
void	view_select(const char*);
void	view_update(View*);
void	view_update_all(void);
void	view_update_rect(View*);
Rectangle*	view_rects(View*, uint *num, Frame *ignore);

/* _util.c */
void	backtrace(char*);
void	closeexec(int);
char**	comm(int, char**, char**);
int	doublefork(void);
void	grep(char**, Reprog*, int);
char*	join(char**, char*);
void	refree(Regex*);
void	reinit(Regex*, char*);
int	strlcatprint(char*, int, const char*, ...);
int	spawn3(int[3], const char*, char*[]);
int	spawn3l(int[3], const char*, ...);
void	uniq(char**);
int	unquote(char*, char*[], int);

/* utf.c */
char*	toutf8(const char*);
char*	toutf8n(const char*, size_t);

/* xdnd.c */
int	xdnd_clientmessage(XClientMessageEvent*);
void	xdnd_initwindow(Window*);

/* xext.c */
void	randr_event(XEvent*);
bool	render_argb_p(Visual*);
void	xext_event(XEvent*);
void	xext_init(void);
Rectangle*	xinerama_screens(int*);

