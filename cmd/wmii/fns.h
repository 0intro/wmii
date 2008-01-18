/* © 2004-2006 Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#ifdef VARARGCK
# pragma varargck	argpos	write_event	1
#
# pragma varargck	type	"C"	Client*	
# pragma varargck	type	"W"	Window*	
# pragma varargck	type	"P"	Point
# pragma varargck	type	"R"	Rectangle
# pragma varargck	type	"r"	void
#endif

/* area.c */
uint	area_idx(Area*);
char*	area_name(Area*);
Client*	area_selclient(Area*);
void	attach_to_area(Area*, Frame*);
Area*	create_area(View*, Area *pos, uint w);
void	destroy_area(Area*);
void	detach_from_area(Frame*);
void	focus_area(Area*);
void	send_to_area(Area*, Frame*);

/* bar.c */
Bar*	bar_of_name(Bar*, const char*);
Bar*	create_bar(Bar**, char*);
void	destroy_bar(Bar**, Bar*);
void	draw_bar(WMScreen*);
void	initbar(WMScreen*);
void	resize_bar(WMScreen*);

/* client.c */
int	Cfmt(Fmt *f);
void	apply_rules(Client*);
void	apply_tags(Client*, const char*);
char*	clientname(Client*);
void	configure_client(Client*);
Client*	create_client(XWindow, XWindowAttributes*);
void	destroy_client(Client*);
void	focus(Client*, Bool restack);
void	focus_client(Client*);
void	focus_frame(Frame*, Bool restack);
void	fullscreen(Client*, Bool);
void	kill_client(Client*);
void	manage_client(Client*);
void	map_client(Client*);
int	map_frame(Client*);
void	move_client(Client*, char*);
void	prop_client(Client*, Atom);
void	reparent_client(Client*, Window*, Point);
void	resize_client(Client*, Rectangle);
Client*	selclient(void);
void	set_cursor(Client*, Cursor);
void	set_urgent(Client *, Bool urgent, Bool write);
void	size_client(Client*, char*);
void	unmap_client(Client*, int state);
int	unmap_frame(Client*);
void	update_class(Client*);
Client*	win2client(XWindow);
Rectangle	gravclient(Client*, Rectangle);

/* column.c */
void	arrange_column(Area*, Bool dirty);
char*	colmode2str(int);
void	draw_div(Divide*);
Area*	new_column(View*, Area *, uint);
void	resize_colframe(Frame*, Rectangle*);
void	resize_column(Area*, int);
void	setdiv(Divide*, int x);
int	str2colmode(const char*);
void	update_divs(void);

/* event.c */
void	check_x_event(IxpConn*);
void	dispatch_event(XEvent*);
uint	flushevents(long, Bool dispatch);
void	print_focus(Client*, char*);

/* frame.c */
Frame*	create_frame(Client*, View*);
void	draw_frame(Frame*);
void	draw_frames(void);
int	frame_delta_h(void);
uint	frame_idx(Frame*);
Bool	frame_to_top(Frame*);
int	ingrabbox(Frame*, int x, int y);
void	insert_frame(Frame *pos, Frame*);
void	remove_frame(Frame*);
void	resize_frame(Frame*, Rectangle);
void	set_frame_cursor(Frame*, Point);
void	swap_frames(Frame*, Frame*);
void	update_frame_widget_colors(Frame*);
Rectangle constrain(Rectangle);
Rectangle client2frame(Frame*, Rectangle);
Rectangle frame2client(Frame*, Rectangle);
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
void	write_event(char*, ...);

/* geom.c */
Cursor	cursor_of_quad(Align);
Align	get_sticky(Rectangle src, Rectangle dst);
Bool	ptinrect(Point, Rectangle);
Align	quadrant(Rectangle, Point);

/* key.c */
void	init_lock_keys(void);
void	kpress(XWindow, ulong mod, KeyCode);
ulong	str2modmask(char*);
void	update_keys(void);

/* map.c */
MapEnt*	hashget(Map*, char*, int create);
void*	hashrm(Map*, char*);
MapEnt*	mapget(Map*, ulong, int create);
void*	maprm(Map*, ulong);

/* message.c */
int	getlong(char*, long*);
int	getulong(char*, ulong*);
char*	getword(IxpMsg*);
char*	message_client(Client*, IxpMsg*);
char*	message_root(void*, IxpMsg*);
char*	message_view(View*, IxpMsg*);
char*	parse_colors(IxpMsg*, CTuple*);
char*	read_root_ctl(void);
char*	select_area(Area*, IxpMsg*);
char*	send_client(View*, IxpMsg*, Bool swap);
Area*	strarea(View*, char*);

