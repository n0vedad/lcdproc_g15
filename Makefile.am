## Process this file with automake to produce Makefile.in

## set automake strictness to 'foreign'
AUTOMAKE_OPTIONS = foreign

SUBDIRS = shared clients server services .

EXTRA_DIST =

clean-local:
	-rm -rf pkg/ src/ node_modules/
	-rm -f *.pkg.tar.* *.log

# distclean-local runs LAST now (SUBDIRS = ... .), so we can clean everything
distclean-local:
	-rm -rf autom4te.cache pkg/ src/ node_modules/
	-rm -f *.tar.gz aclocal.m4 compile configure configure~ depcomp missing
	-rm -f install-sh config.guess config.sub config.h.in config.h.in~ ar-lib
	-rm -f *.pkg.tar.* *.log
	-./setup-hooks.sh remove 2>/dev/null || true
	-find . -name "Makefile.in" -delete
	-find . -name ".deps" -type d -exec rm -rf {} + 2>/dev/null || true

## convenience targets

.PHONY: $(SUBDIRS) install-server install-clients format format-check format-before-build

# Format code before any compilation starts
all-recursive: format-before-build

format-before-build:
	@if [ -f .git/hooks/pre-commit ]; then \
		echo "🔍 Checking code formatting before build..."; \
		$(MAKE) format-check; \
		if [ $$? -ne 0 ]; then \
			echo "⚠️  Formatting issues found - fixing automatically..."; \
			$(MAKE) format; \
			echo "✅ Files formatted - proceeding with build"; \
		else \
			echo "✅ All files properly formatted - proceeding with build"; \
		fi; \
	fi

$(SUBDIRS):
	$(MAKE) -C $@

clients server: shared

install-server: server
	$(MAKE) -C server install

install-clients: clients
	$(MAKE) -C clients install

## Code formatting targets
format:
	@echo "🔍 Checking if formatting is needed..."
	@c_issues=$$(find . -name "*.c" -o -name "*.h" | grep -v -E "autom4te.cache|config\.h|\.deps|pkg/" | xargs clang-format --dry-run --Werror 2>&1 | grep -v "^$$" || true); \
	prettier_issues=""; \
	if [ -f package-lock.json ] && command -v npx >/dev/null 2>&1; then \
		prettier_issues=$$(npx prettier --check "**/*.{md,json,sh}" 2>&1 | grep -E "\.md$$|\.json$$|\.sh$$" || true); \
	fi; \
	if [ -z "$$c_issues" ] && [ -z "$$prettier_issues" ]; then \
		echo "✅ Nothing to do! All files are already properly formatted."; \
	else \
		echo "⚠️  Formatting issues found - fixing now..."; \
		if [ -n "$$c_issues" ]; then \
			echo "Formatting C code with clang-format..."; \
			if command -v clang-format >/dev/null 2>&1; then \
				find . -name "*.c" -o -name "*.h" | grep -v -E "autom4te.cache|config\.h|\.deps|pkg/" | xargs clang-format -i; \
			else \
				echo "clang-format not available"; \
			fi; \
		fi; \
		if [ -n "$$prettier_issues" ]; then \
			echo "Formatting Markdown/JSON/Shell with prettier..."; \
			if [ -f package-lock.json ] && command -v npx >/dev/null 2>&1; then \
				npx prettier --write --log-level warn "**/*.{md,json,sh}" || echo "prettier not available"; \
			else \
				echo "prettier not available"; \
			fi; \
		fi; \
		echo "✅ Formatting complete!"; \
	fi

