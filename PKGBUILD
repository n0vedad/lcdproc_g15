# Maintainer: n0vedad <https://github.com/n0vedad/>
# Based on original LCDproc by various contributors

pkgname=lcdproc-g15
pkgver=0.0.2
pkgrel=1
pkgdesc="LCDproc optimized for G15 keyboards - display driver for LCD devices"
arch=('x86_64')
url="https://lcdproc.org/"
license=('GPL')
depends=('libg15' 'libg15render' 'libusb' 'libftdi-compat' 'ydotool')
makedepends=('gcc' 'make' 'autoconf' 'automake')
optdepends=('clang: for automatic code formatting'
            'npm: for prettier code formatting')
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
    # Set flag to skip interactive formatting setup during package build
    export PKGBUILD_MODE=1
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
