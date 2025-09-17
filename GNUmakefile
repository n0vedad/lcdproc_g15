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
		echo -e "$(RED)❌ Standard build already configured$(NC)"; \
		echo -e "$(YELLOW)💡 Run 'make distclean' first to rebuild or switch to development mode$(NC)"; \
		exit 1; \
	elif [ -f Makefile ] && [ -f config.h ] && grep -q "^#define DEBUG" config.h 2>/dev/null; then \
		echo -e "$(RED)❌ Development build already configured$(NC)"; \
		echo -e "$(YELLOW)💡 Run 'make distclean' first to switch to standard mode$(NC)"; \
		exit 1; \
	elif [ -f Makefile ]; then \
		echo "Using existing Makefile..."; \
		if $(MAKE) -f Makefile all; then \
			echo -e "$(GREEN)🎉 Build successful!$(NC)"; \
		else \
			echo -e "$(RED)❌ Build failed!$(NC)"; \
			exit 1; \
		fi; \
	else \
		echo "No Makefile found - running bootstrap..."; \
		if $(MAKE) -f $(lastword $(MAKEFILE_LIST)) bootstrap-all; then \
			echo -e "$(GREEN)🎉 Bootstrap and build successful!$(NC)"; \
		else \
			echo -e "$(RED)❌ Bootstrap or build failed!$(NC)"; \
			exit 1; \
		fi; \
	fi

# Development build target
dev:
	@if [ -f Makefile ] && [ -f config.h ] && ! grep -q "^#define DEBUG" config.h 2>/dev/null; then \
		echo -e "$(RED)❌ Standard build already configured$(NC)"; \
		echo -e "$(YELLOW)💡 Run 'make distclean' first to switch to development mode$(NC)"; \
		exit 1; \
	elif [ -f Makefile ] && [ -f config.h ] && grep -q "^#define DEBUG" config.h 2>/dev/null; then \
		echo -e "$(RED)❌ Development build already configured$(NC)"; \
		echo -e "$(YELLOW)💡 Run 'make distclean' first to rebuild or switch to standard mode$(NC)"; \
		exit 1; \
	elif [ -f Makefile ]; then \
		echo "Using existing Makefile..."; \
		if $(MAKE) -f Makefile all; then \
			echo -e "$(GREEN)🎉 Development build successful!$(NC)"; \
		else \
			echo -e "$(RED)❌ Development build failed!$(NC)"; \
			exit 1; \
		fi; \
	else \
		echo "No development build found - running bootstrap..."; \
		if $(MAKE) -f $(lastword $(MAKEFILE_LIST)) bootstrap-dev; then \
			echo -e "$(GREEN)🎉 Development bootstrap and build successful!$(NC)"; \
		else \
			echo -e "$(RED)❌ Development bootstrap or build failed!$(NC)"; \
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
		echo -e "$(GREEN)✓ Project already clean$(NC)"; \
		exit 0; \
	fi; \
	if [ -f Makefile ]; then \
		echo -e "$(BLUE)=== Running Clean First ===$(NC)"; \
		$(MAKE) clean >/dev/null 2>&1; \
		echo -e "$(GREEN)✓ Build artifacts cleaned$(NC)"; \
	fi; \
	if [ -f tests/Makefile.am ] && [ ! -f tests/Makefile.am.dev ]; then \
		echo -e "$(RED)⚠ Warning: tests/Makefile.am exists but tests/Makefile.am.dev is missing$(NC)"; \
	elif [ -f tests/Makefile.am ]; then \
		echo -e "$(YELLOW)Removing development tests/Makefile.am...$(NC)"; \
		rm -f tests/Makefile.am tests/Makefile.in tests/Makefile; \
		rm -rf tests/.deps tests/autom4te.cache; \
		echo -e "$(GREEN)✓ Development test files removed$(NC)"; \
	fi; \
	if [ -f .git/hooks/pre-commit ]; then \
		echo -e "$(YELLOW)Removing git hooks...$(NC)"; \
		rm -f .git/hooks/pre-commit; \
		echo -e "$(GREEN)✓ Git hooks removed$(NC)"; \
	fi; \
	if [ -d node_modules ]; then \
		echo -e "$(YELLOW)Removing node_modules...$(NC)"; \
		rm -rf node_modules; \
		echo -e "$(GREEN)✓ node_modules removed$(NC)"; \
	fi; \
	autotools_files=$$(ls config.h config.log config.status Makefile aclocal.m4 configure 2>/dev/null | head -1); \
	if [ -n "$$autotools_files" ]; then \
		echo -e "$(YELLOW)Removing autotools files...$(NC)"; \
		rm -f compile_commands.json config.h config.log config.status Makefile; \
		rm -f aclocal.m4 configure configure~ config.h.in config.h.in~; \
		rm -f ar-lib compile depcomp install-sh missing config.guess config.sub; \
		echo -e "$(GREEN)✓ Autotools files removed$(NC)"; \
	fi; \
	if [ -f configure.ac.backup ]; then \
		echo -e "$(YELLOW)Restoring configure.ac from backup...$(NC)"; \
		mv configure.ac.backup configure.ac; \
		echo -e "$(GREEN)✓ configure.ac restored$(NC)"; \
	fi; \
	if [ -f Makefile.am.backup ]; then \
		echo -e "$(YELLOW)Restoring Makefile.am from backup...$(NC)"; \
		mv Makefile.am.backup Makefile.am; \
		echo -e "$(GREEN)✓ Makefile.am restored$(NC)"; \
	fi; \
	rm -f configure.ac.backup Makefile.am.backup stamp-h1 test-driver; \
	rm -rf autom4te.cache .deps; \
	remaining_files=$$(find . -name "Makefile.in" -o -name "test-driver" 2>/dev/null | head -1); \
	remaining_makefiles=$$(find . -name "Makefile" ! -path "./GNUmakefile" 2>/dev/null | head -1); \
	remaining_deps=$$(find . -name ".deps" -type d 2>/dev/null | head -1); \
	if [ -n "$$remaining_files" ] || [ -n "$$remaining_makefiles" ] || [ -n "$$remaining_deps" ]; then \
		echo -e "$(YELLOW)Cleaning remaining generated files...$(NC)"; \
		find . -name "Makefile.in" -delete 2>/dev/null || true; \
		find . -name "Makefile" ! -path "./GNUmakefile" -delete 2>/dev/null || true; \
		find . -name ".deps" -type d -exec rm -rf {} + 2>/dev/null || true; \
		find . -name "test-driver" -delete 2>/dev/null || true; \
		echo -e "$(GREEN)✓ Generated files removed$(NC)"; \
	fi

