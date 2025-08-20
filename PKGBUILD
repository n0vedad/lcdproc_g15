# Maintainer: n0vedad <https://github.com/n0vedad/>

pkgname=lcdproc-g15
pkgver=0.0.1
pkgrel=1
pkgdesc="LCDproc optimized for G15 keyboards - display driver for LCD devices"
arch=('x86_64')
url="https://lcdproc.org/"
license=('GPL')
depends=('libg15' 'libg15render' 'libusb' 'libftdi-compat')
makedepends=('gcc' 'make' 'autoconf' 'automake')
install=lcdproc-g15.install
backup=(
    'etc/LCDd.conf'
    'etc/lcdexec.conf'
    'etc/lcdproc.conf'
)
source=()
sha512sums=()

prepare() {
    cd "$startdir"
    sh ./autogen.sh
}

build() {
    cd "$startdir"
    ./configure \
        --prefix=/usr \
        --sbindir=/usr/bin \
        --sysconfdir=/etc \
        --enable-libusb \
        --enable-lcdproc-menus \
        --enable-stat-smbfs \
        --enable-drivers=g15,linux_input,debug
    make
}

package() {
    cd "$startdir"
    make DESTDIR="$pkgdir" install
}
