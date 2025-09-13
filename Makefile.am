## Process this file with automake to produce Makefile.in

## set automake strictness to 'foreign'
AUTOMAKE_OPTIONS = foreign

SUBDIRS = shared clients server services .

EXTRA_DIST =

# Standard autotools check delegation works through SUBDIRS

clean-local:
	-rm -rf pkg/ src/ node_modules/
	-rm -f *.pkg.tar.* *.log
	-rm -f compile_commands.json
	-rm -f test-driver
	-find . -name "*.orig" -delete 2>/dev/null || true
	-find . -name "*.rej" -delete 2>/dev/null || true

# distclean-local runs LAST now (SUBDIRS = ... .), so we can clean everything
distclean-local:
	-rm -rf autom4te.cache pkg/ src/ node_modules/
	-rm -f *.tar.gz aclocal.m4 compile configure configure~ depcomp missing
	-rm -f install-sh config.guess config.sub config.h.in config.h.in~ ar-lib
	-rm -f *.pkg.tar.* *.log
	-rm -f compile_commands.json
	-rm -f test-driver stamp-h1
	-rm -f Makefile.am.backup configure.ac.backup
	-if [ -f .git/hooks/pre-commit ]; then \
		echo -e "\033[0;34m🧹 Removing git hooks...\033[0m"; \
		rm -f .git/hooks/pre-commit; \
		echo -e "\033[0;32m✓ Git hooks removed\033[0m"; \
	else \
		echo -e "\033[1;33m⚠ No git hooks to remove\033[0m"; \
	fi
	-rm -f tests/Makefile.am tests/Makefile.in tests/Makefile tests/test-driver
	-rm -f tests/test_g15 tests/*.o tests/*.log tests/*.trs tests/test-suite.log
	-rm -rf tests/.deps tests/autom4te.cache
	-find . -name "Makefile.in" -delete
	-find . -name "Makefile" -not -path "./GNUmakefile" -delete
	-find . -name ".deps" -type d -exec rm -rf {} + 2>/dev/null || true
	-rm -rf shared/.deps server/.deps server/commands/.deps server/drivers/.deps clients/.deps clients/lcdproc/.deps clients/lcdexec/.deps services/.deps

## convenience targets

.PHONY: $(SUBDIRS) install-server install-clients format format-check format-before-build help test-full test-ci

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
	@if [ ! -f .git/hooks/pre-commit ]; then \
		echo -e "\033[0;31m❌ Code formatting is not enabled\033[0m"; \
		echo -e "\033[1;33m💡 Enable with: make distclean && make dev\033[0m"; \
		exit 1; \
	fi
	@if ! command -v clang-format >/dev/null 2>&1; then \
		echo -e "\033[0;31m❌ clang-format not available\033[0m"; \
		echo -e "\033[1;33m💡 Install with: sudo pacman -S clang\033[0m"; \
		exit 1; \
	fi
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
		echo -e "\033[0;31m❌ Code formatting is not enabled\033[0m"; \
		echo -e "\033[1;33m💡 Enable with: make distclean && make dev\033[0m"; \
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
		echo -e "\033[1;33m💡 Install with: make distclean && make dev\033[0m"; \
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
	@if [ ! -f .git/hooks/pre-commit ]; then \
		echo -e "\033[0;31m❌ Code formatting is not enabled\033[0m"; \
		echo -e "\033[1;33m💡 Enable with: make distclean && make dev\033[0m"; \
		exit 1; \
	fi
	@echo "Running clang-tidy static analysis..."
	@if command -v clang-tidy >/dev/null 2>&1; then \
		if [ ! -f compile_commands.json ]; then \
			echo "⚠ Generating compile database..."; \
			$(MAKE) clean >/dev/null 2>&1 || true; \
			bear -- $(MAKE) >/dev/null 2>&1 || { \
				echo "❌ Bear not available - install with: sudo pacman -S bear"; \
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
	@if [ ! -f .git/hooks/pre-commit ]; then \
		echo -e "\033[0;31m❌ Code formatting is not enabled\033[0m"; \
		echo -e "\033[1;33m💡 Enable with: make distclean && make dev\033[0m"; \
		exit 1; \
	fi
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


## Build convenience targets


# Setup and build targets with automatic configuration

# Note: setup-standard, setup-dev, and dev targets are handled by GNUmakefile
# The generated Makefile only handles the standard build process


# Standard autotools targets work automatically through SUBDIRS
# test-full, test-ci etc. are handled by GNUmakefile for development-only features

## Help target
help:
	@echo "LCDproc G15 - Available Make Targets"
	@echo "===================================="
	@echo ""
	@echo "🔨 Build Commands (with automatic setup):"
	@echo "  make            - Complete setup + standard build (autogen.sh + configure + build)"
	@echo "  make all        - Same as 'make'"
	@echo "  make dev        - Complete setup + development build (autogen.sh --dev + configure + build)"
	@echo ""
	@echo "🔧 Manual Setup (advanced users):"
	@echo "  make setup-standard - Run autogen.sh + configure (standard)"
	@echo "  make setup-dev      - Run autogen.sh --dev + configure (development)"
	@echo ""
	@echo "🧪 Testing:"
	@echo "  make check                - Run basic tests (fast, ~3s)"
	@echo "  make test-full           - Full test suite (basic + memory leaks)"
	@echo "  make test-ci             - CI/CD suite (full + multi-compiler)"
	@echo ""
	@echo "📦 Installation:"
	@echo "  make install    - Install to system (requires root)"
	@echo ""
	@echo "🎨 Code Quality:"
	@echo "  make format     - Auto-format code (C + Markdown/JSON)"
	@echo "  make format-check - Check if code is properly formatted"
	@echo "  make lint       - Run static code analysis (clang-tidy)"
	@echo "  make lint-fix   - Auto-fix static analysis issues"
	@echo ""
	@echo "🧹 Cleanup:"
	@echo "  make clean      - Remove build artifacts"
	@echo "  make distclean  - Complete cleanup (reset to clean state)"
	@echo ""
	@echo "💡 Quick Start:"
	@echo "  make            - Build project (setup must be done via GNUmakefile first)"
	@echo "  For initial setup: use 'make' or 'make dev' from clean state"
	@echo ""
	@echo "📖 More Info:"
	@echo "  See INSTALL.md for detailed build instructions"
	@echo "  See README.md for project overview and testing"
	@echo "  cd tests && make help - Show all testing options"

## EOF