# Bootstrap targets 
.PHONY: bootstrap-all bootstrap-dev clean help setup-autotools setup-autotools-dev setup-hooks-install setup-hooks-remove setup-hooks-status check-autotools check-system-deps check-dev-deps check-no-standard-build install-deps install-dev-deps check test-full test-ci test-g15 test-g510 test-scenarios test-verbose test-memcheck test-compilers

bootstrap-all: setup-autotools configure-standard build

bootstrap-dev: check-no-standard-build setup-hooks-install setup-autotools-dev configure-dev build-dev

# Autotools setup
setup-autotools: check-autotools
	@echo -e "$(BLUE)=== Autotools Setup ===$(NC)"
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
	@echo -e "$(YELLOW)Running aclocal ...$(NC)"
	@$(ACLOCAL) && echo -e "$(GREEN)✓ aclocal completed$(NC)" || { echo -e "$(RED)✗ aclocal failed$(NC)"; exit 1; }
	@if grep "^A[CM]_CONFIG_HEADER" configure.ac > /dev/null; then \
		echo -e "$(YELLOW)Running autoheader...$(NC)"; \
		$(AUTOHEADER) && echo -e "$(GREEN)✓ autoheader completed$(NC)" || { echo -e "$(RED)✗ autoheader failed$(NC)"; exit 1; }; \
	fi
	@echo -e "$(YELLOW)Running automake ...$(NC)"
	@$(AUTOMAKE) --add-missing --copy && echo -e "$(GREEN)✓ automake completed$(NC)" || { echo -e "$(RED)✗ automake failed$(NC)"; exit 1; }
	@echo -e "$(YELLOW)Running autoconf ...$(NC)"
	@$(AUTOCONF) && echo -e "$(GREEN)✓ autoconf completed$(NC)" || { echo -e "$(RED)✗ autoconf failed$(NC)"; exit 1; }
	@echo -e "$(GREEN)✓ Autotools setup complete!$(NC)"
	@echo


check-no-standard-build:
	@if [ -f config.h ] && ! grep -q "^#define DEBUG" config.h; then \
		echo -e "$(RED)❌ Standard build already configured$(NC)"; \
		echo -e "$(YELLOW)💡 Run 'make distclean' first to switch to development mode$(NC)"; \
		exit 1; \
	fi

check-autotools: check-system-deps

check-autotools-dev: check-dev-deps
	@echo -e "$(BLUE)=== Checking Autotools Dependencies ===$(NC)"
	@$(AUTOCONF) --version > /dev/null 2>&1 || { \
		echo; \
		echo -e "$(RED)✗ autoconf not found$(NC)"; \
		echo -e "$(YELLOW)📦 Install with: $(BLUE)sudo pacman -S autoconf$(NC)"; \
		exit 1; \
	}
	@$(AUTOMAKE) --version > /dev/null 2>&1 || { \
		echo; \
		echo -e "$(RED)✗ automake not found$(NC)"; \
		echo -e "$(YELLOW)📦 Install with: $(BLUE)sudo pacman -S automake$(NC)"; \
		exit 1; \
	}
	@$(ACLOCAL) --version > /dev/null 2>&1 || { \
		echo; \
		echo -e "$(RED)✗ aclocal not found$(NC)"; \
		echo -e "$(YELLOW)📦 Install with: $(BLUE)sudo pacman -S automake$(NC)"; \
		exit 1; \
	}

