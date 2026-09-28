# ****************************************************************************
# *                                                                          *
# *  frames.mak                                                                *
# *  ==========                                                                *
# *                                                                          *
# ****************************************************************************

# ****************************************************************************
# *                                                                          *
# *  Define the name of the makefile.                                        *
# *                                                                          *
# ****************************************************************************

MAKNAM = frames.mak

# ****************************************************************************
# *                                                                          *
# *  Define the directories in which to search for library files.            *
# *                                                                          *
# ****************************************************************************

LIBDRS =

# ****************************************************************************
# *                                                                          *
# *  Define the directories in which to search for include files.            *
# *                                                                          *
# ****************************************************************************

INCDRS =

# ****************************************************************************
# *                                                                          *
# *  Define the library files.                                               *
# *                                                                          *
# ****************************************************************************

LIBFLS =

# ****************************************************************************
# *                                                                          *
# *  Define the source files.                                                *
# *                                                                          *
# ****************************************************************************

SRCFLS = video_frames.c

# ****************************************************************************
# *                                                                          *
# *  Video que será processado.                                              *
# *                                                                          *
# ****************************************************************************

VIDEO = ./arquivo.mp4
FFPROBE = ffprobe
FFPROBE_OPT = -v error \
			  -count_frames \
			  -select_streams v:0 \
			  -show_entries stream=nb_frames \
			  -of default=nw=1:nk=1

# ****************************************************************************
# *                                                                          *
# *  Define the object files.                                                *
# *                                                                          *
# ****************************************************************************

OBJFLS = video_frames.o

# ****************************************************************************
# *                                                                          *
# *  Define the executable.                                                  *
# *                                                                          *
# ****************************************************************************

EXE    = video_frames.exe

# ****************************************************************************
# *                                                                          *
# *  lib FFMPEG                                                              *
# *                                                                          *
# ****************************************************************************

PKG_CONFIG_PATH:=/usr/lib/x86_64-linux-gnu/pkgconfig
export PKG_CONFIG_PATH

FFMPEG_LIBS   = libavformat libavcodec libavutil libswscale
FFMPEG_CFLAGS = $(shell /usr/bin/pkg-config --cflags $(FFMPEG_LIBS))
FFMPEG_LDLIBS = $(shell /usr/bin/pkg-config --libs $(FFMPEG_LIBS))

# ****************************************************************************
# *                                                                          *
# *  VIDEO OPTIONS                                                           *
# *                                                                          *
# ****************************************************************************

NFRAMES = 10

# ****************************************************************************
# *                                                                          *
# *  biblioteca de fonte                                                     *
# *                                                                          *
# ****************************************************************************

FREETYPE2_LIBS   = freetype2
FREETYPE2_CFLAGS = $(shell /usr/bin/pkg-config --cflags $(FREETYPE2_LIBS))
FREETYPE2_LDLIBS = $(shell /usr/bin/pkg-config --libs $(FREETYPE2_LIBS))

# ****************************************************************************
# *                                                                          *
# *  Define the compile and link options.                                    *
# *                                                                          *
# ****************************************************************************

CC     = gcc
LL     = gcc
CFLAGS = -Wall -Wextra -std=c99 $(FFMPEG_CFLAGS)
LFLAGS = $(FFMPEG_LDLIBS)

# ****************************************************************************
# *                                                                          *
# *  Define the rules.                                                       *
# *                                                                          *
# ****************************************************************************

$(EXE): $(OBJFLS)
	$(LL) -o $@ $(OBJFLS) $(LIBDRS) $(LIBFLS) $(LFLAGS)

.c.o:
	$(CC) $(CFLAGS) -o $@ -c $(INCDRS) $<

all:
	make -f $(MAKNAM) clean
	make -f $(MAKNAM) depend

depend:
	makedepend $(INCDRS) -f $(MAKNAM) $(SRCFLS)
	make -f $(MAKNAM) $(EXE)

run: $(EXE)
	./$(EXE) $(VIDEO) $(NFRAMES)

free: $(EXE)
	valgrind --leak-check=full ./$(EXE) $(VIDEO) $(NFRAMES)

frames:
	$(FFPROBE) $(FFPROBE_OPT) $(VIDEO)

clean:
	-rm $(EXE)
	-rm $(OBJFLS)

# DO NOT DELETE THIS LINE -- make depend depends on it.
