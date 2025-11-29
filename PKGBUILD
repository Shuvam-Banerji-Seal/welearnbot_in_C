# Maintainer: Shuvam Banerji Seal
pkgname=welearn-downloader
_pkgname=welearn-downloader
pkgver=1.0.0
pkgrel=1
pkgdesc="A modern C application for automating resource downloads from the WeLearn platform."
arch=('x86_64')
url="https://github.com/shuvam-banerji-seal/welearn-downloader"
license=('custom')
install=${pkgname}.install
depends=('curl' 'zlib' 'openssl')
optdepends=('gtk4: for GUI version')
source=("${_pkgname}-${pkgver}.tar.gz")
sha256sums=('66a72702f4cd212daa49e2ae90ee2511a66efcfa9d22c15dfef82605707c8bc6')

build() {
    cd "${srcdir}/${_pkgname}-${pkgver}"
    make
}

package() {
    cd "${srcdir}/${_pkgname}-${pkgver}"
    install -Dm755 "welearn_cli" "${pkgdir}/usr/bin/welearn_cli"
    if [ -f "welearn_gui" ]; then
        install -Dm755 "welearn_gui" "${pkgdir}/usr/bin/welearn_gui"
    fi
}