check-system-deps:
	@echo -e "$(BLUE)=== Checking System Dependencies ===$(NC)"
	@MISSING_DEPS=""; \
	command -v clang > /dev/null 2>&1 || { \
		echo -e "$(RED)✗ clang not found$(NC)"; \
		echo -e "$(YELLOW)📦 Install with: $(BLUE)sudo pacman -S clang$(NC)"; \
		MISSING_DEPS="$$MISSING_DEPS clang"; \
	}; \
	command -v make > /dev/null 2>&1 || { \
		echo -e "$(RED)✗ make not found$(NC)"; \
		echo -e "$(YELLOW)📦 Install with: $(BLUE)sudo pacman -S make$(NC)"; \
		MISSING_DEPS="$$MISSING_DEPS make"; \
	}; \
	command -v autoconf > /dev/null 2>&1 || { \
		echo -e "$(RED)✗ autoconf not found$(NC)"; \
		echo -e "$(YELLOW)📦 Install with: $(BLUE)sudo pacman -S autoconf$(NC)"; \
		MISSING_DEPS="$$MISSING_DEPS autoconf"; \
	}; \
	command -v automake > /dev/null 2>&1 || { \
		echo -e "$(RED)✗ automake not found$(NC)"; \
		echo -e "$(YELLOW)📦 Install with: $(BLUE)sudo pacman -S automake$(NC)"; \
		MISSING_DEPS="$$MISSING_DEPS automake"; \
	}; \
	pkg-config --exists libusb-1.0 2>/dev/null || { \
		echo -e "$(RED)✗ libusb-1.0 not found$(NC)"; \
		echo -e "$(YELLOW)📦 Install with: $(BLUE)sudo pacman -S libusb$(NC)"; \
		MISSING_DEPS="$$MISSING_DEPS libusb"; \
	}; \
	if ! pkg-config --exists libftdi1 2>/dev/null && ! pkg-config --exists libftdi 2>/dev/null; then \
		echo -e "$(RED)✗ libftdi not found$(NC)"; \
		echo -e "$(YELLOW)📦 Install with: $(BLUE)sudo pacman -S libftdi-compat$(NC)"; \
		MISSING_DEPS="$$MISSING_DEPS libftdi-compat"; \
	fi; \
	if [ "$$CI" != "true" ]; then \
		if [ ! -f "/usr/include/libg15.h" ] && [ ! -f "/usr/local/include/libg15.h" ]; then \
			echo -e "$(RED)✗ libg15 not found$(NC)"; \
			echo -e "$(YELLOW)📦 Install with: $(BLUE)sudo pacman -S libg15$(NC)"; \
			MISSING_DEPS="$$MISSING_DEPS libg15"; \
		fi; \
		if [ ! -f "/usr/include/libg15render.h" ] && [ ! -f "/usr/local/include/libg15render.h" ]; then \
			echo -e "$(RED)✗ libg15render not found$(NC)"; \
			echo -e "$(YELLOW)📦 Install with: $(BLUE)sudo pacman -S libg15render$(NC)"; \
			MISSING_DEPS="$$MISSING_DEPS libg15render"; \
		fi; \
		command -v ydotool > /dev/null 2>&1 || { \
			echo -e "$(RED)✗ ydotool not found$(NC)"; \
			echo -e "$(YELLOW)📦 Install with: $(BLUE)sudo pacman -S ydotool$(NC)"; \
			MISSING_DEPS="$$MISSING_DEPS ydotool"; \
		}; \
	else \
		echo -e "$(YELLOW)⚠ Skipping hardware-specific dependencies in CI mode$(NC)"; \
	fi; \
	if [ -n "$$MISSING_DEPS" ]; then \
		echo; \
		echo -e "$(RED)❌ Missing dependencies detected!$(NC)"; \
		echo -e "$(YELLOW)💡 Install all at once with:$(NC)"; \
		echo -e "$(BLUE)sudo pacman -S$$MISSING_DEPS$(NC)"; \
		echo -e "$(BLUE)yay -S libg15render$(NC) $(RED)(if missing)$(NC)"; \
		echo; \
		echo -e "$(YELLOW)🤖 Or run with guided installation:$(NC)"; \
		echo -e "$(BLUE)make install-deps$(NC)"; \
		exit 1; \
	else \
		echo -e "$(GREEN)✓ All system dependencies found$(NC)"; \
	fi

