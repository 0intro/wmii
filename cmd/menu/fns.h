
void	check_x_event(IxpConn*);
void	dispatch_event(XEvent*);
uint	flushenterevents(void);
uint	flushevents(long, bool);
void	xtime_kludge(void);

/* caret.c */
void	caret_delete(int, int);
char*	caret_find(int, int);
void	caret_insert(char*, bool);
void	caret_move(int, int);
void	caret_set(int, int);

/* history.c */
void	history_dump(const char*, int);
char*	history_search(int, char*, int);

/* main.c */
void	debug(int, const char*, ...);
Item*	filter_list(Item*, char*);
void	init_screens(int);
void	update_filter(bool);

/* menu.c */
void	menu_draw(void);
void	menu_init(void);
void	menu_show(void);

/* keys.c */
void	parse_keys(char*);
char**	find_key(char*, long);
int	getsym(char*);

/* geom.c */
Align	get_sticky(Rectangle src, Rectangle dst);
Cursor	quad_cursor(Align);
Align	quadrant(Rectangle, Point);
bool	rect_contains_p(Rectangle, Rectangle);
bool	rect_haspoint_p(Point, Rectangle);
bool	rect_intersect_p(Rectangle, Rectangle);
Rectangle	rect_intersection(Rectangle, Rectangle);

/* xext.c */
void	randr_event(XEvent*);
bool	render_argb_p(Visual*);
void	xext_event(XEvent*);
void	xext_init(void);
Rectangle*	xinerama_screens(int*);

