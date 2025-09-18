# Maintainer: n0vedad <https://github.com/n0vedad/>
# Based on original LCDproc by various contributors

pkgname=lcdproc-g15
pkgver=0.0.4
pkgrel=1
pkgdesc="LCDproc for Logitech G15/G510/G510s keyboards - optimized for G510s with RGB backlight and macro support"
arch=('x86_64')
url="https://lcdproc.org/"
license=('GPL')
depends=('libg15' 'libg15render' 'libusb' 'libftdi-compat' 'ydotool')
makedepends=('clang' 'make' 'autoconf' 'automake')
optdepends=('gcc: alternative supported compiler (multi-compiler-test)'
            'npm: for prettier code formatting'
            'bear: for generating compile database (static analysis)'
            'valgrind: for memory leak detection in tests'
            'gcovr: for code coverage analysis and reporting'
            'act: for testing GitHub workflows locally'
            'python-evdev: for reading raw input events from /dev/input/event*')
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
}

build() {
    cd "$startdir"
    make
}

package() {
    cd "$startdir"
    make DESTDIR="$pkgdir" install
}
