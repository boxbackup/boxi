
## Created by Anjuta - will be overwritten
## If you don't want it to overwrite it,
## 	Please disable it in the Anjuta project configuration

SUBDIRS = intl boxbackup src po

boxidocdir = ${prefix}/doc/boxi
boxidoc_DATA = \
	README\
	COPYING\
	AUTHORS\
	ChangeLog\
	INSTALL\
	NEWS\
	TODO

# DO NOT commit gettextize's change to add config.rpath to EXTRA_DIST below.
# Silly gettextize.
EXTRA_DIST = $(boxidoc_DATA)

# Copy all the spec files. Of cource, only one is actually used.
dist-hook:
	for specfile in *.spec; do \
		if test -f $$specfile; then \
			cp -p $$specfile $(distdir); \
		fi \
	done

ACLOCAL_AMFLAGS = -I m4 --install
