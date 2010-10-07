#pragma once

#include <stuff/base.h>

typedef struct Point Point;
typedef struct Rectangle Rectangle;

struct Point {
	int x, y;
};

struct Rectangle {
	Point min, max;
};

enum Align {
	North = 0x01,
	East  = 0x02,
	South = 0x04,
	West  = 0x08,
	NEast = North | East,
	NWest = North | West,
	SEast = South | East,
	SWest = South | West,
	Center = NEast | SWest,
};

typedef enum Align Align;

#define Dx(r) ((r).max.x - (r).min.x)
#define Dy(r) ((r).max.y - (r).min.y)
#define Pt(x, y) ((Point){(x), (y)})
#define Rpt(p, q) ((Rectangle){(p), (q)})
#define Rect(x0, y0, x1, y1) Rpt(Pt(x0, y0), Pt(x1, y1))

Point		addpt(Point, Point);
Point		divpt(Point, Point);
int		eqpt(Point, Point);
int		eqrect(Rectangle, Rectangle);
Rectangle	gravitate(Rectangle dst, Rectangle src, Point grav);
Rectangle	insetrect(Rectangle, int);
Point		mulpt(Point p, Point q);
Rectangle	rectaddpt(Rectangle, Point);
Rectangle	rectsetorigin(Rectangle, Point);
Rectangle	rectsubpt(Rectangle, Point);
Point		subpt(Point, Point);

Align		get_sticky(Rectangle src, Rectangle dst);
Align		quadrant(Rectangle, Point);
bool		rect_contains_p(Rectangle, Rectangle);
bool		rect_haspoint_p(Rectangle, Point);
bool		rect_intersect_p(Rectangle, Rectangle);
Rectangle	rect_intersection(Rectangle, Rectangle);

