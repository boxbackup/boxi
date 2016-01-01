
## Created by Anjuta - will be overwritten
## If you don't want it to overwrite it,
## 	Please disable it in the Anjuta project configuration

# We are using *external* gettext, via AM_GNU_GETTEXT([external]), so there
# should be NO intl directory and we should NOT try to compile in it, and
# intl should NOT appear in the list of SUBDIRS below:
SUBDIRS = boxbackup src po

boxidocdir = ${prefix}/doc/boxi
boxidoc_DATA = \
	README.md\
	COPYING\
	AUTHORS\
	ChangeLog\
	INSTALL\
	TODO

# DO NOT commit gettextize's change to add config.rpath to EXTRA_DIST below.
# Silly gettextize.
EXTRA_DIST = infrastructure/automake/config.rpath infrastructure/m4/ChangeLog $(boxidoc_DATA)

# Copy all the spec files. Of cource, only one is actually used.
dist-hook:
	for specfile in *.spec; do \
		if test -f $$specfile; then \
			cp -p $$specfile $(distdir); \
		fi \
	done

ACLOCAL_AMFLAGS = --install
