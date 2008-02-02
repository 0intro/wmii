/* Copyright ©2007-2008 Kris Maglione <jg@suckless.org>
 * See LICENSE file for license details.
 */

#ifdef VARARGCK
# pragma varargck	argpos	debug	2
# pragma varargck	argpos	dprint	1
# pragma varargck	argpos	event	1
# pragma varargck	argpos	warning	1
#
# pragma varargck	type	"C"	Client*	
# pragma varargck	type	"r"	void
#endif

/* area.c */
void	area_attach(Area*, Frame*);
Area*	area_create(View*, Area *pos, uint w);
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
int	map_frame(Client*);
Client*	selclient(void);
int	unmap_frame(Client*);
void	update_class(Client*);
Client*	win2client(XWindow);
Rectangle	client_grav(Client*, Rectangle);

/* column.c */
char*	colmode2str(uint);
void	column_arrange(Area*, bool dirty);
void	column_attach(Area*, Frame*);
void	column_attachrect(Area*, Frame*, Rectangle);
void	column_detach(Frame*);
void	column_insert(Area*, Frame*, Frame*);
Area*	column_new(View*, Area *, uint);
void	column_remove(Frame*);
void	column_resize(Area*, int);
void	column_resizeframe(Frame*, Rectangle);
void	div_draw(Divide*);
void	div_set(Divide*, int x);
void	div_update_all(void);
int	str2colmode(const char*);

/* event.c */
void	check_x_event(IxpConn*);
void	dispatch_event(XEvent*);
uint	flushenterevents(void);
uint	flushevents(long, bool dispatch);
void	print_focus(const char*, Client*, const char*);

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
void	float_attach(Area*, Frame*);
void	float_detach(Frame*);

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
void	frame_setcursor(Frame*, Point);
void	frame_swap(Frame*, Frame*);
int	ingrabbox_p(Frame*, int x, int y);
void	move_focus(Frame*, Frame*);
Rectangle constrain(Rectangle);
Rectangle frame_client2rect(Client*, Rectangle, bool);
Rectangle frame_rect2client(Client*, Rectangle, bool);
Rectangle frame_hints(Frame*, Rectangle, Align);

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
void	init_screen(WMScreen*);

/* map.c */
MapEnt*	hash_get(Map*, const char*, int create);
void*	hash_rm(Map*, const char*);
MapEnt*	map_get(Map*, ulong, int create);
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
int	getdebug(char*);
bool	setdebug(int);
void	vdebug(int, const char*, va_list);

/* mouse.c */
void	mouse_movegrabbox(Client*);
void	mouse_resize(Client*, Align);
void	mouse_resizecol(Divide*);
void	grab_button(XWindow, uint button, ulong mod);
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
char**	view_names(void);
uint	view_newcolw(View*, int i);
void	view_restack(View*);
void	view_scale(View*, int w);
Client*	view_selclient(View*);
void	view_select(const char*);
void	view_update_all(void);
void	view_update_rect(View*);
Rectangle*	view_rects(View*, uint *num, Frame *ignore);

/* util.c */
char**	comm(int, char**, char**);
void	grep(char**, Reprog*, int);
char*	join(char**, char*);
void	refree(Regex*);
void	reinit(Regex*, char*);
void	uniq(char**);

/* utf.c */
char*	toutf8(const char*);
char*	toutf8n(const char*, size_t);

/* xdnd.c */
int	xdnd_clientmessage(XClientMessageEvent*);
void	xdnd_initwindow(Window*);

/* xext.c */
void	randr_event(XEvent*);
void	randr_init(void);
void	xext_event(XEvent*);
void	xext_init(void);