check-dev-deps:
	@echo -e "$(BLUE)=== Checking Development Dependencies ===$(NC)"
	@MISSING_DEPS=""; \
	MISSING_OPT=""; \
	command -v clang > /dev/null 2>&1 || { \
		echo -e "$(RED)✗ clang not found$(NC)"; \
		echo -e "$(YELLOW)📦 Install with: $(BLUE)sudo pacman -S clang$(NC)"; \
		MISSING_DEPS="$$MISSING_DEPS clang"; \
	}; \
	command -v make > /dev/null 2>&1 || { \
		echo -e "$(RED)✗ make not found$(NC)"; \
		echo -e "$(YELLOW)📦 Install with: $(BLUE)sudo pacman -S make$(NC)"; \
		MISSING_DEPS="$$MISSING_DEPS make"; \
	}; \
	command -v autoconf > /dev/null 2>&1 || { \
		echo -e "$(RED)✗ autoconf not found$(NC)"; \
		echo -e "$(YELLOW)📦 Install with: $(BLUE)sudo pacman -S autoconf$(NC)"; \
		MISSING_DEPS="$$MISSING_DEPS autoconf"; \
	}; \
	command -v automake > /dev/null 2>&1 || { \
		echo -e "$(RED)✗ automake not found$(NC)"; \
		echo -e "$(YELLOW)📦 Install with: $(BLUE)sudo pacman -S automake$(NC)"; \
		MISSING_DEPS="$$MISSING_DEPS automake"; \
	}; \
	pkg-config --exists libusb-1.0 2>/dev/null || { \
		echo -e "$(RED)✗ libusb-1.0 not found$(NC)"; \
		echo -e "$(YELLOW)📦 Install with: $(BLUE)sudo pacman -S libusb$(NC)"; \
		MISSING_DEPS="$$MISSING_DEPS libusb"; \
	}; \
	if ! pkg-config --exists libftdi1 2>/dev/null && ! pkg-config --exists libftdi 2>/dev/null; then \
		echo -e "$(RED)✗ libftdi not found$(NC)"; \
		echo -e "$(YELLOW)📦 Install with: $(BLUE)sudo pacman -S libftdi-compat$(NC)"; \
		MISSING_DEPS="$$MISSING_DEPS libftdi-compat"; \
	fi; \
	if [ "$$CI" != "true" ]; then \
		if [ ! -f "/usr/include/libg15.h" ] && [ ! -f "/usr/local/include/libg15.h" ]; then \
			echo -e "$(RED)✗ libg15 not found$(NC)"; \
			echo -e "$(YELLOW)📦 Install with: $(BLUE)sudo pacman -S libg15$(NC)"; \
			MISSING_DEPS="$$MISSING_DEPS libg15"; \
		fi; \
		if [ ! -f "/usr/include/libg15render.h" ] && [ ! -f "/usr/local/include/libg15render.h" ]; then \
			echo -e "$(RED)✗ libg15render not found$(NC)"; \
			echo -e "$(YELLOW)📦 Install with: $(BLUE)sudo pacman -S libg15render$(NC)"; \
			MISSING_DEPS="$$MISSING_DEPS libg15render"; \
		fi; \
		command -v ydotool > /dev/null 2>&1 || { \
			echo -e "$(RED)✗ ydotool not found$(NC)"; \
			echo -e "$(YELLOW)📦 Install with: $(BLUE)sudo pacman -S ydotool$(NC)"; \
			MISSING_DEPS="$$MISSING_DEPS ydotool"; \
		}; \
	else \
		echo -e "$(YELLOW)⚠ Skipping hardware-specific dependencies in CI mode$(NC)"; \
	fi; \
	echo -e "$(YELLOW)=== Optional Development Tools ===$(NC)"; \
	command -v gcc > /dev/null 2>&1 || { \
		echo -e "$(YELLOW)⚠ gcc not found$(NC)"; \
		echo -e "$(BLUE)📦 Install with: $(BLUE)sudo pacman -S gcc$(NC)"; \
		MISSING_OPT="$$MISSING_OPT gcc"; \
	}; \
	command -v npm > /dev/null 2>&1 || { \
		echo -e "$(YELLOW)⚠ npm not found$(NC)"; \
		echo -e "$(BLUE)📦 Install with: $(BLUE)sudo pacman -S npm$(NC)"; \
		MISSING_OPT="$$MISSING_OPT npm"; \
	}; \
	command -v bear > /dev/null 2>&1 || { \
		echo -e "$(YELLOW)⚠ bear not found$(NC)"; \
		echo -e "$(BLUE)📦 Install with: $(BLUE)sudo pacman -S bear$(NC)"; \
		MISSING_OPT="$$MISSING_OPT bear"; \
	}; \
	command -v valgrind > /dev/null 2>&1 || { \
		echo -e "$(YELLOW)⚠ valgrind not found$(NC)"; \
		echo -e "$(BLUE)📦 Install with: $(BLUE)sudo pacman -S valgrind$(NC)"; \
		MISSING_OPT="$$MISSING_OPT valgrind"; \
	}; \
	command -v act > /dev/null 2>&1 || { \
		echo -e "$(YELLOW)⚠ act not found$(NC)"; \
		echo -e "$(BLUE)📦 Install with: $(BLUE)yay -S act$(NC)"; \
		MISSING_OPT="$$MISSING_OPT act"; \
	}; \
	python -c "import evdev" 2>/dev/null || { \
		echo -e "$(YELLOW)⚠ python-evdev not found$(NC)"; \
		echo -e "$(BLUE)📦 Install with: $(BLUE)sudo pacman -S python-evdev$(NC)"; \
		MISSING_OPT="$$MISSING_OPT python-evdev"; \
	}; \
	if [ -n "$$MISSING_DEPS" ]; then \
		echo; \
		echo -e "$(RED)❌ Missing required dependencies detected!$(NC)"; \
		echo -e "$(YELLOW)💡 Install all at once with:$(NC)"; \
		echo -e "$(BLUE)sudo pacman -S$$MISSING_DEPS$(NC)"; \
		echo; \
		echo -e "$(YELLOW)🤖 Or run with guided installation:$(NC)"; \
		echo -e "$(BLUE)make install-dev-deps$(NC)"; \
		exit 1; \
	else \
		echo -e "$(GREEN)✓ All required dependencies found$(NC)"; \
	fi; \
	if [ -n "$$MISSING_OPT" ]; then \
		echo; \
		echo -e "$(YELLOW)💡 Optional tools available:$(NC)"; \
		echo -e "$(BLUE)sudo pacman -S$$MISSING_OPT$(NC)"; \
		echo -e "$(BLUE)make install-dev-deps$(NC) $(YELLOW)(installs all)$(NC)"; \
	else \
		echo -e "$(GREEN)✓ All optional development tools found$(NC)"; \
	fi


