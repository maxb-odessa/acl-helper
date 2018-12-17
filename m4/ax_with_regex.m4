
AC_DEFUN([AX_WITH_REGEX],[dnl


AC_ARG_ENABLE([regex],
    AS_HELP_STRING([--enable-regex], [eisable posix regex support]))

AC_MSG_CHECKING([for enabled regex support])

AS_IF([test "x$enable_regex" != "xno"], [
  AC_MSG_RESULT([yes])
  AC_CHECK_HEADERS([regex.h])
  AC_CHECK_FUNCS([regcomp])
])

AS_IF([test "x$enable_regex" == "xno"], [
  AC_MSG_RESULT([no])
])

])

