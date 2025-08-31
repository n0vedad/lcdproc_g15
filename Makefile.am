## Process this file with automake to produce Makefile.in

## set automake strictness to 'foreign'
AUTOMAKE_OPTIONS = foreign

SUBDIRS = shared clients server services

EXTRA_DIST =

clean-local:
	-rm -rf pkg/ src/
	-rm -f *.pkg.tar.* *.log

distclean-local:
	-rm -rf autom4te.cache pkg/ src/
	-rm -f *.tar.gz aclocal.m4 compile configure configure~ depcomp missing
	-rm -f install-sh config.guess config.sub config.h.in config.h.in~ ar-lib
	-rm -f *.pkg.tar.* *.log
	-find . -name "Makefile.in" -delete
	-find . -name ".deps" -type d -exec rm -rf {} + 2>/dev/null || true

## convenience targets

.PHONY: $(SUBDIRS) install-server install-clients

$(SUBDIRS):
	$(MAKE) -C $@

clients server: shared

install-server: server
	$(MAKE) -C server install

install-clients: clients
	$(MAKE) -C clients install


## EOF
