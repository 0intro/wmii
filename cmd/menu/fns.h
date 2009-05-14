
void	caret_delete(int, int);
char*	caret_find(int, int);
void	caret_insert(char*, bool);
void	caret_move(int, int);
void	check_x_event(IxpConn*);
void	debug(int, const char*, ...);
void	dispatch_event(XEvent*);
Item*	filter_list(Item*, char*);
uint	flushenterevents(void);
uint	flushevents(long, bool);
void	history_dump(const char*, int);
char*	history_search(int, char*, int);
char*	histtext(Item*);
void	init_screens(int);
void	menu_init(void);
void	menu_show(void);
void	xtime_kludge(void);
void	update_filter(void);

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

