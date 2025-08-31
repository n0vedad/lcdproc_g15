
## Process this file with automake to produce Makefile.in

## We use bin_ to generate the shared libraries, because automake only
## supports libtool shared libraries. But we don't want to use libtool...
##
## configure replaces all occurrences of so to the appropriate extension
## (without dot!)

## Fill in loadable module extension
EXEEXT = @SO@

## How to compile the files for the modules:
AM_CFLAGS = @CCSHARED@

## And how to build them
AM_LDFLAGS = @LDSHARED@

## Forget the libs that the server core requires
#LIBS =

## Keep the lists sorted!

lcdexecbindir = $(pkglibdir)
lcdexecbin_PROGRAMS = @DRIVERS@
EXTRA_PROGRAMS = g15 linux_input debug
noinst_LIBRARIES = libLCD.a libbignum.a

g15_CFLAGS =         @LIBUSB_CFLAGS@ @FT2_CFLAGS@ $(AM_CFLAGS)

g15_LDADD =          @LIBG15@

libLCD_a_SOURCES =   lcd_lib.h lcd_lib.c
libbignum_a_SOURCES = adv_bignum.h  adv_bignum.c

debug_SOURCES =      lcd.h debug.c debug.h
g15_SOURCES =        lcd.h lcd_lib.h g15.h g15-num.c g15.c hidraw_lib.c hidraw_lib.h
linux_input_SOURCES = lcd.h linux_input.h linux_input.c


AM_CPPFLAGS = -I$(top_srcdir)

## EOF
