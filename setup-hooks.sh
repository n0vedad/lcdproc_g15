#!/bin/bash
# Setup or remove git hooks for lcdproc-g15
#
# Copyright (C) 2025 n0vedad <https://github.com/n0vedad/>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# any later version.

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

HOOK_SOURCE=".git/hooks/pre-commit"
HOOK_TEMPLATE="hooks-pre-commit.template"

usage() {
    echo "Usage: $0 [install|remove|status]"
    echo "  install  - Install pre-commit formatting hook"
    echo "  remove   - Remove pre-commit hook"
    echo "  status   - Show hook status"
    exit 1
}

install_hook() {
    if [ -f "$HOOK_SOURCE" ] && [ -x "$HOOK_SOURCE" ]; then
        echo -e "${GREEN}✓ Pre-commit hook already installed and executable${NC}"
    elif [ -f "$HOOK_SOURCE" ]; then
        chmod +x "$HOOK_SOURCE"
        echo -e "${GREEN}✓ Pre-commit hook made executable${NC}"
    elif [ -f "$HOOK_TEMPLATE" ]; then
        cp "$HOOK_TEMPLATE" "$HOOK_SOURCE"
        chmod +x "$HOOK_SOURCE"
        echo ""
        echo -e "${GREEN}✓ Pre-commit hook installed from template${NC}"
    else
        echo -e "${RED}✗ Hook template not found at $HOOK_TEMPLATE${NC}"
        echo -e "${RED}Cannot install git hook without template.${NC}"
        exit 1
    fi

    echo -e "${GREEN}Pre-commit hook installed successfully!${NC}"
    echo -e "${BLUE}It will automatically check code formatting before commits.${NC}"
}

remove_hook() {
    if [ -f "$HOOK_SOURCE" ]; then
        rm "$HOOK_SOURCE"
        echo -e "${GREEN}✓ Pre-commit hook removed${NC}"
    else
        echo -e "${YELLOW}✓ No pre-commit hook to remove${NC}"
    fi
}

status_hook() {
    if [ -f "$HOOK_SOURCE" ] && [ -x "$HOOK_SOURCE" ]; then
        echo -e "${GREEN}✓ Pre-commit hook: ACTIVE${NC}"
        echo -e "  Location: $HOOK_SOURCE"
        echo -e "  Executable: YES"
    elif [ -f "$HOOK_SOURCE" ]; then
        echo -e "${YELLOW}⚠ Pre-commit hook: EXISTS but NOT EXECUTABLE${NC}"
        echo -e "  Run: $0 install"
    elif [ -f "$HOOK_TEMPLATE" ]; then
        echo -e "${YELLOW}✗ Pre-commit hook: NOT INSTALLED (template available)${NC}"
        echo -e "  Run: $0 install"
    else
        echo -e "${RED}✗ Pre-commit hook: NOT INSTALLED (no template)${NC}"
        echo -e "  Template missing: $HOOK_TEMPLATE"
    fi
}

case "${1:-}" in
    "install")
        install_hook
        ;;
    "remove")
        remove_hook
        ;;
    "status")
        status_hook
        ;;
    "")
        usage
        ;;
    *)
        usage
        ;;
esac
