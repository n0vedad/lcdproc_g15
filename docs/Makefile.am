## Process this file with automake to produce Makefile.in

# Doxygen documentation generation

EXTRA_DIST = Doxyfile.in doxy-mainpage.md DoxygenLayout.xml

# Generated documentation directory (excluded from dist)
CLEANFILES =
CLEAN_DIRS = html latex

# Note: Doxyfile generation handled automatically by Autoconf via AC_CONFIG_FILES
# No manual rule needed - configure.ac already defines: AC_CONFIG_FILES([docs/Doxyfile])

# Generate HTML documentation
html-local: Doxyfile
	@if command -v doxygen >/dev/null 2>&1; then \
		echo "Generating Doxygen documentation..."; \
		doxygen Doxyfile; \
		echo "Documentation generated in docs/html/index.html"; \
	else \
		echo ""; \
		echo "❌ Doxygen not found!"; \
		echo ""; \
		echo "📦 Install Doxygen:"; \
		echo "   Arch Linux:  sudo pacman -S doxygen graphviz"; \
		echo "   Ubuntu:      sudo apt install doxygen graphviz"; \
		echo "   macOS:       brew install doxygen graphviz"; \
		echo ""; \
		exit 1; \
	fi

# Clean generated documentation
clean-local:
	-rm -rf html latex
	-rm -f Doxyfile

# Install documentation to docdir
install-data-local: html-local
	$(MKDIR_P) $(DESTDIR)$(docdir)/html
	cp -r html/* $(DESTDIR)$(docdir)/html/

# Uninstall documentation
uninstall-local:
	-rm -rf $(DESTDIR)$(docdir)/html

.PHONY: html-local

## EOF
