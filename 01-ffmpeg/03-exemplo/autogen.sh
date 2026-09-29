##!/usr/bin/env sh
#
#aclocal && autoconf && automake --add-missing
#
touch NEWS README AUTHORS ChangeLog COPYING

autoreconf -i -v &&
    PKG_CONFIG_PATH=/usr/lib/x86_64-linux-gnu/pkgconfig ./configure &&
    make

exit 0
