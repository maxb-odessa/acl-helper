#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([acl-helper], [0.0.3b1], [vesemir-odessa@sf.net], [], [http://sf.net/p/acl-helper])
AM_INIT_AUTOMAKE([subdir-objects])
AC_CONFIG_SRCDIR([src/acl-helper.h])
AC_CONFIG_HEADER([autoconf.h])
AC_CONFIG_MACRO_DIR([m4])
ACLOCAL_AMFLAGS="$ACLOCAL_AMFLAGS -I m4"
AC_CANONICAL_HOST
AC_LANG([C])

# Checks for programs.
CFLAGS=
CXXFLAGS=
LDFLAGS=
AC_PROG_CC
AC_PROG_CPP
AC_PROG_CC_STDC
AC_PROG_CC_C_O
AM_PROG_CC_C_O
AX_PTHREAD
AC_PROG_INSTALL
DX_INIT_DOXYGEN($PACKAGE_NAME, [Doxyfile], [docs])

# Checks for libraries.
AX_LIB_PGSQL([9.0.0])
AX_LIB_SQLITE3([3.7.7])
AX_LIB_MEMCACHED([1.0.0])
AX_LIB_GEOIP2()
AX_CHECK_OPENSSL([AC_DEFINE([HAVE_OPENSSL], [1], [Have the OPENSSL library])])
AX_PATH_LIB_PCRE
AX_WITH_REGEX

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([sys/syscall.h netdb.h strings.h sys/auxv.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_FUNC([getauxval])
AC_CHECK_TYPES([sighandler_t], [], [], [[#include <signal.h>]])

# Checks for library functions.
AC_FUNC_FNMATCH
#AC_CHECK_LIB([pq], [PQconnectdb])
#AC_CHECK_LIB([sqlite3], [sqlite3_open])
#AC_CHECK_LIB([memcached], [memcached])
#AC_CHECK_LIB([pcre], [pcre_compile])

# use debug mode?
AC_ARG_ENABLE(debug,
AS_HELP_STRING([--disable-debug],
               [disable debug build, default: no]),
[case "${enableval}" in
             no) disable_debug=true ;;
            yes) disable_debug=false ;;
              *) AC_MSG_ERROR([bad value ${enableval} for --disable-debug]) ;;
esac],
[disable_debug=false])

if test x"$disable_debug" = x"false"; then
  AC_MSG_NOTICE([Building DEBUG version])
else
  AC_MSG_NOTICE([Building PRODUCTION version])
fi
AM_CONDITIONAL(DEBUG, test x"$disable_debug" = x"false")

# generate package extended version
ext_version=`echo "Build host: $(uname -omnri)"`
AC_DEFINE_UNQUOTED(ACLH_EXT_VERSION, ["$ext_version"], [extended package version])
svn_revision=`(svnversion || echo '(none)') 2>/dev/null`
AC_DEFINE_UNQUOTED(ACLH_SVN_REVISION, ["$svn_revision"], [svn revision])

# generate files
AC_OUTPUT([Makefile])

