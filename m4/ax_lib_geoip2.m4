#
# SYNOPSIS
#
#   AX_LIB_GEOIP2()
#
# This macro calls:
#
#     AC_SUBST(GEOIP2_CFLAGS)
#     AC_SUBST(GEOIP2_LDFLAGS)
#     AC_SUBST(GEOIP2_LIBS)
#
# And sets:
#
#     HAVE_GEOIP2
#

AC_DEFUN([AX_LIB_GEOIP2],
[
    AC_ARG_WITH([geoip2],
        AS_HELP_STRING(
            [--with-geoip2=@<:@ARG@:>@],
            [use GeoIP2 library @<:@default=yes@:>@, optionally specify the prefix for geoip2 library]
        ),
        [
        if test "$withval" = "no"; then
            WANT_GEOIP2="no"
        elif test "$withval" = "yes"; then
            WANT_GEOIP2="yes"
            ac_geoip2_path=""
        else
            WANT_GEOIP2="yes"
            ac_geoip2_path="$withval"
        fi
        ],
        [WANT_GEOIP2="yes"]
    )

    GEOIP2_CFLAGS=""
    GEOIP2_LDFLAGS=""
    GEOIP2_LIBS=""


    if test "x$WANT_GEOIP2" = "xyes"; then
    
        ac_geoip2_header="maxminddb.h"

        AC_MSG_CHECKING([for GeoIP2 (libmaxminddb) library])

        if test "$ac_geoip2_path" != ""; then
            ac_geoip2_ldflags="-L$ac_geoip2_path/lib"
            ac_geoip2_cppflags="-I$ac_geoip2_path/include"
        else
            for ac_geoip2_path_tmp in /usr /usr/local /opt ; do
                if test -f "$ac_geoip2_path_tmp/include/$ac_geoip2_header" \
                    && test -r "$ac_geoip2_path_tmp/include/$ac_geoip2_header"; then
                    ac_geoip2_path=$ac_geoip2_path_tmp
                    ac_geoip2_cppflags="-I$ac_geoip2_path_tmp/include"
                    ac_geoip2_ldflags="-L$ac_geoip2_path_tmp/lib"
                    break;
                fi
            done
        fi


        if test "$ac_geoip2_path" != ""; then
            ac_geoip2_ldflags="-L$ac_geoip2_path/lib"
            ac_geoip2_cppflags="-I$ac_geoip2_path/include"
        else
            for ac_geoip2_path_tmp in /usr /usr/local /opt ; do
                if test -f "$ac_geoip2_path_tmp/include/$ac_geoip2_header" \
                    && test -r "$ac_geoip2_path_tmp/include/$ac_geoip2_header"; then
                    ac_geoip2_path=$ac_geoip2_path_tmp
                    ac_geoip2_cppflags="-I$ac_geoip2_path_tmp/include"
                    ac_geoip2_ldflags="-L$ac_geoip2_path_tmp/lib"
                    break;
                fi
            done
        fi

        ac_geoip2_ldflags="$ac_geoip2_ldflags -lmaxminddb"

        saved_CPPFLAGS="$CPPFLAGS"
        CPPFLAGS="$CPPFLAGS $ac_geoip2_cppflags"

        AC_LANG_PUSH(C)
        AC_COMPILE_IFELSE(
            [
            AC_LANG_PROGRAM([[@%:@include <$ac_geoip2_header>]],
                [[
                  void main(void) { (void*)MMDB_lib_version(); };
                ]]
            )
            ],
            [
            AC_MSG_RESULT([yes])
            success="yes"
            ],
            [
            AC_MSG_RESULT([not found, but you may get it here: https://github.com/maxmind/libmaxminddb])
            success="no"
            ]
        )
        AC_LANG_POP(C)

        CPPFLAGS="$saved_CPPFLAGS"

        if test "$success" = "yes"; then

            GEOIP2_CFLAGS="$ac_geoip2_cppflags"
            GEOIP2_LDFLAGS="$ac_geoip2_ldflags"

            ac_geoip2_header_path="$ac_geoip2_path/include/$ac_geoip2_header"

            AC_SUBST(GEOIP2_CFLAGS)
            AC_SUBST(GEOIP2_LDFLAGS)
            AC_SUBST(GEOIP2_LIBS)
            AC_DEFINE([HAVE_GEOIP2], [1], [Have the GEOIP2 library])
        fi
    fi
])
