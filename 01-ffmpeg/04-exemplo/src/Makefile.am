bin_PROGRAMS = videoffmpeg
videoffmpeg_SOURCES = main.c
videoffmpeg_CFLAGS  = ${FFMPEG_CFLAGS}
videoffmpeg_LDADD   = ${FFMPEG_LIBS}

install-exec-local:
	$(MKDIR_P) $(DESTDIR)/$(bindir)
	cd $(DESTDIR)/$(bindir) && \
			$(LN_S) videoffmpeg command_ffmpeg_1 && \
			$(LN_S) videoffmpeg command_ffmpeg_2