install-deps:
	@echo -e "$(BLUE)=== Guided Dependency Installation ===$(NC)"
	@echo -e "$(YELLOW)⚠ This will install system packages with sudo$(NC)"
	@read -p "Continue? (y/N): " confirm; \
	if [ "$$confirm" = "y" ] || [ "$$confirm" = "Y" ]; then \
		echo -e "$(BLUE)🔧 Installing build dependencies...$(NC)"; \
		sudo pacman -S --needed clang make autoconf automake || { \
			echo -e "$(RED)✗ Failed to install build tools$(NC)"; \
			exit 1; \
		}; \
		echo -e "$(BLUE)🔧 Installing G15 hardware support...$(NC)"; \
		sudo pacman -S --needed libg15 libg15render libusb libftdi-compat || { \
			echo -e "$(RED)✗ Failed to install G15 libraries$(NC)"; \
			exit 1; \
		}; \
		echo -e "$(BLUE)🔧 Installing G-Key macro system...$(NC)"; \
		sudo pacman -S --needed ydotool || { \
			echo -e "$(RED)✗ Failed to install ydotool$(NC)"; \
			exit 1; \
		}; \
		echo -e "$(GREEN)✅ All dependencies installed successfully!$(NC)"; \
		echo -e "$(BLUE)💡 Now run: make$(NC)"; \
	else \
		echo -e "$(YELLOW)❌ Installation cancelled$(NC)"; \
		exit 1; \
	fi

install-dev-deps:
	@echo -e "$(BLUE)=== Guided Development Dependencies Installation ===$(NC)"
	@echo -e "$(YELLOW)⚠ This will install required + optional packages with sudo$(NC)"
	@read -p "Continue? (y/N): " confirm; \
	if [ "$$confirm" = "y" ] || [ "$$confirm" = "Y" ]; then \
		echo -e "$(BLUE)🔧 Installing build dependencies...$(NC)"; \
		sudo pacman -S --needed clang make autoconf automake || { \
			echo -e "$(RED)✗ Failed to install build tools$(NC)"; \
			exit 1; \
		}; \
		echo -e "$(BLUE)🔧 Installing G15 hardware support...$(NC)"; \
		sudo pacman -S --needed libg15 libg15render libusb libftdi-compat || { \
			echo -e "$(RED)✗ Failed to install G15 libraries$(NC)"; \
			exit 1; \
		}; \
		echo -e "$(BLUE)🔧 Installing G-Key macro system...$(NC)"; \
		sudo pacman -S --needed ydotool || { \
			echo -e "$(RED)✗ Failed to install ydotool$(NC)"; \
			exit 1; \
		}; \
		echo -e "$(YELLOW)📦 Installing optional development tools...$(NC)"; \
		sudo pacman -S --needed gcc npm bear valgrind python-evdev || { \
			echo -e "$(YELLOW)⚠ Some optional tools may have failed to install$(NC)"; \
		}; \
		echo -e "$(YELLOW)📦 Installing AUR development tools...$(NC)"; \
		if command -v yay > /dev/null 2>&1; then \
			yay -S --needed act || { \
				echo -e "$(YELLOW)⚠ act installation failed$(NC)"; \
			}; \
		elif command -v paru > /dev/null 2>&1; then \
			paru -S --needed act || { \
				echo -e "$(YELLOW)⚠ act installation failed$(NC)"; \
			}; \
		else \
			echo -e "$(YELLOW)⚠ No AUR helper found - install act manually: yay -S act$(NC)"; \
		fi; \
		echo -e "$(GREEN)✅ Development environment setup complete!$(NC)"; \
		echo -e "$(BLUE)💡 Now run: make dev$(NC)"; \
	else \
		echo -e "$(YELLOW)❌ Installation cancelled$(NC)"; \
		exit 1; \
	fi

# Git hooks management
setup-hooks-install:
	@echo -e "$(BLUE)=== Installing Git Hooks ===$(NC)"; \
	if [ ! -d ".git" ]; then \
		echo -e "$(RED)✗ Not a git repository$(NC)"; \
		exit 1; \
	fi; \
	if [ ! -f "$(HOOK_TEMPLATE)" ]; then \
		echo -e "$(RED)✗ Hook template $(HOOK_TEMPLATE) not found$(NC)"; \
		exit 1; \
	fi; \
	if command -v npm > /dev/null 2>&1; then \
		if [ ! -f "package-lock.json" ] || [ ! -d "node_modules" ]; then \
			echo -e "$(YELLOW)Installing prettier dependencies...$(NC)"; \
			npm install || { \
				echo -e "$(RED)✗ Failed to install npm dependencies$(NC)"; \
				echo -e "$(YELLOW)⚠ Continuing with hook installation anyway$(NC)"; \
			}; \
		else \
			echo -e "$(GREEN)✓ Prettier dependencies already installed$(NC)"; \
		fi; \
	else \
		echo -e "$(YELLOW)⚠ npm not available - prettier formatting will not work$(NC)"; \
	fi; \
	if command -v clang-format > /dev/null 2>&1; then \
		echo -e "$(GREEN)✓ clang-format available for code formatting$(NC)"; \
	else \
		echo -e "$(YELLOW)⚠ clang-format not available - C code formatting will not work$(NC)"; \
		echo -e "$(YELLOW)  Install with: sudo pacman -S clang$(NC)"; \
	fi; \
	echo -e "$(YELLOW)Installing pre-commit hook...$(NC)"; \
	cp "$(HOOK_TEMPLATE)" "$(HOOK_SOURCE)"; \
	chmod +x "$(HOOK_SOURCE)"; \
	if [ -f "$(HOOK_SOURCE)" ]; then \
		echo -e "$(GREEN)✓ Pre-commit hook installed successfully$(NC)"; \
		echo -e "$(GREEN)✓ Code will be automatically formatted before commits$(NC)"; \
	else \
		echo -e "$(RED)✗ Failed to install hook$(NC)"; \
		exit 1; \
	fi

