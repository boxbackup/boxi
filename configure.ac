dnl Process this file with autoconf to produce a configure script.
dnl Created by Anjuta application wizard.

AC_INIT(Boxi, 0.1.1, https://github.com/boxbackup/boxi)
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

dnl AC_CANONICAL_SYSTEM #deprecated
AC_CANONICAL_BUILD

AC_CONFIG_MACRO_DIR([infrastructure/m4])
AC_CONFIG_AUX_DIR([infrastructure/automake])

#AM_CONFIG_HEADER([include/BoxiConfig.h]) # no such file
AM_INIT_AUTOMAKE
AM_MAINTAINER_MODE

if test -z "$CFLAGS"; then
	export CFLAGS="-g -O0"
fi

if test -z "$CXXFLAGS"; then
	export CXXFLAGS="-g -O0"
fi

AC_ISC_POSIX
AC_PROG_CC
AM_PROG_CC_STDC
AC_HEADER_STDC

WXCONFIG=wx-config
AC_ARG_WITH(wx-config,
[[  --with-wx-config=FILE
    Use the given path to wx-config when determining
    wxWidgets configuration; defaults to "wx-config"]],
[
    if test "$withval" != "yes" -a "$withval" != ""; then
        WXCONFIG=$withval
    fi
])
AC_MSG_CHECKING([wxWidgets version])
if wxversion=`$WXCONFIG --version`; then
    AC_MSG_RESULT([$wxversion])
else
    AC_MSG_RESULT([not found])
    AC_MSG_ERROR([wxWidgets is required. Try --with-wx-config.])
fi

WX_CPPFLAGS="`$WXCONFIG --cppflags`"
WX_CXXFLAGS="`$WXCONFIG --cxxflags`"
WX_LIBS="`$WXCONFIG --libs`"

AC_SUBST(WX_CPPFLAGS)
AC_SUBST(WX_CXXFLAGS)
AC_SUBST(WX_LIBS)

AC_PROG_CPP
AC_PROG_CXX

AM_PATH_CPPUNIT(1.10.0,,[AC_MSG_ERROR([[CppUnit 1.10 or higher is required]])])
CXXFLAGS="$CXXFLAGS $CPPUNIT_CFLAGS"
LIBS="$LIBS $CPPUNIT_LIBS"

dnl ***************************************************************************
dnl i18n
dnl ***************************************************************************
GETTEXT_PACKAGE=boxi
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [GETTEXT package name])
AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.17])
dnl AM_GNU_GETTEXT([external])
dnl AM_GLIB_GNU_GETTEXT
dnl IT_PROG_INTLTOOL([0.35.0])
AM_PROG_LIBTOOL

dnl Custom macros begin here

m4_include([boxbackup/infrastructure/m4/boxbackup_tests.m4])

AM_CONDITIONAL([WINDOWS], [test "$WINDOWS" = "true"])

if test "$WINDOWS" = "true"; then
        LT_PROG_RC
fi

# extract boxbackup in release build
if ! (
	set -e
	cd boxbackup
	./bootstrap # PATCH IN RELEASE
); then
	echo "Failed to configure Box Backup, aborting!"
	exit 1
fi

AC_CONFIG_SUBDIRS(boxbackup)
AC_CONFIG_SUBDIRS(wxchart)

dnl Custom macros end here

PKG_CHECK_MODULES(All, cppunit)

# We are using *external* gettext, via AM_GNU_GETTEXT([external]), so there
# should be NO intl directory and we should NOT try to compile in it, and no
# intl/* files should appear in the AC_OUTPUT list below:
AC_OUTPUT([
	po/Makefile.in
	Makefile
	src/Makefile
])
