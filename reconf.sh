#! /bin/bash
#
#####################################

aclocal && \
autoheader && \
automake -a -c && \
autoconf && \
./configure \
     --without-memcached 