setup-hooks-remove:
	@echo -e "$(BLUE)=== Removing Git Hooks ===$(NC)"
	@if [ -f "$(HOOK_SOURCE)" ]; then \
		rm -f "$(HOOK_SOURCE)"; \
		echo -e "$(GREEN)✓ Pre-commit hook removed$(NC)"; \
	else \
		echo -e "$(YELLOW)⚠ No hook to remove$(NC)"; \
	fi

setup-hooks-status:
	@echo -e "$(BLUE)=== Git Hook Status ===$(NC)"
	@if [ ! -d ".git" ]; then \
		echo -e "$(RED)✗ Not a git repository$(NC)"; \
		exit 1; \
	fi
	@if [ -f "$(HOOK_SOURCE)" ]; then \
		echo -e "$(GREEN)✓ Pre-commit hook installed$(NC)"; \
		if command -v clang-format > /dev/null 2>&1; then \
			echo -e "$(GREEN)✓ clang-format available$(NC)"; \
		else \
			echo -e "$(YELLOW)⚠ clang-format not available$(NC)"; \
		fi; \
		if command -v npx > /dev/null 2>&1 && [ -f "package-lock.json" ]; then \
			echo -e "$(GREEN)✓ prettier available$(NC)"; \
		else \
			echo -e "$(YELLOW)⚠ prettier not available$(NC)"; \
		fi; \
	else \
		echo -e "$(RED)✗ Pre-commit hook not installed$(NC)"; \
		echo -e "$(YELLOW)💡 Install with: make setup-hooks-install$(NC)"; \
	fi

# Configure targets
configure-standard: setup-autotools
	@echo -e "$(BLUE)=== Code Formatting Setup ===$(NC)"
	@echo -e "$(YELLOW)⚠ Code formatting skipped (standard mode)$(NC)"
	@echo "To enable: make dev or make setup-hooks-install"
	@echo
	@echo -e "$(YELLOW)Running configure (standard)...$(NC)"
	@./configure --prefix=/usr --sbindir=/usr/bin --sysconfdir=/etc --enable-libusb --enable-lcdproc-menus --enable-stat-smbfs --enable-drivers=g15,linux_input

configure-dev: setup-autotools-dev
	@echo -e "$(BLUE)=== Code Formatting Setup ===$(NC)"
	@if [ ! -t 0 ] || [ -n "$(PKGBUILD_MODE)" ]; then \
		echo -e "$(YELLOW)⚠ Non-interactive mode detected - skipping formatting setup$(NC)"; \
		echo "Code formatting can be enabled manually with: make setup-hooks-install"; \
	else \
		echo -e "$(GREEN)✓ Code formatting setup complete!$(NC)"; \
		echo "Files will be automatically formatted before commits."; \
		echo "Manual formatting: make format"; \
	fi
	@echo
	@echo -e "$(YELLOW)Running configure (development)...$(NC)"
	@./configure --enable-debug --enable-drivers=g15,linux_input,debug

# Separate autotools setup for development (includes tests)
setup-autotools-dev: check-autotools-dev
	@echo -e "$(BLUE)=== Autotools Setup (Development) ===$(NC)"
	@echo -e "$(YELLOW)Setting up tests for development build...$(NC)"
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
	@echo -e "$(GREEN)✓ Added tests to build system$(NC)"
	@echo -e "$(YELLOW)Running aclocal ...$(NC)"
	@$(ACLOCAL) && echo -e "$(GREEN)✓ aclocal completed$(NC)" || { echo -e "$(RED)✗ aclocal failed$(NC)"; exit 1; }
	@if grep "^A[CM]_CONFIG_HEADER" configure.ac > /dev/null; then \
		echo -e "$(YELLOW)Running autoheader...$(NC)"; \
		$(AUTOHEADER) && echo -e "$(GREEN)✓ autoheader completed$(NC)" || { echo -e "$(RED)✗ autoheader failed$(NC)"; exit 1; }; \
	fi
	@echo -e "$(YELLOW)Running automake ...$(NC)"
	@$(AUTOMAKE) --add-missing --copy && echo -e "$(GREEN)✓ automake completed$(NC)" || { echo -e "$(RED)✗ automake failed$(NC)"; exit 1; }
	@echo -e "$(YELLOW)Running autoconf ...$(NC)"
	@$(AUTOCONF) && echo -e "$(GREEN)✓ autoconf completed$(NC)" || { echo -e "$(RED)✗ autoconf failed$(NC)"; exit 1; }
	@echo -e "$(GREEN)✓ Autotools setup complete!$(NC)"
	@echo