/* mouse.c */
void	do_mouse_resize(Client*, Bool opaque, Align);
void	grab_button(XWindow, uint button, ulong mod);
void	grab_mouse(XWindow, ulong mod, ulong button);
void	mouse_resizecol(Divide*);
Align	snap_rect(Rectangle *rects, int num, Rectangle *current, Align *mask, int snapw);
void	ungrab_mouse(XWindow, ulong mod, uint button);

/* rule.c */
void	trim(char *str, const char *chars);
void	update_rules(Rule**, const char*);

/* view.c */
View	*get_view(const char*);
void	arrange_view(View*);
void	attach_to_view(View*, Frame*);
View*	create_view(const char*);
void	destroy_view(View*);
void	focus_view(WMScreen*, View*);
char*	message_view(View *v, IxpMsg *m);
uint	newcolw(View*, int i);
void	restack_view(View*);
void	scale_view(View*, int w);
void	select_view(const char*);
void	update_client_views(Client*, char**);
void	update_views(void);
Frame*	view_clientframe(View *v, Client *c);
uchar*	view_ctl(View *v);
uchar*	view_index(View*);
View*	view_of_id(ushort);
Client*	view_selclient(View*);
Rectangle*	rects_of_view(View*, uint *num, Frame *ignore);

/* wm.c */
int	win_proto(Window);

/* x11.c */
Window	*createwindow(Window *parent, Rectangle, int depth, uint class, WinAttr*, int valuemask);
char	*gettextproperty(Window*, char*);
Point	addpt(Point, Point);
Image*	allocimage(int w, int h, int depth);
void	border(Image *dst, Rectangle, int w, ulong col);
void	changeprop_char(Window *w, char *prop, char *type, char data[], int len);
void	changeprop_long(Window *w, char *prop, char *type, long data[], int len);
void	changeprop_short(Window *w, char *prop, char *type, short data[], int len);
void	changeproperty(Window*, char *prop, char *type, int width, uchar *data, int n);
void	copyimage(Image *dst, Rectangle, Image *src, Point p);
void	destroywindow(Window*);
Point	divpt(Point, Point);
void	drawline(Image *dst, Point p1, Point p2, int cap, int w, ulong col);
void	drawpoly(Image *dst, Point *pt, int np, int cap, int w, ulong col);
void	drawstring(Image *dst, Font *font, Rectangle, Align align, char *text, ulong col);
int	eqpt(Point, Point);
int	eqrect(Rectangle, Rectangle);
void	fill(Image *dst, Rectangle, ulong col);
void	fillpoly(Image *dst, Point *pt, int np, ulong col);
Window*	findwin(XWindow);
void	freefont(Font*);
void	freeimage(Image *);
void	freestringlist(char**);
ulong	getproperty(Window *w, char *prop, char *type, Atom *actual, ulong offset, uchar **ret, ulong length);
int	gettextlistproperty(Window *w, char *name, char **ret[]);
int	grabpointer(Window*, Window *confine, Cursor, int mask);
void	initdisplay(void);
uint	labelh(Font*);
Bool	loadcolor(CTuple*, char*);
Font*	loadfont(char*);
void	lowerwin(Window*);
int	mapwin(Window*);
void	movewin(Window*, Point);
Point	mulpt(Point p, Point q);
Bool	namedcolor(char *name, ulong*);
Point	querypointer(Window*);
void	raisewin(Window*);
void	reparentwindow(Window*, Window*, Point);
void	reshapewin(Window*, Rectangle);
void	setfocus(Window*, int mode);
void	sethints(Window*);
void	setshapemask(Window *dst, Image *src, Point);
void	setwinattr(Window*, WinAttr*, int valmask);
Point	subpt(Point, Point);
uint	textwidth(Font*, char*);
uint	textwidth_l(Font*, char*, uint len);
Point	translate(Window*, Window*, Point);
void	ungrabpointer(void);
int	unmapwin(Window*);
void	warppointer(Point);
uint	winprotocols(Window*);
Atom	xatom(char*);
Handlers*	sethandler(Window*, Handlers*);
XRectangle	XRect(Rectangle);
Rectangle	gravitate(Rectangle dst, Rectangle src, Point grav);
Rectangle	insetrect(Rectangle, int);
Rectangle	rectaddpt(Rectangle, Point);
Rectangle	rectsubpt(Rectangle, Point);
Rectangle	sizehint(WinHints*, Rectangle);

/* utf.c */
char*	toutf8(char*);
char*	toutf8n(char*, size_t);
