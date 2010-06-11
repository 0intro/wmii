/* Copyright ©2007-2010 Kris Maglione <jg@suckless.org>
 * See LICENSE file for license details.
 */

#include <setjmp.h>

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

#define _cond(cond, n) (cond) && __alive++ == n
#define _cont(cont) (void)(__alive--, cont)

#define with(type, var) \
	for(type var=(type)-1; (var == (type)-1) && ((var=0) || true);)

/* Grotesque, but worth it. */

#define foreach_area(v, s, a) \
	with(int, __alive)                            \
	with(Area*, __anext)                          \
	for(s=0; _cond(s <= nscreens, 0); _cont(s++)) \
		for((a)=(s < nscreens ? (v)->areas[s] : v->floating), __anext=(a)->next; _cond(a, 1); _cont(((a)=__anext) && (__anext=(a)->next)))

#define foreach_column(v, s, a) \
	with(int, __alive)                           \
	with(Area*, __anext)                         \
	for(s=0; _cond(s < nscreens, 0); _cont(s++)) \
		for((a)=(v)->areas[s], __anext=(a)->next; _cond(a, 1); _cont(((a)=__anext) && (__anext=(a)->next)))

#define foreach_frame(v, s, a, f) \
	with(Frame*, __fnext)     \
	foreach_area(v, s, a)     \
		for((void)(((f)=(a)->frame) && (__fnext=(f)->anext)); _cond(f, 2); _cont(((f)=__fnext) && (__fnext=(f)->anext)))

#define btassert(arg, cond) \
	(cond ? fprint(1, __FILE__":%d: failed assertion: " #cond "\n", __LINE__), backtrace(arg), true : false)

/* area.c */
int	afmt(Fmt*);
void	area_attach(Area*, Frame*);
Area*	area_create(View*, Area *pos, int scrn, uint w);
void	area_destroy(Area*);
void	area_detach(Frame*);
Area*	area_find(View*, Rectangle, int, bool);
void	area_focus(Area*);
int	area_idx(Area*);
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
void	bar_load(Bar*);
void	bar_resize(WMScreen*);
void	bar_sety(WMScreen*, int);
void	bar_setbounds(WMScreen*, int, int);

/* client.c */
int	Cfmt(Fmt *f);
bool	client_applytags(Client*, const char*);
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
bool	client_prop(Client*, Atom);
void	client_reparent(Client*, Window*, Point);
void	client_resize(Client*, Rectangle);
void	client_setcursor(Client*, Cursor);
void	client_seturgent(Client*, int, int);
void	client_setviews(Client*, char**);
void	client_unmap(Client*, int state);
Frame*	client_viewframe(Client *c, View *v);
void	focus(Client*, bool user);
void	fullscreen(Client*, int, long);
void	group_init(Client*);
Client*	group_leader(Group*);
void	group_remove(Client*);
int	client_mapframe(Client*);
Client*	selclient(void);
int	client_unmapframe(Client*);
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
int	column_minwidth(void);
Area*	column_new(View*, Area*, int, uint);
void	column_remove(Frame*);
void	column_resize(Area*, int);
void	column_resizeframe(Frame*, Rectangle);
void	column_settle(Area*);
void	div_draw(Divide*);
void	div_set(Divide*, int x);
void	div_update_all(void);

/* error.c */
#define waserror() setjmp(*pusherror())
void	error(char*, ...);
void	nexterror(void);
void	poperror(void);
jmp_buf*	pusherror(void);

/* event.c */
void	debug_event(XEvent*);
void	print_focus(const char*, Client*, const char*);

/* ewmh.c */
void	ewmh_checkresponsive(Client*);
void	ewmh_destroyclient(Client*);
void	ewmh_framesize(Client*);
void	ewmh_getstrut(Client*);
void	ewmh_getwintype(Client*);
void	ewmh_init(void);
void	ewmh_initclient(Client*);
bool	ewmh_prop(Client*, Atom);
long	ewmh_protocols(Window*);
bool	ewmh_responsive_p(Client*);
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
Vector_rect*	unique_rects(Vector_rect*, Rectangle);
Rectangle	max_rect(Vector_rect*);

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
Rectangle constrain(Rectangle, int);
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

/* key.c */
void	init_lock_keys(void);
void	kpress(XWindow, ulong mod, KeyCode);
void	update_keys(void);

/* main.c */
void	init_screens(void);
void	spawn_command(const char*);

/* message.c */
char*	mask(char**, int*, int*);
char*	message_client(Client*, IxpMsg*);
char*	message_root(void*, IxpMsg*);
char*	message_view(View*, IxpMsg*);
void	msg_debug(char*);
void	msg_eatrunes(IxpMsg*, int (*)(Rune), int);
char*	msg_getword(IxpMsg*, char*);
void	msg_parsecolors(IxpMsg*, CTuple*);
char*	msg_selectarea(Area*, IxpMsg*);
char*	msg_sendclient(View*, IxpMsg*, bool swap);
char*	readctl_client(Client*);
char*	readctl_root(void);
char*	readctl_view(View*);
Area*	strarea(View*, ulong, const char*);
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
Align	snap_rect(const Rectangle *rects, int num, Rectangle *current, Align *mask, int snapw);

/* print.c */
int	Ffmt(Fmt*);

/* root.c */
void	root_init(void);

/* screen.c */
void*	findthing(Rectangle, int, Vector_ptr*, Rectangle(*)(void*), bool);
int	ownerscreen(Rectangle);

/* rule.c */
void	update_rules(Rule**, char*);

/* stack.c */
bool	find(Area* *, Frame**, int, bool, bool);
int	stack_count(Frame*, int*);
Frame*	stack_find(Area*, Frame*, int, bool);
void	stack_info(Frame*, Frame**, Frame**, int*, int*);
void	stack_scale(Frame*, int);


/* view.c */
void	view_arrange(View*);
void	view_attach(View*, Frame*);
View*	view_create(const char*);
void	view_destroy(View*);
void	view_detach(Frame*);
Area*	view_findarea(View*, int, int, bool);
void	view_focus(WMScreen*, View*);
bool	view_fullscreen_p(View*, int);
char*	view_index(View*);
void	view_init(View*, int iscreen);
char**	view_names(void);
uint	view_newcolwidth(View*, int, int);
void	view_restack(View*);
void	view_scale(View*, int, int);
Client*	view_selclient(View*);
void	view_select(const char*);
void	view_update(View*);
void	view_update_all(void);
void	view_update_rect(View*);
Rectangle*	view_rects(View*, uint *num, Frame *ignore);

/* utf.c */
char*	toutf8(const char*);
char*	toutf8n(const char*, size_t);

/* xdnd.c */
void	xdnd_initwindow(Window*);

