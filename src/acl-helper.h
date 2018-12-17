/** \file */

#ifndef __AC_HELPER_H__
#define __AC_HELPER_H__

#ifdef HAVE_CONFIG_H
  #include "autoconf.h"
#endif

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <ctype.h>
#include <syslog.h>
#include <pwd.h>
#include <grp.h>
#include <unistd.h>
#include <stdarg.h>
#include <time.h>
#include <signal.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifdef HAVE_STRINGS_H
  #include <strings.h>
#endif

#ifdef HAVE_SYS_SYSCALL_H
  #include <sys/syscall.h>
#endif

#ifdef HAVE_PTHREAD
  #include <pthread.h>
#endif

#ifdef HAVE_FNMATCH
  #define USE_MATCH 1
#endif

#ifdef HAVE_REGEX_H
  #define USE_REGEX 1
#endif

#ifdef HAVE_LIBPCRE
  #define USE_PCRE 1
#endif

#ifdef HAVE_NETDB_H
  #define USE_RESOLVE 1
#endif

#ifdef HAVE_OPENSSL
  #define USE_SSL 1
#endif

#ifdef HAVE_SQLITE3
  #define USE_SQLITE3 1
#endif

#ifdef HAVE_PGSQL
  #define USE_PGSQL 1
#endif

#ifdef HAVE_GEOIP2
  #define USE_GEOIP2 1
#endif

#ifdef HAVE_MEMCACHED
  #undef USE_MEMCACHED  /* not yet implemented */
#endif



#ifndef PATH_MAX
  #define PATH_MAX    4096
#endif

#ifndef HAVE_SIGHANDLER_T
  #define sighandler_t sig_t
#endif


//! full package version
#define ACLH_VERSION PACKAGE_NAME \
                     ", version: " VERSION " (rev." ACLH_SVN_REVISION ")" \
                     "\nBuild date: " __DATE__ ", " __TIME__ "\n" \
                     ACLH_EXT_VERSION \
                     "\nEmail: <" PACKAGE_BUGREPORT ">" \
                     "\nUrl: " PACKAGE_URL

//! default config file location
#define DEFAULT_CONFIG_FILE "./acl-helper.conf"

//! comment char for config and source files
#define COMMENT_CHAR      '#'

//! common config data structure
extern struct config config;


#endif //__AC_HELPER_H__

