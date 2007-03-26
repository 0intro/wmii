beginfig(1)
%u=0.6cm;
u=0.2cm;
h=3u;
space=u;
linecap:=butt;
linejoin:=beveled;
pickup pencircle scaled 1.1u;
draw (.5u,h)--(.5u,.5u)--(1.5u+space,.5u)--(1.5u+space,h)--(1.5u+space,.5u)--(2.5u+2space,.5u)\
		--(2.5u+2space,h-.5u)--(3.5u+3space,h-.5u);
linejoin:=mitered;
draw (3.5u+3space,h-.5u)--(3.5u+3space,0)--(3.5u+3space,h-.5u)--(4.5u+4space,h-.5u)--(4.5u+4space,0);;

gap=.6u;
draw (3.5u+3space,h+gap)--(3.5u+3space,h+u+gap);
draw (4.5u+4space,h+gap)--(4.5u+4space,h+u+gap);
endfig

end
