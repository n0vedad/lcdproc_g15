## Process this file with automake to produce Makefile.in

## set automake strictness to 'foreign'
AUTOMAKE_OPTIONS = foreign

SUBDIRS = shared clients server services

EXTRA_DIST =

clean-local:
	-rm -rf pkg/ src/ node_modules/
	-rm -f *.pkg.tar.* *.log

distclean-local:
	-rm -rf autom4te.cache pkg/ src/ node_modules/
	-rm -f *.tar.gz aclocal.m4 compile configure configure~ depcomp missing
	-rm -f install-sh config.guess config.sub config.h.in config.h.in~ ar-lib
	-rm -f *.pkg.tar.* *.log
	-./setup-hooks.sh remove 2>/dev/null || true
	-find . -name "Makefile.in" -delete
	-find . -name ".deps" -type d -exec rm -rf {} + 2>/dev/null || true

## convenience targets

.PHONY: $(SUBDIRS) install-server install-clients format format-check

# Auto-format before build
all-local: format
	@echo "Code formatted before build"

$(SUBDIRS):
	$(MAKE) -C $@

clients server: shared

install-server: server
	$(MAKE) -C server install

install-clients: clients
	$(MAKE) -C clients install

## Code formatting targets
format:
	@echo "Formatting C code with clang-format..."
	@find . -name "*.c" -o -name "*.h" | grep -v -E "autom4te.cache|config\.h|\.deps" | xargs clang-format -i
	@echo "Formatting Markdown/JSON/Shell with prettier..."
	@npx prettier --write "**/*.{md,json,sh}" || echo "prettier not available"

format-check:
	@echo "Checking C code formatting..."
	@if find . -name "*.c" -o -name "*.h" | grep -v -E "autom4te.cache|config\.h|\.deps" | xargs clang-format --dry-run --Werror >/dev/null 2>&1; then \
		echo "✅ C code properly formatted"; \
	else \
		echo "❌ C formatting issues found - run 'make format' to fix"; \
	fi
	@echo "Checking Markdown/JSON/Shell formatting..."
	@if npx prettier --check "**/*.{md,json,sh}" >/dev/null 2>&1; then \
		echo "✅ Prettier files properly formatted"; \
	else \
		echo "❌ Some files need formatting - run 'make format'"; \
	fi


## EOF
