
ACLOCAL_AMFLAGS = -I m4

bin_PROGRAMS = acl-helper

acl_helper_SOURCES = src/options.c \
                     src/options.h \
                     src/url.h \
                     src/url.c \
                     src/tree.h \
                     src/tree.c \
                     src/source.c \
                     src/source.h \
                     src/loop.c \
                     src/loop.h \
                     src/log.c \
                     src/log.h \
                     src/checker.c \
                     src/checker.h \
                     src/misc.c \
                     src/misc.h \
                     src/conf.c \
                     src/conf.h \
                     src/resolve.c \
                     src/resolve.h \
                     src/ssl.c \
                     src/ssl.h \
                     src/geoip2.c \
                     src/geoip2.h \
                     src/acl-helper.c \
                     src/acl-helper.h



acl_helper_CFLAGS = $(PTHREAD_CFLAGS) \
                    $(PGSQL_CFLAGS) \
                    $(SQLITE3_CFLAGS) \
                    $(MEMCACHED_CFLAGS) \
                    $(PCRE_CFLAGS) \
                    $(OPENSSL_INCLUDES) \
                    $(GEOIP2_INCLUDES)

acl_helper_LDFLAGS = $(PTHREAD_LDFLAGS) \
                     $(PGSQL_LDFLAGS) \
                     $(SQLITE3_LDFLAGS) \
                     $(MEMCACHED_LDFLAGS) \
                     $(PCRE_LDFLAGS) \
                     $(OPENSSL_LDFLAGS) \
                     $(GEOIP2_LDFLAGS)

acl_helper_LDADD = $(PTHREAD_LDFLAGS) \
                   $(PGSQL_LDFLAGS) \
                   $(SQLITE3_LDFLAGS) \
                   $(MEMCACHED_LDFLAGS) \
                   $(PCRE_LDFLAGS) \
                   $(OPENSSL_LIBS) \
                   $(GEOIP2_LDFLAGS)


if DEBUG

AM_CPPFLAGS = -D_GNU_SOURCE -DDEBUG -Wall
acl_helper_CFLAGS += -pg -ggdb -Wall
acl_helper_LDFLAGS += -pg -ggdb
acl_helper_LDADD +=

else

AM_CPPFLAGS = -D_GNU_SOURCE

acl_helper_CFLAGS += -O3 -finline -Wno-unused-result
acl_helper_LDFLAGS += -O3 -s -finline -Wno-unused-result
acl_helper_LDADD += 

endif


#install-data-local:
#	do something like cp, chmod, etc...


all-local: docs

clean-local:
	-rm -rf $(top_builddir)/doc/html/* $(top_builddir)/doc/man/* gmon.out core.*

docs:
	$(DX_DOXYGEN) $(top_srcdir)/Doxyfile

EXTRA_DIST = doc etc tests Doxyfile m4 

