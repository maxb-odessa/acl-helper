# ===========================================================================
#
# SYNOPSIS
#
#   AX_LIB_PGSQL([MINIMUM-VERSION])
#
# DESCRIPTION
#
#   Test for the PostgreSQL 3 library of a particular version (or newer)
#   If no intallation prefix to the installed PostgreSQL library is given the
#   macro searches under /usr, /usr/local, and /opt.
#
#   This macro calls:
#
#     AC_SUBST(PGSQL_CFLAGS)
#     AC_SUBST(PGSQL_LDFLAGS)
#
#   And sets:
#
#     HAVE_PGSQL
#
# LICENSE
#
#   Copyright (c) 2008 Mateusz Loskot <mateusz@loskot.net>
#
#   Copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved. This file is offered as-is, without any
#   warranty.



AC_DEFUN([AX_LIB_PGSQL],
[
    AC_ARG_WITH([pgsql],
        AS_HELP_STRING(
            [--with-pgsql=@<:@ARG@:>@],
            [use PostgreSQL library @<:@default=yes@:>@, optionally specify the prefix for pgsql library]
        ),
        [
        if test "$withval" = "no"; then
            WANT_PGSQL="no"
        elif test "$withval" = "yes"; then
            WANT_PGSQL="yes"
            ac_pgsql_path=""
        else
            WANT_PGSQL="yes"
            ac_pgsql_path="$withval"
        fi
        ],
        [WANT_PGSQL="yes"]
    )

    PGSQL_CFLAGS=""
    PGSQL_LDFLAGS=""
    PGSQL_VERSION=""

    if test "x$WANT_PGSQL" = "xyes"; then

        ac_pgsql_header="pg_config.h"

        pgsql_version_req=ifelse([$1], [], [9.0.0], [$1])
        pgsql_version_req_shorten=`expr $pgsql_version_req : '\([[0-9]]*\.[[0-9]]*\)'`
        pgsql_version_req_major=`expr $pgsql_version_req : '\([[0-9]]*\)'`
        pgsql_version_req_minor=`expr $pgsql_version_req : '[[0-9]]*\.\([[0-9]]*\)'`
        pgsql_version_req_micro=`expr $pgsql_version_req : '[[0-9]]*\.[[0-9]]*\.\([[0-9]]*\)'`
        if test "x$pgsql_version_req_micro" = "x" ; then
            pgsql_version_req_micro="0"
        fi

        pgsql_version_req_number=`expr $pgsql_version_req_major \* 10000 \
                                   \+ $pgsql_version_req_minor \* 100 \
                                   \+ $pgsql_version_req_micro`

        AC_MSG_CHECKING([for PostgreSQL library >= $pgsql_version_req])

        if test "$ac_pgsql_path" != ""; then
            ac_pgsql_ldflags="-L$ac_pgsql_path/lib"
            ac_pgsql_cppflags="-I$ac_pgsql_path/include"
        else
            for ac_pgsql_path_tmp in /usr /usr/local /opt; do
                if test -f "$ac_pgsql_path_tmp/include/$ac_pgsql_header" \
                    && test -r "$ac_pgsql_path_tmp/include/$ac_pgsql_header"; then
                    ac_pgsql_path=$ac_pgsql_path_tmp
                    ac_pgsql_cppflags="-I$ac_pgsql_path_tmp/include"
                    ac_pgsql_ldflags="-L$ac_pgsql_path_tmp/lib"
                    break;
                fi
               if test -f "$ac_pgsql_path_tmp/include/postgresql/$ac_pgsql_header" \
                    && test -r "$ac_pgsql_path_tmp/include/postgresql/$ac_pgsql_header"; then
                    ac_pgsql_path=$ac_pgsql_path_tmp
                    ac_pgsql_cppflags="-I$ac_pgsql_path_tmp/include/postgresql"
                    ac_pgsql_ldflags="-L$ac_pgsql_path_tmp/lib"
                    break;
                fi
            done
        fi

        ac_pgsql_ldflags="$ac_pgsql_ldflags -lpq"

        saved_CPPFLAGS="$CPPFLAGS"
        CPPFLAGS="$CPPFLAGS $ac_pgsql_cppflags"

        AC_LANG_PUSH(C)
        AC_COMPILE_IFELSE(
            [
            AC_LANG_PROGRAM([[@%:@include <pg_config.h>]],
                [[
#if (PG_VERSION_NUM >= $pgsql_version_req_number)
/* Everything is okay */
#else
#  error PostgreSQL version is too old
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
            PGSQL_CFLAGS="$ac_pgsql_cppflags"
            PGSQL_LDFLAGS="$ac_pgsql_ldflags"
            AC_SUBST(PGSQL_CFLAGS)
            AC_SUBST(PGSQL_LDFLAGS)
            AC_DEFINE([HAVE_PGSQL], [1], [Have the PGSQL library])
        fi
    fi
])
