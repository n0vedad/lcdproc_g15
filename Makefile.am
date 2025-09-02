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

# Auto-format before build (only if pre-commit hook is installed)
all-local:
	@if [ -f .git/hooks/pre-commit ]; then \
		$(MAKE) format && echo "Code formatted before build"; \
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
	@echo "Formatting C code with clang-format..."
	@if command -v clang-format >/dev/null 2>&1; then \
		find . -name "*.c" -o -name "*.h" | grep -v -E "autom4te.cache|config\.h|\.deps|pkg/" | xargs clang-format -i; \
	else \
		echo "clang-format not available"; \
	fi
	@echo "Formatting Markdown/JSON/Shell with prettier..."
	@if [ -f package-lock.json ] && command -v npx >/dev/null 2>&1; then \
		npx prettier --write "**/*.{md,json,sh}" 2>/dev/null || echo "prettier not available"; \
	else \
		echo "prettier not available"; \
	fi

format-check:
	@if [ ! -f .git/hooks/pre-commit ]; then \
		echo "❌ Code formatting is not enabled"; \
		echo "💡 Enable with: ./setup-hooks.sh install"; \
		exit 1; \
	fi
	@echo "Checking C code formatting..."
	@c_issues=$$(find . -name "*.c" -o -name "*.h" | grep -v -E "autom4te.cache|config\.h|\.deps" | xargs clang-format --dry-run --Werror 2>&1 | grep -v "^$$" || true); \
	if [ -z "$$c_issues" ]; then \
		echo "✅ C code properly formatted"; \
	else \
		echo "❌ C formatting issues found in the following files:"; \
		echo "$$c_issues" | grep -E "\.c:|\.h:" | sed 's/^/  /'; \
		echo "💡 Run 'make format' to fix these issues"; \
	fi
	@echo "Checking Markdown/JSON/Shell formatting..."
	@if [ ! -f package-lock.json ]; then \
		echo "❌ Prettier dependencies missing (no package-lock.json found)"; \
		echo "💡 Install with: ./setup-hooks.sh install"; \
	elif ! command -v npx >/dev/null 2>&1; then \
		echo "❌ npx command not available"; \
		echo "💡 Install npm: sudo pacman -S npm"; \
	else \
		prettier_issues=$$(npx prettier --check "**/*.{md,json,sh}" 2>&1 | grep -E "\.md$$|\.json$$|\.sh$$" || true); \
		if [ -z "$$prettier_issues" ]; then \
			echo "✅ Prettier files properly formatted"; \
		else \
			echo "❌ Prettier formatting issues found in the following files:"; \
			echo "$$prettier_issues" | sed 's/^/  /'; \
			echo "💡 Run 'make format' to fix these issues"; \
		fi; \
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
