
pkgname=(wmii-hg pyxp-hg pygmi-hg)
pkgver=2746
pkgrel=1
pkgdesc="The latest hg pull of wmii, a lightweight, dynamic window manager for X11"
url="http://wmii.suckless.org"
license=(MIT)
arch=(i686 x86_64)
makedepends=(mercurial python "libixp-hg>="$(sed -rn <mk/wmii.mk 's/.*IXP_NEEDAPI=([0-9]+).*/\1/p'))
options=(!strip)
source=()

FORCE_VER=$(hg log -r . --template {rev})

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

package_wmii-hg() {
    depends=(libx11 libxinerama libxrandr)
    optdepends=("plan9port: for use of the alternative plan9port wmiirc" \
            "pygmi: for use of the alternative Python wmiirc" \
            "ruby-rumai: for use of the alternative Ruby wmiirc" \
            "libxft: for anti-aliased font support")
    provides=(wmii)
    conflicts=(wmii)
    _make install PYMODULES= || return 1

    install -m644 -D ./debian/file/wmii.desktop "$pkgdir/etc/X11/sessions/wmii.desktop"
    install -m644 -D ./LICENSE "$pkgdir/usr/share/licenses/$pkgname/LICENSE"
}

package_pyxp-hg() {
    arch=(any)
    depends=(python)
    _make -C alternative_wmiircs/python pyclean pyxp.install
}

package_pygmi-hg() {
    arch=(any)
    depends=(pyxp-hg)
    _make -C alternative_wmiircs/python pyclean pygmi.install
}

