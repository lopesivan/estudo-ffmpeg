# configure.ac

# Process this file with autoconf to produce a configure script.
AC_INIT([main], [0.02], [ivan@ivanlopes.eng.br],
        [main], [http://ivanlopes.eng.br/])

# Force autoconf to be at least this version number:
AC_PREREQ([2.68])

# Put autotools auxiliary files in a subdir, so they don't clutter top dir:
AC_CONFIG_AUX_DIR([build-aux])

# The modern autotools convention is that you should use an “m4” subdirectory
# for internal “m4” files, instead of the default aclocal.m4.
#AC_CONFIG_MACRO_DIR([m4])

# Enable "automake" to simplify creating makefiles:
AM_INIT_AUTOMAKE([1.11 -Wall -Werror])

# Checks for programs.
AC_PROG_CC
AC_PROG_CC_C_O

PKG_PROG_PKG_CONFIG
#CFLAGS = `pkg-config --cflags libavutil libavformat libswscale libavcodec`
#LDLIBS = `pkg-config --libs libavutil libavformat libswscale libavcodec`
PKG_CHECK_MODULES(FFMPEG, [libavutil libavformat libswscale libavcodec])

AC_SUBST(FFMPEG_CFLAGS)
AC_SUBST(FFMPEG_LIBS)

# tools
AC_PROG_LN_S    # faz LN_S="ln -s"
AC_PROG_SED     # faz SED=/bin/sed
AC_PROG_MKDIR_P # faz MKDIR_P="mkdir -p"

AC_CONFIG_FILES([
    Makefile
    src/Makefile
])
AC_OUTPUT

