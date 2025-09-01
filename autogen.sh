#!/bin/sh
# Run this to generate all the initial makefiles, etc.
#
# Original autotools setup from LCDproc project
# Copyright (C) 2025 n0vedad <https://github.com/n0vedad/> [Interactive formatting setup]

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Set default values for variables
: ${AUTOCONF:=autoconf}
: ${AUTOHEADER:=autoheader}
: ${AUTOMAKE:=automake}
: ${ACLOCAL:=aclocal}

DIE=0

($AUTOCONF --version) < /dev/null > /dev/null 2>&1 || {
    echo
    echo -e "${RED}**Error**: You must have \`autoconf' installed.${NC}"
    echo "Download the appropriate package for your distribution,"
    echo "or get the source tarball at ftp://ftp.gnu.org/pub/gnu/"
    DIE=1
}

($AUTOMAKE --version) < /dev/null > /dev/null 2>&1 || {
    echo
    echo -e "${RED}**Error**: You must have \`automake' installed.${NC}"
    echo "Get ftp://ftp.gnu.org/pub/gnu/automake-1.3.tar.gz"
    echo "(or a newer version if it is available)"
    DIE=1
    NO_AUTOMAKE=yes
}

# If no automake, don't bother testing for aclocal
test -n "$NO_AUTOMAKE" || ($ACLOCAL --version) < /dev/null > /dev/null 2>&1 || {
    echo
    echo -e "${RED}**Error**: Missing \`aclocal'.  The version of \`automake'${NC}"
    echo "installed doesn't appear recent enough."
    echo "Get ftp://ftp.gnu.org/pub/gnu/automake-1.3.tar.gz"
    echo "(or a newer version if it is available)"
    DIE=1
}

if test "$DIE" -eq 1; then
    echo -e "${RED}✗ Missing required autotools. Please install them first.${NC}"
    exit 1
fi

echo -e "${BLUE}=== Autotools Setup ===${NC}"

echo -e "${YELLOW}Running aclocal ...${NC}"
$ACLOCAL && echo -e "${GREEN}✓ aclocal completed${NC}" || {
    echo -e "${RED}✗ aclocal failed${NC}"
    exit 1
}

if grep "^A[CM]_CONFIG_HEADER" configure.ac > /dev/null; then
    echo -e "${YELLOW}Running autoheader...${NC}"
    $AUTOHEADER && echo -e "${GREEN}✓ autoheader completed${NC}" || {
        echo -e "${RED}✗ autoheader failed${NC}"
        exit 1
    }
fi

echo -e "${YELLOW}Running automake ...${NC}"
$AUTOMAKE --add-missing --copy && echo -e "${GREEN}✓ automake completed${NC}" || {
    echo -e "${RED}✗ automake failed${NC}"
    exit 1
}

echo -e "${YELLOW}Running autoconf ...${NC}"
$AUTOCONF && echo -e "${GREEN}✓ autoconf completed${NC}" || {
    echo -e "${RED}✗ autoconf failed${NC}"
    exit 1
}

echo -e "${GREEN}✓ Autotools setup complete!${NC}"
echo ""

# Interactive setup for code formatting
setup_formatting() {
    echo -e "${BLUE}=== Code Formatting Setup ===${NC}"
    echo "This project uses automatic code formatting for consistency."
    echo "Dependencies: clang-format (C code), prettier (other files)"
    echo ""

    # Check if already set up
    if [ -f ".git/hooks/pre-commit" ] && command -v clang-format > /dev/null 2>&1 && command -v npx > /dev/null 2>&1; then
        echo -e "${GREEN}✓ Code formatting already configured${NC}"
        return 0
    fi

    # Check if running in non-interactive mode (like makepkg)
    if [ ! -t 0 ] || [ -n "$PKGBUILD_MODE" ]; then
        echo -e "${YELLOW}⚠ Non-interactive mode detected - skipping formatting setup${NC}"
        echo "Code formatting can be enabled manually with: ./setup-hooks.sh install"
        return 0
    fi

    printf "Enable automatic code formatting? [Y/n]: "
    read -r response
    case "$response" in
        [nN] | [nN][oO])
            echo -e "${YELLOW}⚠ Skipping code formatting setup${NC}"
            echo "You can enable it later by running: ./setup-hooks.sh install"
            return 0
            ;;
        *)
            echo -e "${YELLOW}Setting up code formatting...${NC}"
            ;;
    esac

    # Check and install dependencies
    missing_deps=""

    if ! command -v clang-format > /dev/null 2>&1; then
        missing_deps="$missing_deps clang-format"
    fi

    if ! command -v npx > /dev/null 2>&1; then
        missing_deps="$missing_deps npm"
    fi

    if [ -n "$missing_deps" ]; then
        echo -e "${RED}✗ Missing dependencies:$missing_deps${NC}"
        echo "Please install them first:"
        if echo "$missing_deps" | grep -q "clang-format"; then
            echo "  sudo pacman -S clang"
        fi
        if echo "$missing_deps" | grep -q "npm"; then
            echo "  sudo pacman -S npm"
        fi

        printf "Continue without formatting setup? [y/N]: "
        read -r continue_response
        case "$continue_response" in
            [yY] | [yY][eE][sS])
                echo -e "${YELLOW}⚠ Continuing without formatting setup${NC}"
                return 0
                ;;
            *)
                echo -e "${RED}Aborting. Please install dependencies and re-run.${NC}"
                exit 1
                ;;
        esac
    fi

    # Install npm dependencies
    if command -v npm > /dev/null 2>&1; then
        if [ ! -f "package-lock.json" ] || [ ! -d "node_modules" ]; then
            echo -e "${YELLOW}Installing prettier dependencies...${NC}"
            npm install || {
                echo -e "${RED}✗ Failed to install npm dependencies${NC}"
                return 1
            }
        fi
    fi

    # Install git hooks
    if [ -f "./setup-hooks.sh" ]; then
        ./setup-hooks.sh install
    else
        echo -e "${RED}✗ setup-hooks.sh not found${NC}"
        return 1
    fi

    echo -e "${GREEN}✓ Code formatting setup complete!${NC}"
    echo "Files will be automatically formatted before commits."
    echo "Manual formatting: make format"
}

# Run formatting setup
setup_formatting

# Final summary
echo ""
echo -e "${BLUE}=== Setup Complete! ===${NC}"
echo -e "${GREEN}✓ Autotools configured${NC}"
if [ -f ".git/hooks/pre-commit" ]; then
    echo -e "${GREEN}✓ Code formatting enabled${NC}"
else
    echo -e "${YELLOW}⚠ Code formatting skipped${NC}"
fi
echo ""
echo -e "${BLUE}Next steps:${NC}"
echo "  ./configure --prefix=/usr --sbindir=/usr/bin --sysconfdir=/etc --enable-libusb --enable-lcdproc-menus --enable-stat-smbfs --enable-drivers=g15,linux_input"
echo "  make"
echo ""
echo -e "${GREEN}Ready to build! 🚀${NC}"
