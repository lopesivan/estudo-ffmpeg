# Makefile: compila main.c usando FFmpeg (libavutil, libavformat, libswscale, libavcodec)

PKG_CONFIG_PATH ?= /usr/lib/x86_64-linux-gnu/pkgconfig
PKGS     := libavutil libavformat libswscale libavcodec
CC       ?= cc
CFLAGS   += -Wall -Wextra -O2 -DVERBOSE $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --cflags $(PKGS))
LDLIBS   := $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --libs $(PKGS))

TARGET := main
SRC    := main.c

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $< $(LDLIBS)

clean:
	rm -f $(TARGET) $(TARGET).o
# END OF FILE
