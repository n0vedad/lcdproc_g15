# Bootstrap GNUmakefile for LCDproc G15
# This file handles complete setup including autotools and git hooks
# GNUmakefile takes precedence over Makefile and won't be ignored by git
#
# Copyright (C) 2025 n0vedad <https://github.com/n0vedad/>

# Colors for output
RED := \033[0;31m
GREEN := \033[0;32m
YELLOW := \033[1;33m
BLUE := \033[0;34m
NC := \033[0m

# Autotools variables
AUTOCONF := autoconf
AUTOHEADER := autoheader
AUTOMAKE := automake
ACLOCAL := aclocal

# Git hooks variables
HOOK_SOURCE := .git/hooks/pre-commit
HOOK_TEMPLATE := hooks-pre-commit.template

# Check if we have a configured project (Makefile exists from autotools)
# Use runtime checks instead of parse-time wildcard evaluation

.PHONY: all dev distclean clean help

# Dependency management - manual installation only

# Default target - check at runtime if Makefile exists
all:
	@if [ -f Makefile ] && [ -f config.h ] && ! grep -q "^#define DEBUG" config.h 2>/dev/null; then \
		printf "$(RED)❌ Standard build already configured$(NC)\n"; \
		printf "$(YELLOW)💡 Run 'make distclean' first to rebuild or switch to development mode$(NC)\n"; \
		exit 1; \
	elif [ -f Makefile ] && [ -f config.h ] && grep -q "^#define DEBUG" config.h 2>/dev/null; then \
		printf "$(RED)❌ Development build already configured$(NC)\n"; \
		printf "$(YELLOW)💡 Run 'make distclean' first to switch to standard mode$(NC)\n"; \
		exit 1; \
	elif [ -f Makefile ]; then \
		echo "Using existing Makefile..."; \
		if $(MAKE) -f Makefile all; then \
			printf "$(GREEN)🎉 Build successful!$(NC)\n"; \
		else \
			printf "$(RED)❌ Build failed!$(NC)\n"; \
			exit 1; \
		fi; \
	else \
		echo "No Makefile found - running bootstrap..."; \
		if $(MAKE) -f $(lastword $(MAKEFILE_LIST)) bootstrap-all; then \
			printf "$(GREEN)🎉 Bootstrap and build successful!$(NC)\n"; \
		else \
			printf "$(RED)❌ Bootstrap or build failed!$(NC)\n"; \
			exit 1; \
		fi; \
	fi

# Development build target
dev:
	@if [ -f Makefile ] && [ -f config.h ] && ! grep -q "^#define DEBUG" config.h 2>/dev/null; then \
		printf "$(RED)❌ Standard build already configured$(NC)\n"; \
		printf "$(YELLOW)💡 Run 'make distclean' first to switch to development mode$(NC)\n"; \
		exit 1; \
	elif [ -f Makefile ] && [ -f config.h ] && grep -q "^#define DEBUG" config.h 2>/dev/null; then \
		printf "$(RED)❌ Development build already configured$(NC)\n"; \
		printf "$(YELLOW)💡 Run 'make distclean' first to rebuild or switch to standard mode$(NC)\n"; \
		exit 1; \
	elif [ -f Makefile ]; then \
		echo "Using existing Makefile..."; \
		if $(MAKE) -f Makefile all; then \
			printf "$(GREEN)🎉 Development build successful!$(NC)\n"; \
		else \
			printf "$(RED)❌ Development build failed!$(NC)\n"; \
			exit 1; \
		fi; \
	else \
		echo "No development build found - running bootstrap..."; \
		if $(MAKE) -f $(lastword $(MAKEFILE_LIST)) bootstrap-dev; then \
			printf "$(GREEN)🎉 Development bootstrap and build successful!$(NC)\n"; \
		else \
			printf "$(RED)❌ Development bootstrap or build failed!$(NC)\n"; \
			exit 1; \
		fi; \
	fi

# Distclean target - always available
distclean:
	@cleanup_needed=0; \
	if [ -f tests/Makefile.am ] || [ -f .git/hooks/pre-commit ] || [ -d node_modules ] || [ -f config.h ] || [ -f Makefile ]; then \
		cleanup_needed=1; \
	fi; \
	if [ $$cleanup_needed -eq 0 ]; then \
		printf "$(GREEN)✓ Project already clean$(NC)\n"; \
		exit 0; \
	fi; \
	if [ -f Makefile ]; then \
		printf "$(BLUE)=== Running Clean First ===$(NC)\n"; \
		$(MAKE) clean >/dev/null 2>&1; \
		printf "$(GREEN)✓ Build artifacts cleaned$(NC)\n"; \
	fi; \
	if [ -f tests/Makefile.am ] && [ ! -f tests/Makefile.am.dev ]; then \
		printf "$(RED)⚠ Warning: tests/Makefile.am exists but tests/Makefile.am.dev is missing$(NC)\n"; \
	elif [ -f tests/Makefile.am ]; then \
		printf "$(YELLOW)Removing development tests/Makefile.am...$(NC)\n"; \
		rm -f tests/Makefile.am tests/Makefile.in tests/Makefile; \
		rm -rf tests/.deps tests/autom4te.cache; \
		printf "$(GREEN)✓ Development test files removed$(NC)\n"; \
	fi; \
	if [ -f .git/hooks/pre-commit ]; then \
		printf "$(YELLOW)Removing git hooks...$(NC)\n"; \
		rm -f .git/hooks/pre-commit; \
		printf "$(GREEN)✓ Git hooks removed$(NC)\n"; \
	fi; \
	if [ -d node_modules ]; then \
		printf "$(YELLOW)Removing node_modules...$(NC)\n"; \
		rm -rf node_modules; \
		printf "$(GREEN)✓ node_modules removed$(NC)\n"; \
	fi; \
	autotools_files=$$(ls config.h config.log config.status Makefile aclocal.m4 configure 2>/dev/null | head -1); \
	if [ -n "$$autotools_files" ]; then \
		printf "$(YELLOW)Removing autotools files...$(NC)\n"; \
		rm -f compile_commands.json config.h config.log config.status Makefile; \
		rm -f aclocal.m4 configure configure~ config.h.in config.h.in~; \
		rm -f ar-lib compile depcomp install-sh missing config.guess config.sub; \
		printf "$(GREEN)✓ Autotools files removed$(NC)\n"; \
	fi; \
	if [ -f configure.ac.backup ]; then \
		printf "$(YELLOW)Restoring configure.ac from backup...$(NC)\n"; \
		mv configure.ac.backup configure.ac; \
		printf "$(GREEN)✓ configure.ac restored$(NC)\n"; \
	fi; \
	if [ -f Makefile.am.backup ]; then \
		printf "$(YELLOW)Restoring Makefile.am from backup...$(NC)\n"; \
		mv Makefile.am.backup Makefile.am; \
		printf "$(GREEN)✓ Makefile.am restored$(NC)\n"; \
	fi; \
	rm -f configure.ac.backup Makefile.am.backup stamp-h1 test-driver; \
	rm -rf autom4te.cache .deps; \
	remaining_files=$$(find . -name "Makefile.in" -o -name "test-driver" 2>/dev/null | head -1); \
	remaining_makefiles=$$(find . -name "Makefile" ! -path "./GNUmakefile" 2>/dev/null | head -1); \
	remaining_deps=$$(find . -name ".deps" -type d 2>/dev/null | head -1); \
	if [ -n "$$remaining_files" ] || [ -n "$$remaining_makefiles" ] || [ -n "$$remaining_deps" ]; then \
		printf "$(YELLOW)Cleaning remaining generated files...$(NC)\n"; \
		find . -name "Makefile.in" -delete 2>/dev/null || true; \
		find . -name "Makefile" ! -path "./GNUmakefile" -delete 2>/dev/null || true; \
		find . -name ".deps" -type d -exec rm -rf {} + 2>/dev/null || true; \
		find . -name "test-driver" -delete 2>/dev/null || true; \
		printf "$(GREEN)✓ Generated files removed$(NC)\n"; \
	fi

# Bootstrap targets 
.PHONY: bootstrap-all bootstrap-dev clean help setup-autotools setup-autotools-dev setup-hooks-install setup-hooks-remove setup-hooks-status check-autotools check-system-deps check-dev-deps check-no-standard-build install-deps install-dev-deps check test-full test-ci test-g15 test-g510 test-scenarios test-verbose test-memcheck test-compilers

bootstrap-all: setup-autotools configure-standard build

bootstrap-dev: check-no-standard-build setup-hooks-install setup-autotools-dev configure-dev build-dev

# Autotools setup
setup-autotools: check-autotools
	@printf "$(BLUE)=== Autotools Setup ===$(NC)\n"
	@# Ensure standard tests/Makefile.am exists
	@if [ ! -f tests/Makefile.am ]; then \
		echo "Creating standard tests/Makefile.am..."; \
		echo "## Process this file with automake to produce Makefile.in" > tests/Makefile.am; \
		echo "# Standard build - no tests" >> tests/Makefile.am; \
		echo "# Tests are only available in development mode (make dev)" >> tests/Makefile.am; \
		echo "" >> tests/Makefile.am; \
		echo "# Empty targets for standard autotools compatibility" >> tests/Makefile.am; \
		echo "all-local:" >> tests/Makefile.am; \
		echo "	@# Nothing to build in standard mode" >> tests/Makefile.am; \
		echo "" >> tests/Makefile.am; \
		echo "check-local:" >> tests/Makefile.am; \
		echo "	@echo \"Tests not available in standard build\"" >> tests/Makefile.am; \
		echo "	@echo \"Enable with: make distclean && make dev\"" >> tests/Makefile.am; \
		echo "" >> tests/Makefile.am; \
		echo "clean-local:" >> tests/Makefile.am; \
		echo "	@# Nothing to clean in standard mode" >> tests/Makefile.am; \
		echo "" >> tests/Makefile.am; \
		echo ".PHONY: all-local check-local clean-local" >> tests/Makefile.am; \
	fi
	@printf "$(YELLOW)Running aclocal ...$(NC)\n"
	@$(ACLOCAL) && printf "$(GREEN)✓ aclocal completed$(NC)\n" || { printf "$(RED)✗ aclocal failed$(NC)\n"; exit 1; }
	@if grep "^A[CM]_CONFIG_HEADER" configure.ac > /dev/null; then \
		printf "$(YELLOW)Running autoheader...$(NC)\n"; \
		$(AUTOHEADER) && printf "$(GREEN)✓ autoheader completed$(NC)\n" || { printf "$(RED)✗ autoheader failed$(NC)\n"; exit 1; }; \
	fi
	@printf "$(YELLOW)Running automake ...$(NC)\n"
	@$(AUTOMAKE) --add-missing --copy && printf "$(GREEN)✓ automake completed$(NC)\n" || { printf "$(RED)✗ automake failed$(NC)\n"; exit 1; }
	@printf "$(YELLOW)Running autoconf ...$(NC)\n"
	@$(AUTOCONF) && printf "$(GREEN)✓ autoconf completed$(NC)\n" || { printf "$(RED)✗ autoconf failed$(NC)\n"; exit 1; }
	@printf "$(GREEN)✓ Autotools setup complete!$(NC)\n"
	@echo


check-no-standard-build:
	@if [ -f config.h ] && ! grep -q "^#define DEBUG" config.h; then \
		printf "$(RED)❌ Standard build already configured$(NC)\n"; \
		printf "$(YELLOW)💡 Run 'make distclean' first to switch to development mode$(NC)\n"; \
		exit 1; \
	fi

check-autotools: check-system-deps

check-autotools-dev: check-dev-deps
	@printf "$(BLUE)=== Checking Autotools Dependencies ===$(NC)\n"
	@$(AUTOCONF) --version > /dev/null 2>&1 || { \
		echo; \
		printf "$(RED)✗ autoconf not found$(NC)\n"; \
		printf "$(YELLOW)📦 Install with: $(BLUE)sudo pacman -S autoconf$(NC)\n"; \
		exit 1; \
	}
	@$(AUTOMAKE) --version > /dev/null 2>&1 || { \
		echo; \
		printf "$(RED)✗ automake not found$(NC)\n"; \
		printf "$(YELLOW)📦 Install with: $(BLUE)sudo pacman -S automake$(NC)\n"; \
		exit 1; \
	}
	@$(ACLOCAL) --version > /dev/null 2>&1 || { \
		echo; \
		printf "$(RED)✗ aclocal not found$(NC)\n"; \
		printf "$(YELLOW)📦 Install with: $(BLUE)sudo pacman -S automake$(NC)\n"; \
		exit 1; \
	}

check-system-deps:
	@printf "$(BLUE)=== Checking System Dependencies ===$(NC)\n"
	@MISSING_DEPS=""; \
	command -v clang > /dev/null 2>&1 || { \
		printf "$(RED)✗ clang not found$(NC)\n"; \
		printf "$(YELLOW)📦 Install with: $(BLUE)sudo pacman -S clang$(NC)\n"; \
		MISSING_DEPS="$$MISSING_DEPS clang"; \
	}; \
	command -v make > /dev/null 2>&1 || { \
		printf "$(RED)✗ make not found$(NC)\n"; \
		printf "$(YELLOW)📦 Install with: $(BLUE)sudo pacman -S make$(NC)\n"; \
		MISSING_DEPS="$$MISSING_DEPS make"; \
	}; \
	command -v autoconf > /dev/null 2>&1 || { \
		printf "$(RED)✗ autoconf not found$(NC)\n"; \
		printf "$(YELLOW)📦 Install with: $(BLUE)sudo pacman -S autoconf$(NC)\n"; \
		MISSING_DEPS="$$MISSING_DEPS autoconf"; \
	}; \
	command -v automake > /dev/null 2>&1 || { \
		printf "$(RED)✗ automake not found$(NC)\n"; \
		printf "$(YELLOW)📦 Install with: $(BLUE)sudo pacman -S automake$(NC)\n"; \
		MISSING_DEPS="$$MISSING_DEPS automake"; \
	}; \
	pkg-config --exists libusb-1.0 2>/dev/null || { \
		printf "$(RED)✗ libusb-1.0 not found$(NC)\n"; \
		printf "$(YELLOW)📦 Install with: $(BLUE)sudo pacman -S libusb$(NC)\n"; \
		MISSING_DEPS="$$MISSING_DEPS libusb"; \
	}; \
	if ! pkg-config --exists libftdi1 2>/dev/null && ! pkg-config --exists libftdi 2>/dev/null; then \
		printf "$(RED)✗ libftdi not found$(NC)\n"; \
		printf "$(YELLOW)📦 Install with: $(BLUE)sudo pacman -S libftdi-compat$(NC)\n"; \
		MISSING_DEPS="$$MISSING_DEPS libftdi-compat"; \
	fi; \
	if [ "$$CI" != "true" ]; then \
		if [ ! -f "/usr/include/libg15.h" ] && [ ! -f "/usr/local/include/libg15.h" ]; then \
			printf "$(RED)✗ libg15 not found$(NC)\n"; \
			printf "$(YELLOW)📦 Install with: $(BLUE)sudo pacman -S libg15$(NC)\n"; \
			MISSING_DEPS="$$MISSING_DEPS libg15"; \
		fi; \
		if [ ! -f "/usr/include/libg15render.h" ] && [ ! -f "/usr/local/include/libg15render.h" ]; then \
			printf "$(RED)✗ libg15render not found$(NC)\n"; \
			printf "$(YELLOW)📦 Install with: $(BLUE)sudo pacman -S libg15render$(NC)\n"; \
			MISSING_DEPS="$$MISSING_DEPS libg15render"; \
		fi; \
		command -v ydotool > /dev/null 2>&1 || { \
			printf "$(RED)✗ ydotool not found$(NC)\n"; \
			printf "$(YELLOW)📦 Install with: $(BLUE)sudo pacman -S ydotool$(NC)\n"; \
			MISSING_DEPS="$$MISSING_DEPS ydotool"; \
		}; \
	else \
		printf "$(YELLOW)⚠ Skipping hardware-specific dependencies in CI mode$(NC)\n"; \
	fi; \
	if [ -n "$$MISSING_DEPS" ]; then \
		echo; \
		printf "$(RED)❌ Missing dependencies detected!$(NC)\n"; \
		printf "$(YELLOW)💡 Install all at once with:$(NC)\n"; \
		printf "$(BLUE)sudo pacman -S$$MISSING_DEPS$(NC)\n"; \
		printf "$(BLUE)yay -S libg15render$(NC) $(RED)(if missing)$(NC)\n"; \
		echo; \
		printf "$(YELLOW)🤖 Or run with guided installation:$(NC)\n"; \
		printf "$(BLUE)make install-deps$(NC)\n"; \
		exit 1; \
	else \
		printf "$(GREEN)✓ All system dependencies found$(NC)\n"; \
	fi

check-dev-deps:
	@printf "$(BLUE)=== Checking Development Dependencies ===$(NC)\n"
	@MISSING_DEPS=""; \
	MISSING_OPT=""; \
	command -v clang > /dev/null 2>&1 || { \
		printf "$(RED)✗ clang not found$(NC)\n"; \
		printf "$(YELLOW)📦 Install with: $(BLUE)sudo pacman -S clang$(NC)\n"; \
		MISSING_DEPS="$$MISSING_DEPS clang"; \
	}; \
	command -v make > /dev/null 2>&1 || { \
		printf "$(RED)✗ make not found$(NC)\n"; \
		printf "$(YELLOW)📦 Install with: $(BLUE)sudo pacman -S make$(NC)\n"; \
		MISSING_DEPS="$$MISSING_DEPS make"; \
	}; \
	command -v autoconf > /dev/null 2>&1 || { \
		printf "$(RED)✗ autoconf not found$(NC)\n"; \
		printf "$(YELLOW)📦 Install with: $(BLUE)sudo pacman -S autoconf$(NC)\n"; \
		MISSING_DEPS="$$MISSING_DEPS autoconf"; \
	}; \
	command -v automake > /dev/null 2>&1 || { \
		printf "$(RED)✗ automake not found$(NC)\n"; \
		printf "$(YELLOW)📦 Install with: $(BLUE)sudo pacman -S automake$(NC)\n"; \
		MISSING_DEPS="$$MISSING_DEPS automake"; \
	}; \
	pkg-config --exists libusb-1.0 2>/dev/null || { \
		printf "$(RED)✗ libusb-1.0 not found$(NC)\n"; \
		printf "$(YELLOW)📦 Install with: $(BLUE)sudo pacman -S libusb$(NC)\n"; \
		MISSING_DEPS="$$MISSING_DEPS libusb"; \
	}; \
	if ! pkg-config --exists libftdi1 2>/dev/null && ! pkg-config --exists libftdi 2>/dev/null; then \
		printf "$(RED)✗ libftdi not found$(NC)\n"; \
		printf "$(YELLOW)📦 Install with: $(BLUE)sudo pacman -S libftdi-compat$(NC)\n"; \
		MISSING_DEPS="$$MISSING_DEPS libftdi-compat"; \
	fi; \
	if [ "$$CI" != "true" ]; then \
		if [ ! -f "/usr/include/libg15.h" ] && [ ! -f "/usr/local/include/libg15.h" ]; then \
			printf "$(RED)✗ libg15 not found$(NC)\n"; \
			printf "$(YELLOW)📦 Install with: $(BLUE)sudo pacman -S libg15$(NC)\n"; \
			MISSING_DEPS="$$MISSING_DEPS libg15"; \
		fi; \
		if [ ! -f "/usr/include/libg15render.h" ] && [ ! -f "/usr/local/include/libg15render.h" ]; then \
			printf "$(RED)✗ libg15render not found$(NC)\n"; \
			printf "$(YELLOW)📦 Install with: $(BLUE)sudo pacman -S libg15render$(NC)\n"; \
			MISSING_DEPS="$$MISSING_DEPS libg15render"; \
		fi; \
		command -v ydotool > /dev/null 2>&1 || { \
			printf "$(RED)✗ ydotool not found$(NC)\n"; \
			printf "$(YELLOW)📦 Install with: $(BLUE)sudo pacman -S ydotool$(NC)\n"; \
			MISSING_DEPS="$$MISSING_DEPS ydotool"; \
		}; \
	else \
		printf "$(YELLOW)⚠ Skipping hardware-specific dependencies in CI mode$(NC)\n"; \
	fi; \
	printf "$(YELLOW)=== Optional Development Tools ===$(NC)\n"; \
	command -v gcc > /dev/null 2>&1 || { \
		printf "$(YELLOW)⚠ gcc not found$(NC)\n"; \
		printf "$(BLUE)📦 Install with: $(BLUE)sudo pacman -S gcc$(NC)\n"; \
		MISSING_OPT="$$MISSING_OPT gcc"; \
	}; \
	command -v npm > /dev/null 2>&1 || { \
		printf "$(YELLOW)⚠ npm not found$(NC)\n"; \
		printf "$(BLUE)📦 Install with: $(BLUE)sudo pacman -S npm$(NC)\n"; \
		MISSING_OPT="$$MISSING_OPT npm"; \
	}; \
	command -v bear > /dev/null 2>&1 || { \
		printf "$(YELLOW)⚠ bear not found$(NC)\n"; \
		printf "$(BLUE)📦 Install with: $(BLUE)sudo pacman -S bear$(NC)\n"; \
		MISSING_OPT="$$MISSING_OPT bear"; \
	}; \
	command -v act > /dev/null 2>&1 || { \
		printf "$(YELLOW)⚠ act not found$(NC)\n"; \
		printf "$(BLUE)📦 Install with: $(BLUE)yay -S act$(NC)\n"; \
		MISSING_OPT="$$MISSING_OPT act"; \
	}; \
	python -c "import evdev" 2>/dev/null || { \
		printf "$(YELLOW)⚠ python-evdev not found$(NC)\n"; \
		printf "$(BLUE)📦 Install with: $(BLUE)sudo pacman -S python-evdev$(NC)\n"; \
		MISSING_OPT="$$MISSING_OPT python-evdev"; \
	}; \
	if [ -n "$$MISSING_DEPS" ]; then \
		echo; \
		printf "$(RED)❌ Missing required dependencies detected!$(NC)\n"; \
		printf "$(YELLOW)💡 Install all at once with:$(NC)\n"; \
		printf "$(BLUE)sudo pacman -S$$MISSING_DEPS$(NC)\n"; \
		echo; \
		printf "$(YELLOW)🤖 Or run with guided installation:$(NC)\n"; \
		printf "$(BLUE)make install-dev-deps$(NC)\n"; \
		exit 1; \
	else \
		printf "$(GREEN)✓ All required dependencies found$(NC)\n"; \
	fi; \
	if [ -n "$$MISSING_OPT" ]; then \
		echo; \
		printf "$(YELLOW)💡 Optional tools available:$(NC)\n"; \
		printf "$(BLUE)sudo pacman -S$$MISSING_OPT$(NC)\n"; \
		printf "$(BLUE)make install-dev-deps$(NC) $(YELLOW)(installs all)$(NC)\n"; \
	else \
		printf "$(GREEN)✓ All optional development tools found$(NC)\n"; \
	fi


install-deps:
	@printf "$(BLUE)=== Guided Dependency Installation ===$(NC)\n"
	@printf "$(YELLOW)⚠ This will install system packages with sudo$(NC)\n"
	@read -p "Continue? (y/N): " confirm; \
	if [ "$$confirm" = "y" ] || [ "$$confirm" = "Y" ]; then \
		printf "$(BLUE)🔧 Installing build dependencies...$(NC)\n"; \
		sudo pacman -S --needed clang make autoconf automake || { \
			printf "$(RED)✗ Failed to install build tools$(NC)\n"; \
			exit 1; \
		}; \
		printf "$(BLUE)🔧 Installing G15 hardware support...$(NC)\n"; \
		sudo pacman -S --needed libg15 libg15render libusb libftdi-compat || { \
			printf "$(RED)✗ Failed to install G15 libraries$(NC)\n"; \
			exit 1; \
		}; \
		printf "$(BLUE)🔧 Installing G-Key macro system...$(NC)\n"; \
		sudo pacman -S --needed ydotool || { \
			printf "$(RED)✗ Failed to install ydotool$(NC)\n"; \
			exit 1; \
		}; \
		printf "$(GREEN)✅ All dependencies installed successfully!$(NC)\n"; \
		printf "$(BLUE)💡 Now run: make$(NC)\n"; \
	else \
		printf "$(YELLOW)❌ Installation cancelled$(NC)\n"; \
		exit 1; \
	fi

install-dev-deps:
	@printf "$(BLUE)=== Guided Development Dependencies Installation ===$(NC)\n"
	@printf "$(YELLOW)⚠ This will install required + optional packages with sudo$(NC)\n"
	@read -p "Continue? (y/N): " confirm; \
	if [ "$$confirm" = "y" ] || [ "$$confirm" = "Y" ]; then \
		printf "$(BLUE)🔧 Installing build dependencies...$(NC)\n"; \
		sudo pacman -S --needed clang make autoconf automake || { \
			printf "$(RED)✗ Failed to install build tools$(NC)\n"; \
			exit 1; \
		}; \
		printf "$(BLUE)🔧 Installing G15 hardware support...$(NC)\n"; \
		sudo pacman -S --needed libg15 libg15render libusb libftdi-compat || { \
			printf "$(RED)✗ Failed to install G15 libraries$(NC)\n"; \
			exit 1; \
		}; \
		printf "$(BLUE)🔧 Installing G-Key macro system...$(NC)\n"; \
		sudo pacman -S --needed ydotool || { \
			printf "$(RED)✗ Failed to install ydotool$(NC)\n"; \
			exit 1; \
		}; \
		printf "$(YELLOW)📦 Installing optional development tools...$(NC)\n"; \
		sudo pacman -S --needed gcc npm bear python-evdev || { \
			printf "$(YELLOW)⚠ Some optional tools may have failed to install$(NC)\n"; \
		}; \
		printf "$(YELLOW)📦 Installing AUR development tools...$(NC)\n"; \
		if command -v yay > /dev/null 2>&1; then \
			yay -S --needed act || { \
				printf "$(YELLOW)⚠ act installation failed$(NC)\n"; \
			}; \
		elif command -v paru > /dev/null 2>&1; then \
			paru -S --needed act || { \
				printf "$(YELLOW)⚠ act installation failed$(NC)\n"; \
			}; \
		else \
			printf "$(YELLOW)⚠ No AUR helper found - install act manually: yay -S act$(NC)\n"; \
		fi; \
		printf "$(GREEN)✅ Development environment setup complete!$(NC)\n"; \
		printf "$(BLUE)💡 Now run: make dev$(NC)\n"; \
	else \
		printf "$(YELLOW)❌ Installation cancelled$(NC)\n"; \
		exit 1; \
	fi

# Git hooks management
setup-hooks-install:
	@printf "$(BLUE)=== Installing Git Hooks ===$(NC)\n"; \
	if [ ! -d ".git" ]; then \
		printf "$(RED)✗ Not a git repository$(NC)\n"; \
		exit 1; \
	fi; \
	if [ ! -f "$(HOOK_TEMPLATE)" ]; then \
		printf "$(RED)✗ Hook template $(HOOK_TEMPLATE) not found$(NC)\n"; \
		exit 1; \
	fi; \
	if command -v npm > /dev/null 2>&1; then \
		if [ ! -f "package-lock.json" ] || [ ! -d "node_modules" ]; then \
			printf "$(YELLOW)Installing prettier dependencies...$(NC)\n"; \
			npm install || { \
				printf "$(RED)✗ Failed to install npm dependencies$(NC)\n"; \
				printf "$(YELLOW)⚠ Continuing with hook installation anyway$(NC)\n"; \
			}; \
		else \
			printf "$(GREEN)✓ Prettier dependencies already installed$(NC)\n"; \
		fi; \
	else \
		printf "$(YELLOW)⚠ npm not available - prettier formatting will not work$(NC)\n"; \
	fi; \
	if command -v clang-format > /dev/null 2>&1; then \
		printf "$(GREEN)✓ clang-format available for code formatting$(NC)\n"; \
	else \
		printf "$(YELLOW)⚠ clang-format not available - C code formatting will not work$(NC)\n"; \
		printf "$(YELLOW)  Install with: sudo pacman -S clang$(NC)\n"; \
	fi; \
	printf "$(YELLOW)Installing pre-commit hook...$(NC)\n"; \
	cp "$(HOOK_TEMPLATE)" "$(HOOK_SOURCE)"; \
	chmod +x "$(HOOK_SOURCE)"; \
	if [ -f "$(HOOK_SOURCE)" ]; then \
		printf "$(GREEN)✓ Pre-commit hook installed successfully$(NC)\n"; \
		printf "$(GREEN)✓ Code will be automatically formatted before commits$(NC)\n"; \
	else \
		printf "$(RED)✗ Failed to install hook$(NC)\n"; \
		exit 1; \
	fi

setup-hooks-remove:
	@printf "$(BLUE)=== Removing Git Hooks ===$(NC)\n"
	@if [ -f "$(HOOK_SOURCE)" ]; then \
		rm -f "$(HOOK_SOURCE)"; \
		printf "$(GREEN)✓ Pre-commit hook removed$(NC)\n"; \
	else \
		printf "$(YELLOW)⚠ No hook to remove$(NC)\n"; \
	fi

setup-hooks-status:
	@printf "$(BLUE)=== Git Hook Status ===$(NC)\n"
	@if [ ! -d ".git" ]; then \
		printf "$(RED)✗ Not a git repository$(NC)\n"; \
		exit 1; \
	fi
	@if [ -f "$(HOOK_SOURCE)" ]; then \
		printf "$(GREEN)✓ Pre-commit hook installed$(NC)\n"; \
		if command -v clang-format > /dev/null 2>&1; then \
			printf "$(GREEN)✓ clang-format available$(NC)\n"; \
		else \
			printf "$(YELLOW)⚠ clang-format not available$(NC)\n"; \
		fi; \
		if command -v npx > /dev/null 2>&1 && [ -f "package-lock.json" ]; then \
			printf "$(GREEN)✓ prettier available$(NC)\n"; \
		else \
			printf "$(YELLOW)⚠ prettier not available$(NC)\n"; \
		fi; \
	else \
		printf "$(RED)✗ Pre-commit hook not installed$(NC)\n"; \
		printf "$(YELLOW)💡 Install with: make setup-hooks-install$(NC)\n"; \
	fi

# Configure targets
configure-standard: setup-autotools
	@printf "$(BLUE)=== Code Formatting Setup ===$(NC)\n"
	@printf "$(YELLOW)⚠ Code formatting skipped (standard mode)$(NC)\n"
	@echo "To enable: make dev or make setup-hooks-install"
	@echo
	@printf "$(YELLOW)Running configure (standard)...$(NC)\n"
	@./configure --prefix=/usr --sbindir=/usr/bin --sysconfdir=/etc --enable-libusb --enable-lcdproc-menus --enable-stat-smbfs --enable-drivers=g15,linux_input

configure-dev: setup-autotools-dev
	@printf "$(BLUE)=== Code Formatting Setup ===$(NC)\n"
	@if [ ! -t 0 ] || [ -n "$(PKGBUILD_MODE)" ]; then \
		printf "$(YELLOW)⚠ Non-interactive mode detected - skipping formatting setup$(NC)\n"; \
		echo "Code formatting can be enabled manually with: make setup-hooks-install"; \
	else \
		printf "$(GREEN)✓ Code formatting setup complete!$(NC)\n"; \
		echo "Files will be automatically formatted before commits."; \
		echo "Manual formatting: make format"; \
	fi
	@echo
	@printf "$(YELLOW)Running configure (development)...$(NC)\n"
	@./configure --prefix=/usr --sbindir=/usr/bin --sysconfdir=/etc --enable-libusb --enable-lcdproc-menus --enable-stat-smbfs --enable-debug --enable-drivers=g15,linux_input,debug

# Separate autotools setup for development (includes tests)
setup-autotools-dev: check-autotools-dev
	@printf "$(BLUE)=== Autotools Setup (Development) ===$(NC)\n"
	@printf "$(YELLOW)Setting up tests for development build...$(NC)\n"
	@# Copy development test configuration
	@cp tests/Makefile.am.dev tests/Makefile.am
	@# Add tests to SUBDIRS in root Makefile.am 
	@cp Makefile.am Makefile.am.backup
	@sed 's/SUBDIRS = shared clients server services \./SUBDIRS = shared clients server services tests ./' Makefile.am.backup > Makefile.am
	@# Check if tests/Makefile is already in configure.ac 
	@cp configure.ac configure.ac.backup
	@if ! grep -q "tests/Makefile" configure.ac; then \
		sed 's/services\/Makefile/services\/Makefile\n\ttests\/Makefile/' configure.ac.backup > configure.ac; \
	fi
	@printf "$(GREEN)✓ Added tests to build system$(NC)\n"
	@printf "$(YELLOW)Running aclocal ...$(NC)\n"
	@$(ACLOCAL) && printf "$(GREEN)✓ aclocal completed$(NC)\n" || { printf "$(RED)✗ aclocal failed$(NC)\n"; exit 1; }
	@if grep "^A[CM]_CONFIG_HEADER" configure.ac > /dev/null; then \
		printf "$(YELLOW)Running autoheader...$(NC)\n"; \
		$(AUTOHEADER) && printf "$(GREEN)✓ autoheader completed$(NC)\n" || { printf "$(RED)✗ autoheader failed$(NC)\n"; exit 1; }; \
	fi
	@printf "$(YELLOW)Running automake ...$(NC)\n"
	@$(AUTOMAKE) --add-missing --copy && printf "$(GREEN)✓ automake completed$(NC)\n" || { printf "$(RED)✗ automake failed$(NC)\n"; exit 1; }
	@printf "$(YELLOW)Running autoconf ...$(NC)\n"
	@$(AUTOCONF) && printf "$(GREEN)✓ autoconf completed$(NC)\n" || { printf "$(RED)✗ autoconf failed$(NC)\n"; exit 1; }
	@printf "$(GREEN)✓ Autotools setup complete!$(NC)\n"
	@echo

# Build targets
build: configure-standard
	@printf "$(BLUE)=== Setup Complete! ===$(NC)\n"
	@printf "$(GREEN)✓ Autotools configured$(NC)\n"
	@if [ -f "$(HOOK_SOURCE)" ]; then \
		printf "$(GREEN)✓ Code formatting enabled$(NC)\n"; \
	else \
		printf "$(YELLOW)⚠ Code formatting skipped$(NC)\n"; \
	fi
	@echo
	@printf "$(GREEN)Ready to build! 🚀$(NC)\n"
	@printf "$(BLUE)Building (standard mode)...$(NC)\n"
	@echo "✅ Bootstrap complete - project ready to build!"
	@echo "🔄 Restarting make to use generated Makefile..."
	@$(MAKE) -f Makefile all

build-dev: configure-dev
	@printf "$(BLUE)=== Setup Complete! ===$(NC)\n"
	@printf "$(GREEN)✓ Autotools configured$(NC)\n"
	@printf "$(GREEN)✓ Code formatting enabled$(NC)\n"
	@echo
	@printf "$(GREEN)Ready to build! 🚀$(NC)\n"
	@printf "$(BLUE)Building (development mode)...$(NC)\n"
	@if [ -f .git/hooks/pre-commit ]; then \
		echo "🔍 Checking code formatting before build..."; \
		if ! $(MAKE) format-check; then \
			echo "⚠️  Formatting issues found - fixing automatically..."; \
			$(MAKE) format; \
			echo "✅ Files formatted - proceeding with build"; \
		else \
			echo "✅ All files properly formatted - proceeding with build"; \
		fi; \
	fi; \
	if command -v bear >/dev/null 2>&1; then \
		echo "Generating compile_commands.json for IDE/clangd..."; \
		echo "✅ Bootstrap complete - ready for development build with bear support!"; \
		echo "🔄 Restarting make to use generated Makefile..."; \
		bear -- $(MAKE) -f Makefile all; \
	else \
		echo "✅ Bootstrap complete - ready for development build!"; \
		echo "🔄 Restarting make to use generated Makefile..."; \
		$(MAKE) -f Makefile all; \
	fi


clean:
	@if [ -f Makefile ]; then \
		printf "$(BLUE)=== Cleaning Project ===$(NC)\n"; \
		printf "$(YELLOW)Removing compiled files and temporary artifacts...$(NC)\n"; \
		if $(MAKE) -f Makefile clean >/dev/null 2>&1; then \
			printf "$(GREEN)✓ Compiled files removed$(NC)\n"; \
			printf "$(YELLOW)Removing patch files...$(NC)\n"; \
			find . -name "*.orig" -delete 2>/dev/null || true; \
			find . -name "*.rej" -delete 2>/dev/null || true; \
			printf "$(GREEN)✓ Clean completed successfully$(NC)\n"; \
		else \
			printf "$(RED)✗ Clean failed$(NC)\n"; \
			exit 1; \
		fi; \
	else \
		has_artifacts=0; \
		if [ -f tests/test_unit_g15 ] || [ -f tests/test_integration_g15 ] || [ -f tests/mock_g15 ] || ls tests/*.o >/dev/null 2>&1 || ls tests/*.log >/dev/null 2>&1 || ls tests/*.trs >/dev/null 2>&1; then \
			has_artifacts=1; \
		fi; \
		has_patches=$$(find . -name "*.orig" -o -name "*.rej" | head -1); \
		if [ $$has_artifacts -eq 1 ] || [ -n "$$has_patches" ]; then \
			printf "$(BLUE)=== Cleaning Bootstrap Mode ===$(NC)\n"; \
			if [ $$has_artifacts -eq 1 ]; then \
				printf "$(YELLOW)Cleaning test artifacts...$(NC)\n"; \
				rm -f tests/test_unit_g15 tests/test_integration_g15 tests/mock_g15 tests/*.o tests/*.log tests/*.trs tests/test-suite.log; \
				printf "$(GREEN)✓ Test artifacts cleaned$(NC)\n"; \
			fi; \
			if [ -n "$$has_patches" ]; then \
				printf "$(YELLOW)Removing patch files...$(NC)\n"; \
				find . -name "*.orig" -delete 2>/dev/null || true; \
				find . -name "*.rej" -delete 2>/dev/null || true; \
				printf "$(GREEN)✓ Patch files removed$(NC)\n"; \
			fi; \
			printf "$(GREEN)✓ Clean completed$(NC)\n"; \
		else \
			printf "$(GREEN)✓ Nothing to clean$(NC)\n"; \
		fi; \
	fi


# Help target  
help:
	@echo "LCDproc G15 - Available Make Targets"
	@echo "===================================="
	@echo ""
	@printf "$(BLUE)🔨 Build Commands (with automatic setup):$(NC)\n"
	@echo "  make                    - Complete setup + standard build"
	@echo "  make dev                - Complete setup + development build (with git hooks)"
	@echo ""
	@printf "$(BLUE)🚀 Quick Start (Development Workflow):$(NC)\n"
	@echo "  make check              - Basic tests (~3s) - Daily development"
	@echo "  make test-full          - Comprehensive (~60s) - Before commits"
	@echo "  make test-coverage      - Code coverage analysis - Quality assurance"
	@echo ""
	@printf "$(BLUE)🔍 Specific Test Categories:$(NC)\n"
	@echo "  make test-g15           - Test only G15 devices (no RGB)"
	@echo "  make test-g510          - Test only G510 devices (with RGB)"
	@echo "  make test-verbose       - Run tests with detailed output"
	@echo "  make test-scenarios     - All 4 test scenarios:"
	@echo "                            • Device detection (USB IDs)"
	@echo "                            • RGB color testing (HID + LED)"
	@echo "                            • Macro system (G-keys, M1/M2/M3)"
	@echo "                            • Error handling (device failures)"
	@echo ""
	@printf "$(BLUE)🎯 Individual Test Scenarios:$(NC)\n"
	@echo "  make test-scenario-detection - Only device detection tests"
	@echo "  make test-scenario-rgb       - Only RGB functionality tests"  
	@echo "  make test-scenario-macros    - Only macro system tests"
	@echo "  make test-scenario-failures  - Only error handling tests"
	@echo ""
	@printf "$(BLUE)🚀 Advanced Testing:$(NC)\n"
	@echo "  make test-ci            - Complete CI/CD test suite"
	@echo "                            Includes: test-full + multi-compiler testing"
	@echo "  make test-integration   - End-to-end integration tests"
	@echo "                            Includes: LCDd server + client + mock hardware"
	@echo "  make test-integration-g15   - G15/G510 hardware integration tests"
	@echo "  make test-integration-input - Linux input driver integration tests"  
	@echo "  make test-integration-all   - All drivers comprehensive integration tests"
	@echo "  make test-server        - Test only LCDd server functionality"
	@echo "  make test-clients       - Test only client functionality"
	@echo "  make test-e2e           - Full end-to-end workflow testing"
	@echo ""
	@printf "$(BLUE)⚠️  Advanced (Standalone - For Debugging Only):$(NC)\n"
	@echo "  make test-memcheck      - Memory leak detection with AddressSanitizer (ASan)"
	@echo "  make test-coverage      - Code coverage analysis with gcovr"
	@echo "                            Generates: *.gcov coverage.xml coverage.html files"
	@echo "  make test-tsan          - ThreadSanitizer race condition detection"
	@echo "                            DESTRUCTIVE: Rebuilds entire project with -fsanitize=thread"
	@echo "  make test-tsan-quick    - Quick TSan test (requires test-tsan build first)"
	@echo "  make test-compilers     - Multi-compiler build testing"
	@echo "                            DESTRUCTIVE: Cleans + rebuilds with clang & gcc"
	@echo ""
	@printf "$(BLUE)🔧 Code Quality & Formatting (requires 'make dev'):$(NC)\n"
	@echo "  make format             - Format all C code with clang-format + prettier"
	@echo "  make format-check       - Check if formatting is needed (non-destructive)"
	@echo "  make lint               - Run clang-tidy static analysis"
	@echo "  make lint-fix           - Run clang-tidy with automatic fixes"
	@echo ""
	@printf "$(BLUE)🔧 Manual Setup (advanced users):$(NC)\n"
	@echo "  make setup-autotools    - Run autotools setup only"
	@echo "  make setup-hooks-install- Install git pre-commit hooks"
	@echo "  make setup-hooks-remove - Remove git hooks"  
	@echo "  make setup-hooks-status - Show git hooks status"
	@echo ""
	@printf "$(BLUE)💡 Usage Guidelines:$(NC)\n"
	@echo "  • Daily: make check (fast feedback)"
	@echo "  • Pre-commit: make test-full (thorough validation)"
	@echo "  • Coverage: make test-coverage (analyze test coverage)"
	@echo "  • Debugging: make test-scenarios (isolate issues)"
	@echo ""
	@printf "$(YELLOW)ℹ️  Project not configured yet. Run 'make' or 'make dev' to begin setup.$(NC)\n"

# Development-only test features - require make dev
test-verbose test-g15 test-g510 test-scenarios test-scenario-detection test-scenario-rgb test-scenario-macros test-scenario-failures test-memcheck test-coverage test-compilers test-full test-ci test-integration test-integration-g15 test-integration-input test-integration-all test-mock test-server test-clients test-e2e test-tsan test-tsan-quick:
	@if [ ! -f Makefile ]; then \
		printf "$(RED)❌ Project not configured yet$(NC)\n"; \
		printf "$(YELLOW)💡 Run 'make dev' first to set up development environment$(NC)\n"; \
		exit 1; \
	elif [ ! -f config.h ] || ! grep -q "^#define DEBUG" config.h 2>/dev/null; then \
		printf "$(RED)❌ Development build required for advanced test features$(NC)\n"; \
		printf "$(YELLOW)💡 Run 'make distclean && make dev' to enable development features$(NC)\n"; \
		printf "$(YELLOW)💡 Or use standard 'make check' for basic testing$(NC)\n"; \
		exit 1; \
	elif [ ! -f tests/Makefile ]; then \
		printf "$(RED)❌ Test suite not configured$(NC)\n"; \
		printf "$(YELLOW)💡 Development build required - run 'make distclean && make dev'$(NC)\n"; \
		exit 1; \
	else \
		$(MAKE) -C tests $@; \
	fi

# Development-only code quality features - require make dev
format format-check lint lint-fix:
	@if [ ! -f Makefile ]; then \
		printf "$(RED)❌ Project not configured yet$(NC)\n"; \
		printf "$(YELLOW)💡 Run 'make dev' first to set up development environment$(NC)\n"; \
		exit 1; \
	elif [ ! -f config.h ] || ! grep -q "^#define DEBUG" config.h 2>/dev/null; then \
		printf "$(RED)❌ Development build required for code quality features$(NC)\n"; \
		printf "$(YELLOW)💡 Run 'make distclean && make dev' to enable development features$(NC)\n"; \
		exit 1; \
	else \
		$(MAKE) -f Makefile $@; \
	fi

# Standard autotools check target - works with both standard and dev builds
check:
	@if [ -f Makefile ]; then \
		$(MAKE) -f Makefile check; \
	else \
		printf "$(RED)❌ Project not set up yet. Run 'make' or 'make dev' first.$(NC)\n"; \
		exit 1; \
	fi

# Delegate all other targets to Makefile if it exists, otherwise to bootstrap
%:
	@if [ -f Makefile ]; then \
		$(MAKE) -f Makefile $@; \
	else \
		printf "$(RED)❌ Project not set up yet. Run 'make' or 'make dev' first.$(NC)\n"; \
		exit 1; \
	fi