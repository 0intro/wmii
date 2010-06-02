
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
void	update_filter(bool);
void	update_input(void);

/* menu.c */
void	menu_draw(void);
void	menu_init(void);
void	menu_show(void);

/* keys.c */
void	parse_keys(char*);
char**	find_key(char*, long);
int	getsym(char*);

