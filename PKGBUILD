
pkgname="wmii-hg"
pkgver=2647
pkgrel=1
pkgdesc="The latest hg pull of wmii, a lightweight, dynamic window manager for X11"
url="http://wmii.suckless.org"
license=("MIT")
arch=("i686" "x86_64")
depends=("libx11" "libxinerama" "libxrandr")
makedepends=("mercurial")
optdepends=("plan9port: for use of the alternative plan9port wmiirc" \
	"python: for use of the alternative Python wmiirc" \
	"ruby-rumai: for use of the alternative Ruby wmiirc" \
	"libxft: for anti-aliased font support")
provides=("wmii")
conflicts=("wmii")
source=()

FORCE_VER=$(hg tip --template {rev})
#_hgroot="http://hg.suckless.org/"
#_hgrepo="wmii"

build()
{
    cd $startdir
    flags=(PREFIX=/usr \
           ETC=/etc    \
           DESTDIR="$startdir/pkg")

    make "${flags[@]}" || return 1
    make "${flags[@]}" install || return 1

    install -m644 -D ./debian/file/wmii.desktop $startdir/pkg/etc/X11/sessions/wmii.desktop
    install -m644 -D ./LICENSE $startdir/pkg/usr/share/licenses/wmii/LICENSE
}

