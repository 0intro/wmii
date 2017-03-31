Abstract
--------
wmii is a dynamic window manager for X11.  It supports classic and
tiled window management with extended keyboard, mouse, and 9P-based[1]
remote control.  It consists of the wmii(1) window manager and the
wmiir(1) the remote access utility.


Requirements
------------
In order to build wmii you need the Xlib header files and libixp.
xmessage is used by the default scripts.  Libixp, if not provided, can
be obtained from http://libs.suckless.org/.  On debian, you should be
able to obtain all dependencies by running `make deb-dep`.  Python is
recommended for more advanced configurations.


Installation
------------
First, edit config.mk to match your local setup.

To build, simply run:
	make

To install, run the following, as root if necessary:
	make install

On debian, you should only have to run `make deb` to create a debian
package.  No further configuration should be necessary.


Running wmii
------------
Add the following line to your .xinitrc to start wmii using startx:

    until wmii; do :; done

In order to connect wmii to a specific display, make sure that the
DISPLAY environment variable is set correctly.  For example:

    DISPLAY=:1 wmii

This will start wmii on display :1.


Configuration
-------------
The configuration of wmii is done by customizing the rc script wmiirc,
which remotely controls the window manager and handles various events.
The main wmiirc script lives in @GLOBALCONF@ while wmiirc_local goes
in @LOCALCONF@.

More advanced versions of wmiirc are provided in python and ruby.
For more information on them, see alternative_wmiircs/README.

Credits
-------
The following people have contributed especially to wmii in various
ways:

- Christoph Wegscheider <christoph dot wegscheider at wegi dot net>
- Georg Neis <gn at suckless dot org>
- Uwe Zeisberger <zeisberg at informatik dot uni-freiburg dot de>
- Uriel <uriel99 at gmail dot com>
- Scot Doyle <scot at scotdoyle dot com>
- Sebastian Hartmann <seb dot wmi at gmx dot de>
- Bernhard Leiner <bleiner at gmail dot com>
- Jonas Domeij <jonas dot domeij at gmail dot com>
- Vincent <10 dot 50 at free dot fr>
- Oliver Kopp <olly at flupp dot de>
- Sebastian Roth <sebastian dot roth at gmail dot com>
- Nico Golde <nico at ngolde dot de>
- Steve Hoffman <steveh at g2switchworks dot com>
- Christof Musik <christof at senfdax dot de>
- Steffen Liebergeld <perl at gmx dot org>
- Tobias Walkowiak <wal at ivu dot de>
- Sander van Dijk <a dot h dot vandijk at gmail dot com>
- Salvador Peiro <saoret dot one at gmail dot com>
- Anthony Martin <ality at pbrane dot org>
- Icarus Sparry <wmii at icarus dot freeuk dot com>
- Norman Golisz <norman dot golisz at arcor dot de>
- Stefano K. Lee <wizinblack at gmail dot com >
- Stefan Tibus <sjti at gmx dot net>
- Neptun <neptun at gmail dot com>
- Daniel Wäber <_wabu at web dot de>


References
----------
[1] http://9p.cat-v.org
[2] http://plan9.us

