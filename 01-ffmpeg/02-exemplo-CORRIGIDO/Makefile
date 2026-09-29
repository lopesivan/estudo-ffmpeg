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
	rm -f $(TARGET) $(TARGET).o video.mp4 video.avi

run:
	cp ../01-exemplo/video.mp4 .
	./main video.mp4 video.avi

test:
	ffmpeg -i video.mp4 2>&1 | grep -oP 'Duration: \K[0-9:.]+'
	ffmpeg -i video.avi 2>&1 | grep -oP 'Duration: \K[0-9:.]+'
	ffmpeg -i video.mp4 2>&1 | grep -oP 'Duration: \K[0-9:.]+' | awk -F: '{print ($$1*3600)+($$2*60)+$$3}'
	ffmpeg -i video.avi 2>&1 | grep -oP 'Duration: \K[0-9:.]+' | awk -F: '{print ($$1*3600)+($$2*60)+$$3}'

show: video.avi
	ffplay video.avi

# END OF FILE
