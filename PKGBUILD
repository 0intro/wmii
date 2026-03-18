
pkgname=(wmii python-pyxp python-pygmi)
pkgver=2755
pkgrel=1
pkgdesc="A lightweight, dynamic window manager for X11"
url="https://github.com/0intro/wmii"
license=(MIT)
arch=(i686 x86_64)
makedepends=(git python "libixp>="$(sed -rn <mk/wmii.mk 's/.*IXP_NEEDAPI=([0-9]+).*/\1/p'))
options=(!strip)
source=()

FORCE_VER=$(git rev-parse --short HEAD)

_make() {
    cd $startdir
    make PREFIX=/usr		\
	 PYPREFIX=--prefix=/usr	\
         ETC=/etc		\
         DESTDIR="$pkgdir"	\
         "$@"
}

build() {
    _make "${flags[@]}" || return 1
}

package_wmii() {
    depends=(libx11 libxinerama libxrandr)
    optdepends=("plan9port: for use of the alternative plan9port wmiirc" \
            "${pkgname[2]}: for use of the alternative Python wmiirc" \
            "ruby-rumai: for use of the alternative Ruby wmiirc" \
            "libxft: for anti-aliased font support")
    provides=(wmii)
    conflicts=(wmii)
    _make install PYMODULES= || return 1

    install -m644 -D ./debian/file/wmii.desktop "$pkgdir/etc/X11/sessions/wmii.desktop"
    install -m644 -D ./LICENSE "$pkgdir/usr/share/licenses/$pkgname/LICENSE"
}

package_python-pyxp() {
    pkgdesc="Python 9P client library"
    arch=(any)
    depends=(python)
    _make -C alternative_wmiircs/python pyclean pyxp.install
}

package_python-pygmi() {
    pkgdesc="Python wmii interaction library"
    arch=(any)
    depends=(python-pyxp)
    _make -C alternative_wmiircs/python pyclean pygmi.install
}