# Build targets
build: configure-standard
	@echo -e "$(BLUE)=== Setup Complete! ===$(NC)"
	@echo -e "$(GREEN)✓ Autotools configured$(NC)"
	@if [ -f "$(HOOK_SOURCE)" ]; then \
		echo -e "$(GREEN)✓ Code formatting enabled$(NC)"; \
	else \
		echo -e "$(YELLOW)⚠ Code formatting skipped$(NC)"; \
	fi
	@echo
	@echo -e "$(GREEN)Ready to build! 🚀$(NC)"
	@echo -e "$(BLUE)Building (standard mode)...$(NC)"
	@echo "✅ Bootstrap complete - project ready to build!"
	@echo "🔄 Restarting make to use generated Makefile..."
	@$(MAKE) -f Makefile all

build-dev: configure-dev
	@echo -e "$(BLUE)=== Setup Complete! ===$(NC)"
	@echo -e "$(GREEN)✓ Autotools configured$(NC)"
	@echo -e "$(GREEN)✓ Code formatting enabled$(NC)"
	@echo
	@echo -e "$(GREEN)Ready to build! 🚀$(NC)"
	@echo -e "$(BLUE)Building (development mode)...$(NC)"
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
		echo -e "$(BLUE)=== Cleaning Project ===$(NC)"; \
		echo -e "$(YELLOW)Removing compiled files and temporary artifacts...$(NC)"; \
		if $(MAKE) -f Makefile clean >/dev/null 2>&1; then \
			echo -e "$(GREEN)✓ Compiled files removed$(NC)"; \
			echo -e "$(YELLOW)Removing patch files...$(NC)"; \
			find . -name "*.orig" -delete 2>/dev/null || true; \
			find . -name "*.rej" -delete 2>/dev/null || true; \
			echo -e "$(GREEN)✓ Clean completed successfully$(NC)"; \
		else \
			echo -e "$(RED)✗ Clean failed$(NC)"; \
			exit 1; \
		fi; \
	else \
		has_artifacts=0; \
		if [ -f tests/test_unit_g15 ] || [ -f tests/test_integration_g15 ] || [ -f tests/mock_g15 ] || ls tests/*.o >/dev/null 2>&1 || ls tests/*.log >/dev/null 2>&1 || ls tests/*.trs >/dev/null 2>&1; then \
			has_artifacts=1; \
		fi; \
		has_patches=$$(find . -name "*.orig" -o -name "*.rej" | head -1); \
		if [ $$has_artifacts -eq 1 ] || [ -n "$$has_patches" ]; then \
			echo -e "$(BLUE)=== Cleaning Bootstrap Mode ===$(NC)"; \
			if [ $$has_artifacts -eq 1 ]; then \
				echo -e "$(YELLOW)Cleaning test artifacts...$(NC)"; \
				rm -f tests/test_unit_g15 tests/test_integration_g15 tests/mock_g15 tests/*.o tests/*.log tests/*.trs tests/test-suite.log; \
				echo -e "$(GREEN)✓ Test artifacts cleaned$(NC)"; \
			fi; \
			if [ -n "$$has_patches" ]; then \
				echo -e "$(YELLOW)Removing patch files...$(NC)"; \
				find . -name "*.orig" -delete 2>/dev/null || true; \
				find . -name "*.rej" -delete 2>/dev/null || true; \
				echo -e "$(GREEN)✓ Patch files removed$(NC)"; \
			fi; \
			echo -e "$(GREEN)✓ Clean completed$(NC)"; \
		else \
			echo -e "$(GREEN)✓ Nothing to clean$(NC)"; \
		fi; \
	fi


# Help target  
help:
	@echo "LCDproc G15 - Available Make Targets"
	@echo "===================================="
	@echo ""
	@echo -e "$(BLUE)🔨 Build Commands (with automatic setup):$(NC)"
	@echo "  make                    - Complete setup + standard build"
	@echo "  make dev                - Complete setup + development build (with git hooks)"
	@echo ""
	@echo -e "$(BLUE)🚀 Quick Start (Development Workflow):$(NC)"
	@echo "  make check              - Basic tests (~3s) - Daily development"
	@echo "  make test-full          - Comprehensive (~60s) - Before commits"
	@echo "  make test-coverage      - Code coverage analysis - Quality assurance"
	@echo ""
	@echo -e "$(BLUE)🔍 Specific Test Categories:$(NC)"
	@echo "  make test-g15           - Test only G15 devices (no RGB)"
	@echo "  make test-g510          - Test only G510 devices (with RGB)"
	@echo "  make test-verbose       - Run tests with detailed output"
	@echo "  make test-scenarios     - All 4 test scenarios:"
	@echo "                            • Device detection (USB IDs)"
	@echo "                            • RGB color testing (HID + LED)"
	@echo "                            • Macro system (G-keys, M1/M2/M3)"
	@echo "                            • Error handling (device failures)"
	@echo ""
	@echo -e "$(BLUE)🎯 Individual Test Scenarios:$(NC)"
	@echo "  make test-scenario-detection - Only device detection tests"
	@echo "  make test-scenario-rgb       - Only RGB functionality tests"  
	@echo "  make test-scenario-macros    - Only macro system tests"
	@echo "  make test-scenario-failures  - Only error handling tests"
	@echo ""
	@echo -e "$(BLUE)🚀 Advanced Testing:$(NC)"
	@echo "  make test-ci            - Complete CI/CD test suite"
	@echo "                            Includes: test-full + multi-compiler testing"
	@echo "  make test-integration   - End-to-end integration tests"
	@echo "                            Includes: LCDd server + client + mock hardware"
	@echo "  make test-server        - Test only LCDd server functionality"
	@echo "  make test-clients       - Test only client functionality"
	@echo "  make test-e2e           - Full end-to-end workflow testing"
	@echo ""
	@echo -e "$(BLUE)⚠️  Advanced (Standalone - For Debugging Only):$(NC)"
	@echo "  make test-memcheck      - Memory leak detection with valgrind"
	@echo "  make test-coverage      - Code coverage analysis with gcovr"
	@echo "                            Generates: *.gcov coverage.xml coverage.html files"
	@echo "  make test-compilers     - Multi-compiler build testing"
	@echo "                            DESTRUCTIVE: Cleans + rebuilds with clang & gcc"
	@echo ""
	@echo -e "$(BLUE)🔧 Code Quality & Formatting (requires 'make dev'):$(NC)"
	@echo "  make format             - Format all C code with clang-format + prettier"
	@echo "  make format-check       - Check if formatting is needed (non-destructive)"
	@echo "  make lint               - Run clang-tidy static analysis"
	@echo "  make lint-fix           - Run clang-tidy with automatic fixes"
	@echo ""
	@echo -e "$(BLUE)🔧 Manual Setup (advanced users):$(NC)"
	@echo "  make setup-autotools    - Run autotools setup only"
	@echo "  make setup-hooks-install- Install git pre-commit hooks"
	@echo "  make setup-hooks-remove - Remove git hooks"  
	@echo "  make setup-hooks-status - Show git hooks status"
	@echo ""
	@echo -e "$(BLUE)💡 Usage Guidelines:$(NC)"
	@echo "  • Daily: make check (fast feedback)"
	@echo "  • Pre-commit: make test-full (thorough validation)"
	@echo "  • Coverage: make test-coverage (analyze test coverage)"
	@echo "  • Debugging: make test-scenarios (isolate issues)"
	@echo ""
	@echo -e "$(YELLOW)ℹ️  Project not configured yet. Run 'make' or 'make dev' to begin setup.$(NC)"

# Development-only test features - require make dev
test-verbose test-g15 test-g510 test-scenarios test-scenario-detection test-scenario-rgb test-scenario-macros test-scenario-failures test-memcheck test-coverage test-compilers test-full test-ci test-integration test-mock test-server test-clients test-e2e:
	@if [ ! -f Makefile ]; then \
		echo -e "$(RED)❌ Project not configured yet$(NC)"; \
		echo -e "$(YELLOW)💡 Run 'make dev' first to set up development environment$(NC)"; \
		exit 1; \
	elif [ ! -f config.h ] || ! grep -q "^#define DEBUG" config.h 2>/dev/null; then \
		echo -e "$(RED)❌ Development build required for advanced test features$(NC)"; \
		echo -e "$(YELLOW)💡 Run 'make distclean && make dev' to enable development features$(NC)"; \
		echo -e "$(YELLOW)💡 Or use standard 'make check' for basic testing$(NC)"; \
		exit 1; \
	elif [ ! -f tests/Makefile ]; then \
		echo -e "$(RED)❌ Test suite not configured$(NC)"; \
		echo -e "$(YELLOW)💡 Development build required - run 'make distclean && make dev'$(NC)"; \
		exit 1; \
	else \
		$(MAKE) -C tests $@; \
	fi

# Development-only code quality features - require make dev
format format-check lint lint-fix:
	@if [ ! -f Makefile ]; then \
		echo -e "$(RED)❌ Project not configured yet$(NC)"; \
		echo -e "$(YELLOW)💡 Run 'make dev' first to set up development environment$(NC)"; \
		exit 1; \
	elif [ ! -f config.h ] || ! grep -q "^#define DEBUG" config.h 2>/dev/null; then \
		echo -e "$(RED)❌ Development build required for code quality features$(NC)"; \
		echo -e "$(YELLOW)💡 Run 'make distclean && make dev' to enable development features$(NC)"; \
		exit 1; \
	else \
		$(MAKE) -f Makefile $@; \
	fi

# Standard autotools check target - works with both standard and dev builds
check:
	@if [ -f Makefile ]; then \
		$(MAKE) -f Makefile check; \
	else \
		echo -e "$(RED)❌ Project not set up yet. Run 'make' or 'make dev' first.$(NC)"; \
		exit 1; \
	fi

# Delegate all other targets to Makefile if it exists, otherwise to bootstrap
%:
	@if [ -f Makefile ]; then \
		$(MAKE) -f Makefile $@; \
	else \
		echo -e "$(RED)❌ Project not set up yet. Run 'make' or 'make dev' first.$(NC)"; \
		exit 1; \
	fi