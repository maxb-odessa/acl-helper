# ===========================================================================
#      http://www.gnu.org/software/autoconf-archive/ax_lib_memcached.html
# ===========================================================================
#
# SYNOPSIS
#
#   AX_LIB_MEMCACHED([MINIMUM-VERSION])
#
# DESCRIPTION
#
#   Test for the LIBMEMCACHED library of a particular version (or newer)
#
#   This macro takes only one optional argument, required version of LIBMEMECACHED
#   library. If required version is not passed, 1.0.0 is used in the test
#   of existance of LIBMEMCACHED
#
#   If no intallation prefix to the installed LIBMEMECACHED library is given the
#   macro searches under /usr, /usr/local, and /opt.
#
#   This macro calls:
#
#     AC_SUBST(MEMCACHED_CFLAGS)
#     AC_SUBST(MEMCACHED_LDFLAGS)
#     AC_SUBST(MEMCACHED_VERSION)
#
#   And sets:
#
#     HAVE_MEMCACHED
#
# LICENSE
#
#   Copyright (c) 2008 Mateusz Loskot <mateusz@loskot.net>
#
#   Copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved. This file is offered as-is, without any
#   warranty.


AC_DEFUN([AX_LIB_MEMCACHED],
[
    AC_ARG_WITH([memcached],
        AS_HELP_STRING(
            [--with-memcached=@<:@ARG@:>@],
            [use memcached library @<:@default=yes@:>@, optionally specify the prefix for memcached library]
        ),
        [
        if test "$withval" = "no"; then
            WANT_MEMCACHED="no"
        elif test "$withval" = "yes"; then
            WANT_MEMCACHED="yes"
            ac_memcached_path=""
        else
            WANT_MEMCACHED="yes"
            ac_memcached_path="$withval"
        fi
        ],
        [WANT_MEMCACHED="yes"]
    )

    MEMCACHED_CFLAGS=""
    MEMCACHED_LDFLAGS=""
    MEMCACHED_VERSION=""

    if test "x$WANT_MEMCACHED" = "xyes"; then

        ac_memcached_header="libmemcached/memcached.h"

        memcached_version_req=ifelse([$1], [], [1.0.0], [$1])
        memcached_version_req_shorten=`expr $memcached_version_req : '\([[0-9]]*\.[[0-9]]*\)'`
        memcached_version_req_major=`expr $memcached_version_req : '\([[0-9]]*\)'`
        memcached_version_req_minor=`expr $memcached_version_req : '[[0-9]]*\.\([[0-9]]*\)'`
        memcached_version_req_micro=`expr $memcached_version_req : '[[0-9]]*\.[[0-9]]*\.\([[0-9]]*\)'`
        if test "x$memcached_version_req_micro" = "x" ; then
            memcached_version_req_micro="0"
        fi

        memcached_version_req_number=`expr $memcached_version_req_major \* 10000 \
                                   \+ $memcached_version_req_minor \* 100 \
                                   \+ $memcached_version_req_micro`

        AC_MSG_CHECKING([for memcached library >= $memcached_version_req])

        if test "$ac_memcached_path" != ""; then
            ac_memcached_ldflags="-L$ac_memcached_path/lib"
            ac_memcached_cppflags="-I$ac_memcached_path/include"
        else
            for ac_memcached_path_tmp in /usr /usr/local /opt ; do
                if test -f "$ac_memcached_path_tmp/include/$ac_memcached_header" \
                    && test -r "$ac_memcached_path_tmp/include/$ac_memcached_header"; then
                    ac_memcached_path=$ac_memcached_path_tmp
                    ac_memcached_cppflags="-I$ac_memcached_path_tmp/include"
                    ac_memcached_ldflags="-L$ac_memcached_path_tmp/lib"
                    break;
                fi
            done
        fi

        ac_memcached_ldflags="$ac_memcached_ldflags -lmemcached"

        saved_CPPFLAGS="$CPPFLAGS"
        CPPFLAGS="$CPPFLAGS $ac_memcached_cppflags"

        AC_LANG_PUSH(C)
        AC_COMPILE_IFELSE(
            [
            AC_LANG_PROGRAM([[@%:@include <libmemcached/memcached.h>]],
                [[
#if (LIBMEMCACHED_VERSION_HEX >= $memcached_version_req_number)
/* Everything is okay */
#else
#  error libmemcached version is too old
#endif
                ]]
            )
            ],
            [
            AC_MSG_RESULT([yes])
            success="yes"
            ],
            [
            AC_MSG_RESULT([not found])
            success="no"
            ]
        )
        AC_LANG_POP(C)

        CPPFLAGS="$saved_CPPFLAGS"

        if test "$success" = "yes"; then

            MEMCACHED_CFLAGS="$ac_memcached_cppflags"
            MEMCACHED_LDFLAGS="$ac_memcached_ldflags"

            ac_memcached_header_path="$ac_memcached_path/include/$ac_memcached_header"

            AC_SUBST(MEMCACHED_CFLAGS)
            AC_SUBST(MEMCACHED_LDFLAGS)
            AC_DEFINE([HAVE_MEMCACHED], [1], [Have the MEMCACHED library])
        fi
    fi
])