format-check:
	@if [ ! -f .git/hooks/pre-commit ]; then \
		echo "❌ Code formatting is not enabled"; \
		echo "💡 Enable with: ./setup-hooks.sh install"; \
		exit 1; \
	fi
	@echo "Checking C code formatting..."
	@c_issues=$$(find . -name "*.c" -o -name "*.h" | grep -v -E "autom4te.cache|config\.h|\.deps|pkg/" | xargs clang-format --dry-run --Werror 2>&1 | grep -v "^$$" || true); \
	c_exit=0; \
	if [ -z "$$c_issues" ]; then \
		echo "✅ C code properly formatted"; \
	else \
		echo "❌ C formatting issues found in the following files:"; \
		echo "$$c_issues" | grep -E "\.c:|\.h:" | sed 's/^/  /'; \
		echo "💡 Run 'make format' to fix these issues"; \
		c_exit=1; \
	fi; \
	echo "Checking Markdown/JSON/Shell formatting..."; \
	prettier_exit=0; \
	if [ ! -f package-lock.json ]; then \
		echo "❌ Prettier dependencies missing (no package-lock.json found)"; \
		echo "💡 Install with: ./setup-hooks.sh install"; \
		prettier_exit=1; \
	elif ! command -v npx >/dev/null 2>&1; then \
		echo "❌ npx command not available"; \
		echo "💡 Install npm: sudo pacman -S npm"; \
		prettier_exit=1; \
	else \
		prettier_issues=$$(npx prettier --check "**/*.{md,json,sh}" 2>&1 | grep -E "\.md$$|\.json$$|\.sh$$" || true); \
		if [ -z "$$prettier_issues" ]; then \
			echo "✅ Prettier files properly formatted"; \
		else \
			echo "❌ Prettier formatting issues found in the following files:"; \
			echo "$$prettier_issues" | sed 's/^/  /'; \
			echo "💡 Run 'make format' to fix these issues"; \
			prettier_exit=1; \
		fi; \
	fi; \
	if [ "$$c_exit" -ne 0 ] || [ "$$prettier_exit" -ne 0 ]; then \
		exit 1; \
	fi

## Static analysis targets
lint:
	@echo "Running clang-tidy static analysis..."
	@if command -v clang-tidy >/dev/null 2>&1; then \
		if [ ! -f compile_commands.json ]; then \
			echo "⚠ Generating compile database..."; \
			$(MAKE) clean >/dev/null 2>&1 || true; \
			bear -- $(MAKE) >/dev/null 2>&1 || { \
				echo "❌ bear not available - install with: sudo pacman -S bear"; \
				echo "💡 Running clang-tidy without compile database (limited analysis)"; \
				find ./shared -name "*.c" | xargs -n5 clang-tidy --quiet --format-style=file || echo "clang-tidy found issues"; \
			}; \
		fi; \
		if [ -f compile_commands.json ]; then \
			echo "⚠ Analyzing files in small batches to avoid memory issues..."; \
			find . -name "*.c" | grep -v -E "autom4te.cache|config\.h|\.deps|pkg/" | \
			xargs -n5 clang-tidy --quiet --format-style=file || echo "clang-tidy found issues"; \
		fi; \
	else \
		echo "❌ clang-tidy not available"; \
		echo "💡 Install with: sudo pacman -S clang"; \
		exit 1; \
	fi

lint-fix:
	@echo "Auto-fixing clang-tidy issues..."
	@if command -v clang-tidy >/dev/null 2>&1; then \
		find . -name "*.c" | grep -v -E "autom4te.cache|config\.h|\.deps|pkg/" | \
		xargs -n5 clang-tidy --fix --quiet --format-style=file; \
		echo "✅ clang-tidy auto-fixes applied"; \
	else \
		echo "❌ clang-tidy not available"; \
		echo "💡 Install with: sudo pacman -S clang"; \
		exit 1; \
	fi

lint-check:
	@echo "Checking code with clang-tidy (no fixes)..."
	@if command -v clang-tidy >/dev/null 2>&1; then \
		issues=$$(find . -name "*.c" | grep -v -E "autom4te.cache|config\.h|\.deps|pkg/" | \
			xargs -n5 clang-tidy --quiet 2>&1 | grep -E "warning:|error:" || true); \
		if [ -z "$$issues" ]; then \
			echo "✅ No clang-tidy issues found"; \
		else \
			echo "❌ clang-tidy found issues:"; \
			echo "$$issues"; \
			echo "💡 Run 'make lint-fix' to auto-fix some issues"; \
		fi; \
	else \
		echo "❌ clang-tidy not available"; \
		echo "💡 Install with: sudo pacman -S clang"; \
		exit 1; \
	fi


## EOF
