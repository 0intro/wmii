
pkgname=wmii-hg
pkgver=2739
pkgrel=1
pkgdesc="The latest hg pull of wmii, a lightweight, dynamic window manager for X11"
url="http://wmii.suckless.org"
license=(MIT)
arch=(i686 x86_64)
depends=(libx11 libxinerama libxrandr)
makedepends=(mercurial libixp-hg)
optdepends=("plan9port: for use of the alternative plan9port wmiirc" \
	"python: for use of the alternative Python wmiirc" \
	"ruby-rumai: for use of the alternative Ruby wmiirc" \
	"libxft: for anti-aliased font support")
provides=(wmii)
conflicts=(wmii)
source=()

FORCE_VER=$(hg log -r . --template {rev})

build()
{
    cd $startdir
    flags=(PREFIX=/usr \
           ETC=/etc    \
           DESTDIR="$pkgdir")

    make "${flags[@]}" || return 1
    make "${flags[@]}" install || return 1

    install -m644 -D ./debian/file/wmii.desktop "$pkgdir/etc/X11/sessions/wmii.desktop"
    install -m644 -D ./LICENSE "$pkgdir/usr/share/licenses/$pkgname/LICENSE"
}

