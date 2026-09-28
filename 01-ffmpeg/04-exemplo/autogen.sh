##!/usr/bin/env sh
#
#aclocal && autoconf && automake --add-missing
#
touch NEWS README AUTHORS ChangeLog COPYING
autoreconf -i -v
#
#exit 0
