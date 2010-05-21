
pkgname="wmii-hg"
pkgver=2617
pkgrel=1
pkgdesc="The latest hg pull of wmii, a lightweight, dynamic window manager for X11"
url="http://wmii.suckless.org"
license=("MIT")
arch=("i686" "x86_64")
depends=("libx11" "libxinerama" "libxrandr" "libxft")
makedepends=("mercurial")
optdepends=("plan9port: for use of the alternative plan9port wmiirc" \
	"python: for use of the alternative Python wmiirc" \
	"ruby-rumai: for use of the alternative Ruby wmiirc")
provides=("wmii")
conflicts=("wmii")
source=()
options=(!makeflags)

test -d src || mkdir src
ln -snf .. src/wmii
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

    mkdir -p $startdir/pkg/etc/X11/sessions
    install -m644 -D ./LICENSE $startdir/pkg/usr/share/licenses/wmii/LICENSE
}

